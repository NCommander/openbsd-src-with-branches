/*	$OpenBSD: gpt.c,v 1.11 2016/03/28 16:55:09 mestre Exp $	*/
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <uuid.h>

#include "disk.h"
#include "misc.h"
#include "part.h"
#include "gpt.h"

#ifdef DEBUG
#define DPRINTF(x...)	printf(x)
#else
#define DPRINTF(x...)
#endif

struct gpt_header gh;
struct gpt_partition gp[NGPTPARTITIONS];

struct gpt_partition	**sort_gpt(void);
int			  lba_start_cmp(const void *e1, const void *e2);

int
GPT_get_header(off_t where)
{
	char *secbuf;
	uint64_t partlastlba;
	int partspersec;
	uint32_t orig_gh_csum, new_gh_csum;

	secbuf = DISK_readsector(where);
	if (secbuf == 0)
		return (1);

	memcpy(&gh, secbuf, sizeof(struct gpt_header));
	free(secbuf);

	if (letoh64(gh.gh_sig) != GPTSIGNATURE) {
		DPRINTF("gpt signature: expected 0x%llx, got 0x%llx\n",
		    GPTSIGNATURE, letoh64(gh.gh_sig));
		return (1);
	}

	if (letoh32(gh.gh_rev) != GPTREVISION) {
		DPRINTF("gpt revision: expected 0x%x, got 0x%x\n",
		    GPTREVISION, letoh32(gh.gh_rev));
		return (1);
	}

	if (letoh64(gh.gh_lba_self) != where) {
		DPRINTF("gpt self lba: expected %lld, got %llu\n",
		    (long long)where, letoh64(gh.gh_lba_self));
		return (1);
	}

	if (letoh32(gh.gh_size) != GPTMINHDRSIZE) {
		DPRINTF("gpt header size: expected %u, got %u\n",
		    GPTMINHDRSIZE, letoh32(gh.gh_size));
		return (1);
	}

	if (letoh32(gh.gh_part_size) != GPTMINPARTSIZE) {
		DPRINTF("gpt partition size: expected %u, got %u\n",
		    GPTMINPARTSIZE, letoh32(gh.gh_part_size));
		return (1);
	}

	if (letoh32(gh.gh_part_num) > NGPTPARTITIONS) {
		DPRINTF("gpt partition count: expected <= %u, got %u\n",
		    NGPTPARTITIONS, letoh32(gh.gh_part_num));
		return (1);
	}

	orig_gh_csum = gh.gh_csum;
	gh.gh_csum = 0;
	new_gh_csum = crc32((unsigned char *)&gh, letoh32(gh.gh_size));
	gh.gh_csum = orig_gh_csum;
	if (letoh32(orig_gh_csum) != new_gh_csum) {
		DPRINTF("gpt header checksum: expected 0x%x, got 0x%x\n",
		    orig_gh_csum, new_gh_csum);
		return (1);
	}

	if (letoh64(gh.gh_lba_end) >= DL_GETDSIZE(&dl)) {
		DPRINTF("gpt last usable LBA: expected < %lld, got %llu\n",
		    DL_GETDSIZE(&dl), letoh64(gh.gh_lba_end));
		return (1);
	}

	if (letoh64(gh.gh_lba_start) >= letoh64(gh.gh_lba_end)) {
		DPRINTF("gpt first usable LBA: expected < %llu, got %llu\n",
		    letoh64(gh.gh_lba_start), letoh64(gh.gh_lba_start));
		return (1);
	}

	if (letoh64(gh.gh_part_lba) <= letoh64(gh.gh_lba_end) &&
	    letoh64(gh.gh_part_lba) >= letoh64(gh.gh_lba_start)) {
		DPRINTF("gpt partition table start lba: expected < %llu or "
		    "> %llu, got %llu\n", letoh64(gh.gh_lba_start),
		    letoh64(gh.gh_lba_end), letoh64(gh.gh_part_lba));
		return (1);
	}

	partspersec = dl.d_secsize / letoh32(gh.gh_part_size);
	partlastlba = letoh64(gh.gh_part_lba) +
	    ((letoh32(gh.gh_part_num) + partspersec - 1) / partspersec) - 1;
	if (partlastlba <= letoh64(gh.gh_lba_end) &&
	    partlastlba >= letoh64(gh.gh_lba_start)) {
		DPRINTF("gpt partition table last LBA: expected < %llu or "
		    "> %llu, got %llu\n", letoh64(gh.gh_lba_start),
		    letoh64(gh.gh_lba_end), partlastlba);
		return (1);
	}

	/*
	 * Other possible paranoia checks:
	 *	1) partition table starts before primary gpt lba.
	 *	2) partition table extends into lowest partition.
	 *	3) alt partition table starts before gh_lba_end.
	 */
	return (0);
}

int
GPT_get_partition_table(off_t where)
{
	ssize_t len;
	off_t off;
	int secs;
	uint32_t checksum, partspersec;

	DPRINTF("gpt partition table being read from LBA %llu\n",
	    letoh64(gh.gh_part_lba));

	partspersec = dl.d_secsize / letoh32(gh.gh_part_size);
	if (partspersec * letoh32(gh.gh_part_size) != dl.d_secsize) {
		DPRINTF("gpt partition table entry invalid size. %u\n",
		    letoh32(gh.gh_part_size));
		return (1);
	}
	secs = (letoh32(gh.gh_part_num) + partspersec - 1) / partspersec;

	memset(&gp, 0, sizeof(gp));

	where *= dl.d_secsize;
	off = lseek(disk.fd, where, SEEK_SET);
	if (off == -1) {
		DPRINTF("seek to gpt partition table @ sector %llu failed\n",
		    (unsigned long long)where / dl.d_secsize);
		return (1);
	}
	len = read(disk.fd, &gp, secs * dl.d_secsize);
	if (len == -1 || len != secs * dl.d_secsize) {
		DPRINTF("gpt partition table read failed.\n");
		return (1);
	}

	checksum = crc32((unsigned char *)&gp, letoh32(gh.gh_part_num) *
	    letoh32(gh.gh_part_size));
	if (checksum != letoh32(gh.gh_part_csum)) {
		DPRINTF("gpt partition table checksum: expected %x, got %x\n",
		    checksum, letoh32(gh.gh_part_csum));
		return (1);
	}

	return (0);
}

void
GPT_get_gpt(int which)
{
	int privalid, altvalid;

	/*
	 * primary header && primary partition table ||
	 * alt header && alt partition table
	 */
	privalid = GPT_get_header(GPTSECTOR);
	if (privalid == 0)
		privalid = GPT_get_partition_table(gh.gh_part_lba);
	if (which == 1 || (which == 0 && privalid == 0))
		return;

	/* No valid GPT found. Zap any artifacts. */
	memset(&gh, 0, sizeof(gh));
	memset(&gp, 0, sizeof(gp));

	altvalid = GPT_get_header(DL_GETDSIZE(&dl) - 1);
	if (altvalid == 0)
		altvalid = GPT_get_partition_table(gh.gh_part_lba);
	if (which == 2 || altvalid == 0)
		return;

	/* No valid GPT found. Zap any artifacts. */
	memset(&gh, 0, sizeof(gh));
	memset(&gp, 0, sizeof(gp));
}

void
GPT_print(char *units, int verbosity)
{
	const int secsize = unit_types[SECTORS].conversion;
	struct uuid guid;
	char *guidstr = NULL;
	double size;
	int i, u, status;

	u = unit_lookup(units);
	size = ((double)DL_GETDSIZE(&dl) * secsize) / unit_types[u].conversion;
	printf("Disk: %s       Usable LBA: %llu to %llu [%.0f ",
	    disk.name, letoh64(gh.gh_lba_start), letoh64(gh.gh_lba_end), size);

	if (u == SECTORS && secsize != DEV_BSIZE)
		printf("%d-byte ", secsize);
	printf("%s]\n", unit_types[u].lname);

	if (verbosity) {
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
GPT_print_parthdr(int verbosity)
{
	printf("   #: type                                "
	    " [       start:         size ]\n");
	if (verbosity)
		printf("      guid                                 name\n");
	printf("--------------------------------------------------------"
	    "----------------\n");
}

void
GPT_print_part(int n, char *units, int verbosity)
{
	struct uuid guid;
	const int secsize = unit_types[SECTORS].conversion;
	struct gpt_partition *partn = &gp[n];
	char *guidstr = NULL;
	double size;
	int u, status;

	uuid_dec_le(&partn->gp_type, &guid);
	u = unit_lookup(units);
	size = letoh64(partn->gp_lba_end) - letoh64(partn->gp_lba_start) + 1;
	size = (size * secsize) / unit_types[u].conversion;
	printf("%c%3d: %-36s [%12lld: %12.0f%s]\n",
	    (letoh64(partn->gp_attrs) & GPTDOSACTIVE)?'*':' ', n,
	    PRT_uuid_to_typename(&guid), letoh64(partn->gp_lba_start),
	    size, unit_types[u].abbr);

	if (verbosity) {
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
GPT_init(void)
{
	extern u_int32_t b_arg;
	const int secsize = unit_types[SECTORS].conversion;
	struct uuid guid;
	int needed;
	uint32_t status;
	const uint8_t gpt_uuid_efi_system[] = GPT_UUID_EFI_SYSTEM;
	const uint8_t gpt_uuid_openbsd[] = GPT_UUID_OPENBSD;

	memset(&gh, 0, sizeof(gh));
	memset(&gp, 0, sizeof(gp));

	needed = sizeof(gp) / secsize + 2;
	/* Start on 64 sector boundary */
	if (needed % 64)
		needed += (64 - (needed % 64));

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
	if (status != uuid_s_ok)
		return (1);
	uuid_enc_le(&gh.gh_guid, &guid);

#if defined(__i386__) || defined(__amd64__)
	if (b_arg > 0) {
		/* Add an EFI system partition on i386/amd64. */
		uuid_dec_be(gpt_uuid_efi_system, &guid);
		uuid_enc_le(&gp[1].gp_type, &guid);
		uuid_create(&guid, &status);
		if (status != uuid_s_ok)
			return (1);
		uuid_enc_le(&gp[1].gp_guid, &guid);
		gp[1].gp_lba_start = gh.gh_lba_start;
		gp[1].gp_lba_end = htole64(letoh64(gh.gh_lba_start)+b_arg - 1);
		memcpy(gp[1].gp_name, string_to_utf16le("EFI System Area"),
		    sizeof(gp[1].gp_name));
	}
#endif
	uuid_dec_be(gpt_uuid_openbsd, &guid);
	uuid_enc_le(&gp[3].gp_type, &guid);
	uuid_create(&guid, &status);
	if (status != uuid_s_ok)
		return (1);
	uuid_enc_le(&gp[3].gp_guid, &guid);
	gp[3].gp_lba_start = gh.gh_lba_start;
#if defined(__i386__) || defined(__amd64__)
	if (b_arg > 0) {
		gp[3].gp_lba_start = htole64(letoh64(gp[3].gp_lba_start) +
		    b_arg);
		if (letoh64(gp[3].gp_lba_start) % 64)
			gp[3].gp_lba_start =
			    htole64(letoh64(gp[3].gp_lba_start) +
			    (64 - letoh64(gp[3].gp_lba_start) % 64));
	}
#endif
	gp[3].gp_lba_end = gh.gh_lba_end;
	memcpy(gp[3].gp_name, string_to_utf16le("OpenBSD Area"),
	    sizeof(gp[3].gp_name));

	gh.gh_part_csum = crc32((unsigned char *)&gp, sizeof(gp));
	gh.gh_csum = crc32((unsigned char *)&gh, sizeof(gh));

	return 0;
}

int
GPT_write(void)
{
	char *secbuf;
	const int secsize = unit_types[SECTORS].conversion;
	ssize_t len;
	off_t off;
	u_int64_t altgh, altgp;

	/* Assume we always write full-size partition table. XXX */
	altgh = DL_GETDSIZE(&dl) - 1;
	altgp = DL_GETDSIZE(&dl) - 1 - (sizeof(gp) / secsize);

	/*
	 * Place the new GPT header at the start of sectors 1 and
	 * DL_GETDSIZE(lp)-1 and write the sectors back.
	 */
	gh.gh_lba_self = htole64(1);
	gh.gh_lba_alt = htole64(altgh);
	gh.gh_part_lba = htole64(2);
	gh.gh_part_csum = crc32((unsigned char *)&gp, sizeof(gp));
	gh.gh_csum = 0;
	gh.gh_csum = crc32((unsigned char *)&gh, letoh32(gh.gh_size));

	secbuf = DISK_readsector(1);
	if (secbuf == NULL)
		return (-1);

	memcpy(secbuf, &gh, sizeof(gh));
	DISK_writesector(secbuf, 1);
	free(secbuf);

	gh.gh_lba_self = htole64(altgh);
	gh.gh_lba_alt = htole64(1);
	gh.gh_part_lba = htole64(altgp);
	gh.gh_csum = 0;
	gh.gh_csum = crc32((unsigned char *)&gh, letoh32(gh.gh_size));

	secbuf = DISK_readsector(altgh);
	if (secbuf == NULL)
		return (-1);

	memcpy(secbuf, &gh, sizeof(gh));
	DISK_writesector(secbuf, altgh);
	free(secbuf);

	/*
	 * Write partition table after primary header
	 * (i.e. at sector 1) and before alt header
	 * (i.e. ending in sector before alt header.
	 * XXX ALWAYS NGPTPARTITIONS!
	 * XXX ASSUME gp is multiple of sector size!
	 */
	off = lseek(disk.fd, secsize * 2, SEEK_SET);
	if (off == secsize * 2)
		len = write(disk.fd, &gp, sizeof(gp));
	else
		len = -1;
	if (len == -1 || len != sizeof(gp)) {
		errno = EIO;
		return (-1);
	}

	off = lseek(disk.fd, secsize * altgp, SEEK_SET);
	if (off == secsize * altgp)
		len = write(disk.fd, &gp, sizeof(gp));
	else
		len = -1;

	if (len == -1 || len != sizeof(gp)) {
		errno = EIO;
		return (-1);
	}

	/* Refresh in-kernel disklabel from the updated disk information. */
	ioctl(disk.fd, DIOCRLDINFO, 0);

	return (0);
}

int
gp_lba_start_cmp(const void *e1, const void *e2)
{
	struct gpt_partition *p1 = *(struct gpt_partition **)e1;
	struct gpt_partition *p2 = *(struct gpt_partition **)e2;
	u_int64_t o1;
	u_int64_t o2;

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
	static struct gpt_partition *sgp[NGPTPARTITIONS+2];
	unsigned int i, j;

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

	return (sgp);
}

int
GPT_get_lba_start(unsigned int pn)
{
	struct gpt_partition	**sgp;
	uint64_t		  bs, bigbs, nextbs, ns;
	unsigned int		  i;

	bs = letoh64(gh.gh_lba_start);

	if (letoh64(gp[pn].gp_lba_start) >= bs) {
		bs = letoh64(gp[pn].gp_lba_start);
	} else {
		sgp = sort_gpt();
		if (sgp == NULL)
			return -1;
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
			if (ns == 0) {
				printf("no space for partition %u\n", pn);
				return -1;
			}
			bs = bigbs;
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
GPT_get_lba_end(unsigned int pn)
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
