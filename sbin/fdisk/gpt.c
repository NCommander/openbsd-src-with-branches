/*	$OpenBSD: gpt.c,v 1.59 2022/03/11 22:29:55 krw Exp $	*/
/*
 * Copyright (c) 2015 Markus Muller <mmu@grummel.net>
 * Copyright (c) 2015 Kenneth R Westerback <krw@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>	/* DEV_BSIZE */
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/ioctl.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uuid.h>

#include "part.h"
#include "disk.h"
#include "mbr.h"
#include "misc.h"
#include "gpt.h"

#ifdef DEBUG
#define DPRINTF(x...)	printf(x)
#else
#define DPRINTF(x...)
#endif

struct mbr		gmbr;
struct gpt_header	gh;
struct gpt_partition	gp[NGPTPARTITIONS];

struct gpt_partition	**sort_gpt(void);
int			  lba_start_cmp(const void *e1, const void *e2);
int			  lba_free(uint64_t *, uint64_t *);
int			  add_partition(const uint8_t *, const char *, uint64_t);
int			  find_partition(const uint8_t *);
int			  get_header(const uint64_t);
int			  get_partition_table(void);
int			  init_gh(void);
int			  init_gp(const int);
uint32_t		  crc32(const u_char *, const uint32_t);
int			  protective_mbr(const struct mbr *);
int			  gpt_chk_mbr(struct dos_partition *, uint64_t);

/*
 * Return the index into dp[] of the EFI GPT (0xEE) partition, or -1 if no such
 * partition exists.
 *
 * Taken from kern/subr_disk.c.
 *
 */
int
gpt_chk_mbr(struct dos_partition *dp, u_int64_t dsize)
{
	struct dos_partition	*dp2;
	int			 efi, eficnt, found, i;
	uint32_t		 psize;

	found = efi = eficnt = 0;
	for (dp2 = dp, i = 0; i < NDOSPART; i++, dp2++) {
		if (dp2->dp_typ == DOSPTYP_UNUSED)
			continue;
		found++;
		if (dp2->dp_typ != DOSPTYP_EFI)
			continue;
		if (letoh32(dp2->dp_start) != GPTSECTOR)
			continue;
		psize = letoh32(dp2->dp_size);
		if (psize <= (dsize - GPTSECTOR) || psize == UINT32_MAX) {
			efi = i;
			eficnt++;
		}
	}
	if (found == 1 && eficnt == 1)
		return efi;

	return -1;
}

int
protective_mbr(const struct mbr *mbr)
{
	struct dos_partition	dp[NDOSPART], dos_partition;
	int			i;

	if (mbr->mbr_lba_self != 0)
		return -1;

	for (i = 0; i < NDOSPART; i++) {
		PRT_make(&mbr->mbr_prt[i], mbr->mbr_lba_self,
		    mbr->mbr_lba_firstembr, &dos_partition);
		memcpy(&dp[i], &dos_partition, sizeof(dp[i]));
	}

	return gpt_chk_mbr(dp, DL_GETDSIZE(&dl));
}

int
get_header(const uint64_t sector)
{
	char			*secbuf;
	uint64_t		 partlastlba, partslen, lba_end;
	int			 partspersec;
	uint32_t		 gh_csum;

	secbuf = DISK_readsectors(sector, 1);
	if (secbuf == NULL)
		return -1;

	memcpy(&gh, secbuf, sizeof(struct gpt_header));
	free(secbuf);

	if (letoh64(gh.gh_sig) != GPTSIGNATURE) {
		DPRINTF("gpt signature: expected 0x%llx, got 0x%llx\n",
		    GPTSIGNATURE, letoh64(gh.gh_sig));
		return -1;
	}

	if (letoh32(gh.gh_rev) != GPTREVISION) {
		DPRINTF("gpt revision: expected 0x%x, got 0x%x\n",
		    GPTREVISION, letoh32(gh.gh_rev));
		return -1;
	}

	if (letoh64(gh.gh_lba_self) != sector) {
		DPRINTF("gpt self lba: expected %llu, got %llu\n",
		    sector, letoh64(gh.gh_lba_self));
		return -1;
	}

	if (letoh32(gh.gh_size) != GPTMINHDRSIZE) {
		DPRINTF("gpt header size: expected %u, got %u\n",
		    GPTMINHDRSIZE, letoh32(gh.gh_size));
		return -1;
	}

	if (letoh32(gh.gh_part_size) != GPTMINPARTSIZE) {
		DPRINTF("gpt partition size: expected %u, got %u\n",
		    GPTMINPARTSIZE, letoh32(gh.gh_part_size));
		return -1;
	}

	if (letoh32(gh.gh_part_num) > NGPTPARTITIONS) {
		DPRINTF("gpt partition count: expected <= %u, got %u\n",
		    NGPTPARTITIONS, letoh32(gh.gh_part_num));
		return -1;
	}

	gh_csum = gh.gh_csum;
	gh.gh_csum = 0;
	gh.gh_csum = htole32(crc32((unsigned char *)&gh, letoh32(gh.gh_size)));
	if (gh_csum != gh.gh_csum) {
		DPRINTF("gpt header checksum: expected 0x%x, got 0x%x\n",
		    letoh32(gh.gh_csum), letoh32(gh_csum));
		/* Accept wrong-endian checksum. */
		if (swap32(gh_csum) != gh.gh_csum)
			return -1;
	}

	/* XXX Assume part_num * part_size is multiple of secsize. */
	partslen = letoh32(gh.gh_part_num) * letoh32(gh.gh_part_size) /
	    dl.d_secsize;
	lba_end = DL_GETDSIZE(&dl) - partslen - 2;
	if (letoh64(gh.gh_lba_end) > lba_end) {
		DPRINTF("gpt last usable LBA: reduced from %llu to %llu\n",
		    letoh64(gh.gh_lba_end), lba_end);
		gh.gh_lba_end = htole64(lba_end);
	}

	if (letoh64(gh.gh_lba_start) >= letoh64(gh.gh_lba_end)) {
		DPRINTF("gpt first usable LBA: expected < %llu, got %llu\n",
		    letoh64(gh.gh_lba_end), letoh64(gh.gh_lba_start));
		return -1;
	}

	if (letoh64(gh.gh_part_lba) <= letoh64(gh.gh_lba_end) &&
	    letoh64(gh.gh_part_lba) >= letoh64(gh.gh_lba_start)) {
		DPRINTF("gpt partition table start lba: expected < %llu or "
		    "> %llu, got %llu\n", letoh64(gh.gh_lba_start),
		    letoh64(gh.gh_lba_end), letoh64(gh.gh_part_lba));
		return -1;
	}

	partspersec = dl.d_secsize / letoh32(gh.gh_part_size);
	partlastlba = letoh64(gh.gh_part_lba) +
	    ((letoh32(gh.gh_part_num) + partspersec - 1) / partspersec) - 1;
	if (partlastlba <= letoh64(gh.gh_lba_end) &&
	    partlastlba >= letoh64(gh.gh_lba_start)) {
		DPRINTF("gpt partition table last LBA: expected < %llu or "
		    "> %llu, got %llu\n", letoh64(gh.gh_lba_start),
		    letoh64(gh.gh_lba_end), partlastlba);
		return -1;
	}

	/*
	 * Other possible paranoia checks:
	 *	1) partition table starts before primary gpt lba.
	 *	2) partition table extends into lowest partition.
	 *	3) alt partition table starts before gh_lba_end.
	 */
	return 0;
}

int
get_partition_table(void)
{
	char			*secbuf;
	uint64_t		 gpbytes, gpsectors;
	uint32_t		 gh_part_csum, partspersec;

	DPRINTF("gpt partition table being read from LBA %llu\n",
	    letoh64(gh.gh_part_lba));

	partspersec = dl.d_secsize / letoh32(gh.gh_part_size);
	if (partspersec * letoh32(gh.gh_part_size) != dl.d_secsize) {
		DPRINTF("gpt partition table entry invalid size. %u\n",
		    letoh32(gh.gh_part_size));
		return -1;
	}
	gpbytes = letoh32(gh.gh_part_num) * letoh32(gh.gh_part_size);
	gpsectors = gpbytes / dl.d_secsize;
	memset(&gp, 0, sizeof(gp));

	secbuf = DISK_readsectors(letoh64(gh.gh_part_lba), gpsectors);
	if (secbuf == NULL)
		return -1;

	memcpy(&gp, secbuf, gpbytes);
	free(secbuf);

	gh_part_csum = gh.gh_part_csum;
	gh.gh_part_csum = htole32(crc32((unsigned char *)&gp, gpbytes));
	if (gh_part_csum != gh.gh_part_csum) {
		DPRINTF("gpt partition table checksum: expected 0x%x, "
		    "got 0x%x\n", letoh32(gh.gh_part_csum),
		    letoh32(gh_part_csum));
		/* Accept wrong-endian checksum. */
		if (swap32(gh_part_csum) != gh.gh_part_csum)
			return -1;
	}

	return 0;
}

int
GPT_read(const int which)
{
	int			error;

	error = MBR_read(0, 0, &gmbr);
	if (error == 0)
		error = protective_mbr(&gmbr);
	if (error)
		goto done;

	switch (which) {
	case PRIMARYGPT:
		error = get_header(GPTSECTOR);
		break;
	case SECONDARYGPT:
		error = get_header(DL_GETDSIZE(&dl) - 1);
		break;
	case ANYGPT:
		error = get_header(GPTSECTOR);
		if (error != 0 || get_partition_table() != 0)
			error = get_header(DL_GETDSIZE(&dl) - 1);
		break;
	default:
		return -1;
	}

	if (error == 0)
		error = get_partition_table();

 done:
	if (error != 0) {
		/* No valid GPT found. Zap any artifacts. */
		memset(&gmbr, 0, sizeof(gmbr));
		memset(&gh, 0, sizeof(gh));
		memset(&gp, 0, sizeof(gp));
	}

	return error;
}

void
GPT_print(const char *units, const int verbosity)
{
	const struct unit_type	*ut;
	struct uuid		 guid;
	const int		 secsize = dl.d_secsize;
	char			*guidstr = NULL;
	double			 size;
	int			 i, status;

#ifdef	DEBUG
	char			*p;

	p = (char *)&gh.gh_sig;

	printf("gh_sig         : ");
	for (i = 0; i < sizeof(gh.gh_sig); i++)
		printf("%c", isprint((unsigned char)p[i]) ? p[i] : '?');
	printf(" (");
	for (i = 0; i < sizeof(gh.gh_sig); i++) {
		printf("%02x", p[i]);
		if ((i + 1) < sizeof(gh.gh_sig))
			printf(":");
	}
	printf(")\n");
	printf("gh_rev         : %u\n", letoh32(gh.gh_rev));
	printf("gh_size        : %u (%zd)\n", letoh32(gh.gh_size), sizeof(gh));
	printf("gh_csum        : 0x%x\n", letoh32(gh.gh_csum));
	printf("gh_rsvd        : %u\n", letoh32(gh.gh_rsvd));
	printf("gh_lba_self    : %llu\n", letoh64(gh.gh_lba_self));
	printf("gh_lba_alt     : %llu\n", letoh64(gh.gh_lba_alt));
	printf("gh_lba_start   : %llu\n", letoh64(gh.gh_lba_start));
	printf("gh_lba_end     : %llu\n", letoh64(gh.gh_lba_end));
	p = NULL;
	uuid_to_string(&gh.gh_guid, &p, &status);
	printf("gh_gh_guid     : %s\n", (status == uuid_s_ok) ? p : "<invalid>");
	free(p);
	printf("gh_gh_part_lba : %llu\n", letoh64(gh.gh_part_lba));
	printf("gh_gh_part_num : %u (%zu)\n", letoh32(gh.gh_part_num),
	    nitems(gp));
	printf("gh_gh_part_size: %u (%zu)\n", letoh32(gh.gh_part_size),
	    sizeof(gp[0]));
	printf("gh_gh_part_csum: 0x%x\n", letoh32(gh.gh_part_csum));
	printf("\n");
#endif	/* DEBUG */

	size = units_size(units, DL_GETDSIZE(&dl), &ut);
	printf("Disk: %s       Usable LBA: %llu to %llu [%.0f ",
	    disk.dk_name, letoh64(gh.gh_lba_start), letoh64(gh.gh_lba_end),
	    size);
	if (ut->ut_conversion == 0 && secsize != DEV_BSIZE)
		printf("%d-byte ", secsize);
	printf("%s]\n", ut->ut_lname);

	if (verbosity == VERBOSE) {
		printf("GUID: ");
		uuid_dec_le(&gh.gh_guid, &guid);
		uuid_to_string(&guid, &guidstr, &status);
		if (status == uuid_s_ok)
			printf("%s\n", guidstr);
		else
			printf("<invalid header GUID>\n");
		free(guidstr);
	}

	GPT_print_parthdr(verbosity);
	for (i = 0; i < letoh32(gh.gh_part_num); i++) {
		if (uuid_is_nil(&gp[i].gp_type, NULL))
			continue;
		GPT_print_part(i, units, verbosity);
	}
}

void
GPT_print_parthdr(const int verbosity)
{
	printf("   #: type                                "
	    " [       start:         size ]\n");
	if (verbosity == VERBOSE)
		printf("      guid                                 name\n");
	printf("--------------------------------------------------------"
	    "----------------\n");
}

void
GPT_print_part(const int n, const char *units, const int verbosity)
{
	const struct unit_type	*ut;
	struct uuid		 guid;
	struct gpt_partition	*partn = &gp[n];
	char			*guidstr = NULL;
	double			 size;
	uint64_t		 sectors;
	int			 status;

	uuid_dec_le(&partn->gp_type, &guid);
	sectors = letoh64(partn->gp_lba_end) - letoh64(partn->gp_lba_start) + 1;
	size = units_size(units, sectors, &ut);
	printf("%c%3d: %-36s [%12lld: %12.0f%s]\n",
	    (letoh64(partn->gp_attrs) & GPTDOSACTIVE)?'*':' ', n,
	    PRT_uuid_to_typename(&guid), letoh64(partn->gp_lba_start),
	    size, ut->ut_abbr);

	if (verbosity == VERBOSE) {
		uuid_dec_le(&partn->gp_guid, &guid);
		uuid_to_string(&guid, &guidstr, &status);
		if (status != uuid_s_ok)
			printf("      <invalid partition guid>             ");
		else
			printf("      %-36s ", guidstr);
		printf("%-36s\n", utf16le_to_string(partn->gp_name));
		free(guidstr);
	}
}

int
find_partition(const uint8_t *beuuid)
{
	struct uuid		uuid, gp_type;
	unsigned int		pn, pncnt;

	uuid_dec_be(beuuid, &uuid);
	uuid_enc_le(&gp_type, &uuid);

	pncnt = letoh32(gh.gh_part_num);
	for (pn = 0; pn < pncnt; pn++) {
		if (uuid_compare(&gp[pn].gp_type, &gp_type, NULL) == 0)
			return pn;
	}
	return -1;
}

int
add_partition(const uint8_t *beuuid, const char *name, uint64_t sectors)
{
	struct uuid		uuid, gp_type;
	int			rslt;
	uint64_t		end, freesectors, start;
	uint32_t		status, pn, pncnt;

	uuid_dec_be(beuuid, &uuid);
	uuid_enc_le(&gp_type, &uuid);

	pncnt = letoh32(gh.gh_part_num);
	for (pn = 0; pn < pncnt; pn++) {
		if (uuid_is_nil(&gp[pn].gp_type, NULL))
			break;
	}
	if (pn == pncnt)
		goto done;

	rslt = lba_free(&start, &end);
	if (rslt == -1)
		goto done;

	if (start % BLOCKALIGNMENT)
		start += (BLOCKALIGNMENT - start % BLOCKALIGNMENT);
	if (start >= end)
		goto done;

	freesectors = end - start + 1;

	if (sectors == 0)
		sectors = freesectors;

	if (freesectors < sectors)
		goto done;
	else if (freesectors > sectors)
		end = start + sectors - 1;

	gp[pn].gp_type = gp_type;
	gp[pn].gp_lba_start = htole64(start);
	gp[pn].gp_lba_end = htole64(end);
	memcpy(gp[pn].gp_name, string_to_utf16le(name),
	    sizeof(gp[pn].gp_name));

	uuid_create(&uuid, &status);
	if (status != uuid_s_ok)
		goto done;

	uuid_enc_le(&gp[pn].gp_guid, &uuid);
	gh.gh_part_csum = htole32(crc32((unsigned char *)&gp, sizeof(gp)));
	gh.gh_csum = 0;
	gh.gh_csum = htole32(crc32((unsigned char *)&gh, sizeof(gh)));

	return 0;

 done:
	if (pn != pncnt)
		memset(&gp[pn], 0, sizeof(gp[pn]));
	printf("unable to add %s\n", name);
	return -1;
}

int
init_gh(void)
{
	struct gpt_header	oldgh;
	struct uuid		guid;
	const int		secsize = dl.d_secsize;
	int			needed;
	uint32_t		status;

	memcpy(&oldgh, &gh, sizeof(oldgh));
	memset(&gh, 0, sizeof(gh));
	memset(&gmbr, 0, sizeof(gmbr));

	/* XXX Do we need the boot code? UEFI spec & Apple says no. */
	memcpy(gmbr.mbr_code, default_dmbr.dmbr_boot, sizeof(gmbr.mbr_code));
	gmbr.mbr_prt[0].prt_id = DOSPTYP_EFI;
	gmbr.mbr_prt[0].prt_bs = 1;
	gmbr.mbr_prt[0].prt_ns = UINT32_MAX;
	gmbr.mbr_signature = DOSMBR_SIGNATURE;

	needed = sizeof(gp) / secsize + 2;

	if (needed % BLOCKALIGNMENT)
		needed += (needed - (needed % BLOCKALIGNMENT));

	gh.gh_sig = htole64(GPTSIGNATURE);
	gh.gh_rev = htole32(GPTREVISION);
	gh.gh_size = htole32(GPTMINHDRSIZE);
	gh.gh_csum = 0;
	gh.gh_rsvd = 0;
	gh.gh_lba_self = htole64(1);
	gh.gh_lba_alt = htole64(DL_GETDSIZE(&dl) - 1);
	gh.gh_lba_start = htole64(needed);
	gh.gh_lba_end = htole64(DL_GETDSIZE(&dl) - needed);
	gh.gh_part_lba = htole64(2);
	gh.gh_part_num = htole32(NGPTPARTITIONS);
	gh.gh_part_size = htole32(GPTMINPARTSIZE);

	uuid_create(&guid, &status);
	if (status != uuid_s_ok) {
		memcpy(&gh, &oldgh, sizeof(gh));
		return -1;
	}

	uuid_enc_le(&gh.gh_guid, &guid);
	return 0;
}

int
init_gp(const int how)
{
	struct gpt_partition	oldgp[NGPTPARTITIONS];
	const uint8_t		gpt_uuid_efi_system[] = GPT_UUID_EFI_SYSTEM;
	const uint8_t		gpt_uuid_openbsd[] = GPT_UUID_OPENBSD;
	uint64_t		prt_ns;
	int			pn, rslt;

	memcpy(&oldgp, &gp, sizeof(oldgp));
	if (how == GHANDGP)
		memset(&gp, 0, sizeof(gp));
	else {
		for (pn = 0; pn < letoh32(gh.gh_part_num); pn++) {
			if (PRT_protected_guid(&gp[pn].gp_type))
				continue;
			memset(&gp[pn], 0, sizeof(gp[pn]));
		}
	}

	rslt = 0;
	if (disk.dk_bootprt.prt_ns > 0) {
		pn = find_partition(gpt_uuid_efi_system);
		if (pn == -1) {
			rslt = add_partition(gpt_uuid_efi_system,
			    "EFI System Area", disk.dk_bootprt.prt_ns);
		} else {
			prt_ns = gp[pn].gp_lba_end - gp[pn].gp_lba_start + 1;
			if (prt_ns < disk.dk_bootprt.prt_ns) {
				printf("EFI System Area < %llu sectors\n",
				    disk.dk_bootprt.prt_ns);
				rslt = -1;
			}
		}
	}
	if (rslt == 0)
		rslt = add_partition(gpt_uuid_openbsd, "OpenBSD Area", 0);

	if (rslt != 0)
		memcpy(&gp, &oldgp, sizeof(gp));

	return rslt;
}

int
GPT_init(const int how)
{
	int			rslt = 0;

	if (how == GHANDGP)
		rslt = init_gh();
	if (rslt == 0)
		rslt = init_gp(how);

	return rslt;
}

void
GPT_zap_headers(void)
{
	char			*secbuf;
	uint64_t		 sig;

	secbuf = DISK_readsectors(GPTSECTOR, 1);
	if (secbuf == NULL)
		return;

	memcpy(&sig, secbuf, sizeof(sig));
	if (letoh64(sig) == GPTSIGNATURE) {
		memset(secbuf, 0, dl.d_secsize);
		if (DISK_writesectors(secbuf, GPTSECTOR, 1))
			DPRINTF("Unable to zap GPT header @ sector %d",
			    GPTSECTOR);
	}
	free(secbuf);

	secbuf = DISK_readsectors(DL_GETDSIZE(&dl) - 1, 1);
	if (secbuf == NULL)
		return;

	memcpy(&sig, secbuf, sizeof(sig));
	if (letoh64(sig) == GPTSIGNATURE) {
		memset(secbuf, 0, dl.d_secsize);
		if (DISK_writesectors(secbuf, DL_GETDSIZE(&dl) - 1, 1))
			DPRINTF("Unable to zap GPT header @ sector %llu",
			    DL_GETDSIZE(&dl) - 1);
	}
	free(secbuf);
}

int
GPT_write(void)
{
	char			*secbuf;
	uint64_t		 altgh, altgp, prigh, prigp;
	uint64_t		 gpbytes, gpsectors;
	int			 rslt;

	if (MBR_write(&gmbr))
		return -1;

	/*
	 * XXX Assume size of gp is multiple of sector size.
	 */
	gpbytes = letoh32(gh.gh_part_num) * letoh32(gh.gh_part_size);
	gpsectors = gpbytes / dl.d_secsize;

	prigh = GPTSECTOR;
	prigp = prigh + 1;
	altgh = DL_GETDSIZE(&dl) - 1;
	altgp = altgh - gpsectors;

	gh.gh_lba_self = htole64(prigh);
	gh.gh_lba_alt = htole64(altgh);
	gh.gh_part_lba = htole64(prigp);
	gh.gh_part_csum = htole32(crc32((unsigned char *)&gp, gpbytes));
	gh.gh_csum = 0;
	gh.gh_csum = htole32(crc32((unsigned char *)&gh, letoh32(gh.gh_size)));

	secbuf = DISK_readsectors(prigh, 1);
	if (secbuf == NULL)
		return -1;

	memcpy(secbuf, &gh, sizeof(gh));
	rslt = DISK_writesectors(secbuf, prigh, 1);
	free(secbuf);
	if (rslt)
		return -1;

	gh.gh_lba_self = htole64(altgh);
	gh.gh_lba_alt = htole64(prigh);
	gh.gh_part_lba = htole64(altgp);
	gh.gh_csum = 0;
	gh.gh_csum = htole32(crc32((unsigned char *)&gh, letoh32(gh.gh_size)));

	secbuf = DISK_readsectors(altgh, 1);
	if (secbuf == NULL)
		return -1;

	memcpy(secbuf, &gh, sizeof(gh));
	rslt = DISK_writesectors(secbuf, altgh, 1);
	free(secbuf);
	if (rslt)
		return -1;

	if (DISK_writesectors((const char *)&gp, prigp, gpsectors))
		return -1;
	if (DISK_writesectors((const char *)&gp, altgp, gpsectors))
		return -1;

	/* Refresh in-kernel disklabel from the updated disk information. */
	if (ioctl(disk.dk_fd, DIOCRLDINFO, 0) == -1)
		warn("DIOCRLDINFO");

	return 0;
}

int
gp_lba_start_cmp(const void *e1, const void *e2)
{
	struct gpt_partition	*p1 = *(struct gpt_partition **)e1;
	struct gpt_partition	*p2 = *(struct gpt_partition **)e2;
	uint64_t		 o1;
	uint64_t		 o2;

	o1 = letoh64(p1->gp_lba_start);
	o2 = letoh64(p2->gp_lba_start);

	if (o1 < o2)
		return -1;
	else if (o1 > o2)
		return 1;
	else
		return 0;
}

struct gpt_partition **
sort_gpt(void)
{
	static struct gpt_partition	*sgp[NGPTPARTITIONS+2];
	unsigned int			 i, j;

	memset(sgp, 0, sizeof(sgp));

	j = 0;
	for (i = 0; i < letoh32(gh.gh_part_num); i++) {
		if (letoh64(gp[i].gp_lba_start) >= letoh64(gh.gh_lba_start))
			sgp[j++] = &gp[i];
	}

	if (j > 1) {
		if (mergesort(sgp, j, sizeof(sgp[0]), gp_lba_start_cmp) == -1) {
			printf("unable to sort gpt by lba start\n");
			return NULL;
		}
	}

	return sgp;
}

int
lba_free(uint64_t *start, uint64_t *end)
{
	struct gpt_partition	**sgp;
	uint64_t		  bs, bigbs, nextbs, ns;
	unsigned int		  i;

	sgp = sort_gpt();
	if (sgp == NULL)
		return -1;

	bs = letoh64(gh.gh_lba_start);
	ns = letoh64(gh.gh_lba_end) - bs + 1;

	if (sgp[0] != NULL) {
		bigbs = bs;
		ns = 0;
		for (i = 0; sgp[i] != NULL; i++) {
			nextbs = letoh64(sgp[i]->gp_lba_start);
			if (bs < nextbs && ns < nextbs - bs) {
				ns = nextbs - bs;
				bigbs = bs;
			}
			bs = letoh64(sgp[i]->gp_lba_end) + 1;
		}
		nextbs = letoh64(gh.gh_lba_end) + 1;
		if (bs < nextbs && ns < nextbs - bs) {
			ns = nextbs - bs;
			bigbs = bs;
		}
		bs = bigbs;
	}

	if (ns == 0)
		return -1;

	if (start != NULL)
		*start = bs;
	if (end != NULL)
		*end = bs + ns - 1;

	return 0;
}

int
GPT_get_lba_start(const unsigned int pn)
{
	uint64_t		bs;
	unsigned int		i;
	int			rslt;

	bs = letoh64(gh.gh_lba_start);

	if (letoh64(gp[pn].gp_lba_start) >= bs) {
		bs = letoh64(gp[pn].gp_lba_start);
	} else {
		rslt = lba_free(&bs, NULL);
		if (rslt == -1) {
			printf("no space for partition %u\n", pn);
			return -1;
		}
	}

	bs = getuint64("Partition offset", bs, letoh64(gh.gh_lba_start),
	    letoh64(gh.gh_lba_end));

	for (i = 0; i < letoh32(gh.gh_part_num); i++) {
		if (i == pn)
			continue;
		if (bs >= letoh64(gp[i].gp_lba_start) &&
		    bs <= letoh64(gp[i].gp_lba_end)) {
			printf("partition %u can't start inside partition %u\n",
			    pn, i);
			return -1;
		}
	}

	gp[pn].gp_lba_start = htole64(bs);

	return 0;
}

int
GPT_get_lba_end(const unsigned int pn)
{
	struct gpt_partition	**sgp;
	uint64_t		  bs, nextbs, ns;
	unsigned int		  i;

	sgp = sort_gpt();
	if (sgp == NULL)
		return -1;

	bs = letoh64(gp[pn].gp_lba_start);
	ns = letoh64(gh.gh_lba_end) - bs + 1;
	for (i = 0; sgp[i] != NULL; i++) {
		nextbs = letoh64(sgp[i]->gp_lba_start);
		if (nextbs > bs) {
			ns = nextbs - bs;
			break;
		}
	}
	ns = getuint64("Partition size", ns, 1, ns);

	gp[pn].gp_lba_end = htole64(bs + ns - 1);

	return 0;
}

/*
 * Adapted from Hacker's Delight crc32b().
 *
 * To quote http://www.hackersdelight.org/permissions.htm :
 *
 * "You are free to use, copy, and distribute any of the code on
 *  this web site, whether modified by you or not. You need not give
 *  attribution. This includes the algorithms (some of which appear
 *  in Hacker's Delight), the Hacker's Assistant, and any code submitted
 *  by readers. Submitters implicitly agree to this."
 */
uint32_t
crc32(const u_char *buf, const uint32_t size)
{
	int			j;
	uint32_t		i, byte, crc, mask;

	crc = 0xFFFFFFFF;

	for (i = 0; i < size; i++) {
		byte = buf[i];			/* Get next byte. */
		crc = crc ^ byte;
		for (j = 7; j >= 0; j--) {	/* Do eight times. */
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}

	return ~crc;
}
