/*	$OpenBSD: cmd.c,v 1.147 2021/10/21 13:16:49 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#include <sys/types.h>
#include <sys/disklabel.h>

#include <err.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uuid.h>

#include "part.h"
#include "disk.h"
#include "misc.h"
#include "mbr.h"
#include "gpt.h"
#include "user.h"
#include "cmd.h"

int		 gedit(const int);
int		 edit(const int, struct mbr *);
int		 gsetpid(const int);
int		 setpid(const int, struct mbr *);
int		 parsepn(const char *);

int		 ask_num(const char *, int, int, int);
int		 ask_pid(const int);
struct uuid	*ask_uuid(const struct uuid *);
char		*ask_string(const char *, const char *);

extern const unsigned char	manpage[];
extern const int		manpage_sz;

int
Xreinit(char *args, struct mbr *mbr)
{
	int			dogpt;

	dogpt = 0;

	if (strcasecmp(args, "gpt") == 0)
		dogpt = 1;
	else if (strcasecmp(args, "mbr") == 0)
		dogpt = 0;
	else if (strlen(args) > 0) {
		printf("Unrecognized modifier '%s'\n", args);
		return CMD_CONT;
	}

	if (dogpt) {
		GPT_init(GHANDGP);
		GPT_print("s", TERSE);
	} else {
		MBR_init(mbr);
		MBR_print(mbr, "s");
	}

	printf("Use 'write' to update disk.\n");

	return CMD_DIRTY;
}

int
Xswap(char *args, struct mbr *mbr)
{
	char			*from, *to;
	int			 pf, pt;
	struct prt		 pp;
	struct gpt_partition	 gg;

	to = args;
	from = strsep(&to, WHITESPACE);

	pt = parsepn(to);
	if (pt == -1)
		return CMD_CONT;

	pf = parsepn(from);
	if (pf == -1)
		return CMD_CONT;

	if (pt == pf) {
		printf("%d same partition as %d, doing nothing.\n", pt, pf);
		return CMD_CONT;
	}

	if (letoh64(gh.gh_sig) == GPTSIGNATURE) {
		gg = gp[pt];
		gp[pt] = gp[pf];
		gp[pf] = gg;
	} else {
		pp = mbr->mbr_prt[pt];
		mbr->mbr_prt[pt] = mbr->mbr_prt[pf];
		mbr->mbr_prt[pf] = pp;
	}

	return CMD_DIRTY;
}

int
gedit(const int pn)
{
	struct uuid		 oldtype;
	struct gpt_partition	*gg;
	char			*name;
	uint16_t		*utf;
	int			 i;

	gg = &gp[pn];
	oldtype = gg->gp_type;

	if (gsetpid(pn))
		return -1;

	if (uuid_is_nil(&gg->gp_type, NULL)) {
		if (uuid_is_nil(&oldtype, NULL) == 0) {
			memset(gg, 0, sizeof(struct gpt_partition));
			printf("Partition %d is disabled.\n", pn);
		}
		return 0;
	}

	if (GPT_get_lba_start(pn) == -1 ||
	    GPT_get_lba_end(pn) == -1) {
		return -1;
	}

	name = ask_string("Partition name", utf16le_to_string(gg->gp_name));
	if (strlen(name) >= GPTPARTNAMESIZE) {
		printf("partition name must be < %d characters\n",
		    GPTPARTNAMESIZE);
		return -1;
	}
	/*
	 * N.B.: simple memcpy() could copy trash from static buf! This
	 * would create false positives for the partition having changed.
	 */
	utf = string_to_utf16le(name);
	for (i = 0; i < GPTPARTNAMESIZE; i++) {
		gg->gp_name[i] = utf[i];
		if (utf[i] == 0)
			break;
	}
	return 0;
}

int
parsepn(const char *pnstr)
{
	const char		*errstr;
	int			 maxpn, pn;

	if (pnstr == NULL) {
		printf("no partition number\n");
		return -1;
	}

	if (letoh64(gh.gh_sig) == GPTSIGNATURE)
		maxpn = letoh32(gh.gh_part_num) - 1;
	else
		maxpn = NDOSPART - 1;

	pn = strtonum(pnstr, 0, maxpn, &errstr);
	if (errstr) {
		printf("partition number is %s: %s\n", errstr, pnstr);
		return -1;
	}

	return pn;
}

int
edit(const int pn, struct mbr *mbr)
{
	struct prt		*pp;
	unsigned char		 oldid;

	pp = &mbr->mbr_prt[pn];
	oldid = pp->prt_id;

	if (setpid(pn, mbr))
		return -1;

	if (pp->prt_id == DOSPTYP_UNUSED) {
		if (oldid != DOSPTYP_UNUSED) {
			memset(pp, 0, sizeof(*pp));
			printf("Partition %d is disabled.\n", pn);
		}
		return 0;
	}

	if (ask_yn("Do you wish to edit in CHS mode?")) {
		pp->prt_scyl = ask_num("BIOS Starting cylinder", pp->prt_scyl,  0,
		    disk.dk_cylinders - 1);
		pp->prt_shead = ask_num("BIOS Starting head",    pp->prt_shead, 0,
		    disk.dk_heads - 1);
		pp->prt_ssect = ask_num("BIOS Starting sector",  pp->prt_ssect, 1,
		    disk.dk_sectors);

		pp->prt_ecyl = ask_num("BIOS Ending cylinder",   pp->prt_ecyl,
		    pp->prt_scyl, disk.dk_cylinders - 1);
		pp->prt_ehead = ask_num("BIOS Ending head",      pp->prt_ehead,
		    (pp->prt_scyl == pp->prt_ecyl) ? pp->prt_shead : 0, disk.dk_heads - 1);
		pp->prt_esect = ask_num("BIOS Ending sector",    pp->prt_esect,
		    (pp->prt_scyl == pp->prt_ecyl && pp->prt_shead == pp->prt_ehead) ? pp->prt_ssect
		    : 1, disk.dk_sectors);

		/* Fix up off/size values */
		PRT_fix_BN(pp, pn);
		/* Fix up CHS values for LBA */
		PRT_fix_CHS(pp);
	} else {
		pp->prt_bs = getuint64("Partition offset", pp->prt_bs, 0, disk.dk_size - 1);
		pp->prt_ns = getuint64("Partition size",   pp->prt_ns, 1,
		    disk.dk_size - pp->prt_bs);

		/* Fix up CHS values */
		PRT_fix_CHS(pp);
	}

	return 0;
}

int
Xedit(char *args, struct mbr *mbr)
{
	struct gpt_partition	 oldgg;
	struct prt		 oldprt;
	struct gpt_partition	*gg;
	int			 pn;

	pn = parsepn(args);
	if (pn == -1)
		return CMD_CONT;

	if (letoh64(gh.gh_sig) == GPTSIGNATURE) {
		oldgg = gp[pn];
		if (gedit(pn))
			gp[pn] = oldgg;
		else if (memcmp(&gp[pn], &oldgg, sizeof(oldgg)))
			return CMD_DIRTY;
	} else {
		oldprt = mbr->mbr_prt[pn];
		if (edit(pn, mbr))
			mbr->mbr_prt[pn] = oldprt;
		else if (memcmp(&mbr->mbr_prt[pn], &oldprt, sizeof(oldprt)))
			return CMD_DIRTY;
	}

	return CMD_CONT;
}

int
gsetpid(const int pn)
{
	struct uuid		 gp_type, gp_guid;
	struct gpt_partition	*gg;
	int			 status;

	gg = &gp[pn];

	GPT_print_parthdr(TERSE);
	GPT_print_part(pn, "s", TERSE);

	uuid_dec_le(&gg->gp_type, &gp_type);
	if (PRT_protected_guid(&gp_type)) {
		printf("can't edit partition type %s\n",
		    PRT_uuid_to_typename(&gp_type));
		return -1;
	}

	gp_type = *ask_uuid(&gp_type);
	if (PRT_protected_guid(&gp_type)) {
		printf("can't change partition type to %s\n",
		    PRT_uuid_to_typename(&gp_type));
		return -1;
	}

	uuid_dec_le(&gg->gp_guid, &gp_guid);
	if (uuid_is_nil(&gp_guid, NULL)) {
		uuid_create(&gp_guid, &status);
		if (status != uuid_s_ok) {
			printf("could not create guid for partition\n");
			return -1;
		}
	}

	uuid_enc_le(&gg->gp_type, &gp_type);
	uuid_enc_le(&gg->gp_guid, &gp_guid);

	return 0;
}

int
setpid(const int pn, struct mbr *mbr)
{
	struct prt		*pp;

	pp = &mbr->mbr_prt[pn];

	PRT_print_parthdr();
	PRT_print_part(pn, pp, "s");

	pp->prt_id = ask_pid(pp->prt_id);

	return 0;
}

int
Xsetpid(char *args, struct mbr *mbr)
{
	struct gpt_partition	oldgg;
	struct prt		oldprt;
	int			pn;

	pn = parsepn(args);
	if (pn == -1)
		return CMD_CONT;

	if (letoh64(gh.gh_sig) == GPTSIGNATURE) {
		oldgg = gp[pn];
		if (gsetpid(pn))
			gp[pn] = oldgg;
		else if (memcmp(&gp[pn], &oldgg, sizeof(oldgg)))
			return CMD_DIRTY;
	} else {
		oldprt = mbr->mbr_prt[pn];
		if (setpid(pn, mbr))
			mbr->mbr_prt[pn] = oldprt;
		else if (memcmp(&mbr->mbr_prt[pn], &oldprt, sizeof(oldprt)))
			return CMD_DIRTY;
	}

	return CMD_CONT;
}

int
Xselect(char *args, struct mbr *mbr)
{
	static uint64_t		lba_firstembr = 0;
	uint64_t		lba_self;
	int			pn;

	pn = parsepn(args);
	if (pn == -1)
		return CMD_CONT;

	lba_self = mbr->mbr_prt[pn].prt_bs;

	if ((mbr->mbr_prt[pn].prt_id != DOSPTYP_EXTEND) &&
	    (mbr->mbr_prt[pn].prt_id != DOSPTYP_EXTENDL)) {
		printf("Partition %d is not an extended partition.\n", pn);
		return CMD_CONT;
	}

	if (lba_firstembr == 0)
		lba_firstembr = lba_self;

	if (lba_self == 0) {
		printf("Loop to MBR (sector 0)! Not selected.\n");
		return CMD_CONT;
	} else {
		printf("Selected extended partition %d\n", pn);
		printf("New EMBR at offset %llu.\n", lba_self);
	}

	USER_edit(lba_self, lba_firstembr);

	return CMD_CONT;
}

int
Xprint(char *args, struct mbr *mbr)
{
	if (letoh64(gh.gh_sig) == GPTSIGNATURE)
		GPT_print(args, VERBOSE);
	else
		MBR_print(mbr, args);

	return CMD_CONT;
}

int
Xwrite(char *args, struct mbr *mbr)
{
	int			i, n;

	for (i = 0, n = 0; i < NDOSPART; i++)
		if (mbr->mbr_prt[i].prt_id == 0xA6)
			n++;
	if (n >= 2) {
		warnx("MBR contains more than one OpenBSD partition!");
		if (!ask_yn("Write MBR anyway?"))
			return CMD_CONT;
	}

	if (letoh64(gh.gh_sig) == GPTSIGNATURE) {
		printf("Writing GPT.\n");
		if (GPT_write() == -1) {
			warn("error writing GPT");
			return CMD_CONT;
		}
	} else {
		printf("Writing MBR at offset %lld.\n", (long long)mbr->mbr_lba_self);
		if (MBR_write(mbr) == -1) {
			warn("error writing MBR");
			return CMD_CONT;
		}
		GPT_zap_headers();
	}

	return CMD_CLEAN;
}

int
Xquit(char *args, struct mbr *mbr)
{
	return CMD_QUIT;
}

int
Xabort(char *args, struct mbr *mbr)
{
	exit(0);
}

int
Xexit(char *args, struct mbr *mbr)
{
	return CMD_EXIT;
}

int
Xhelp(char *args, struct mbr *mbr)
{
	USER_help();

	return CMD_CONT;
}

int
Xupdate(char *args, struct mbr *mbr)
{
	memcpy(mbr->mbr_code, default_dmbr.dmbr_boot, sizeof(mbr->mbr_code));
	mbr->mbr_signature = DOSMBR_SIGNATURE;
	printf("Machine code updated.\n");
	return CMD_DIRTY;
}

int
Xflag(char *args, struct mbr *mbr)
{
	const char		*errstr;
	char			*part, *flag;
	long long		 val = -1;
	int			 i, pn;

	flag = args;
	part = strsep(&flag, WHITESPACE);

	pn = parsepn(part);
	if (pn == -1)
		return CMD_CONT;

	if (flag != NULL) {
		if (letoh64(gh.gh_sig) == GPTSIGNATURE)
			val = strtonum(flag, 0, INT64_MAX, &errstr);
		else
			val = strtonum(flag, 0, 0xff, &errstr);
		if (errstr) {
			printf("flag value is %s: %s.\n", errstr, flag);
			return CMD_CONT;
		}
		if (letoh64(gh.gh_sig) == GPTSIGNATURE)
			gp[pn].gp_attrs = htole64(val);
		else
			mbr->mbr_prt[pn].prt_flag = val;
		printf("Partition %d flag value set to 0x%llx.\n", pn, val);
	} else {
		if (letoh64(gh.gh_sig) == GPTSIGNATURE) {
			for (i = 0; i < NGPTPARTITIONS; i++) {
				if (i == pn)
					gp[i].gp_attrs = htole64(GPTDOSACTIVE);
				else
					gp[i].gp_attrs = htole64(0);
			}
		} else {
			for (i = 0; i < NDOSPART; i++) {
				if (i == pn)
					mbr->mbr_prt[i].prt_flag = DOSACTIVE;
				else
					mbr->mbr_prt[i].prt_flag = 0x00;
			}
		}
		printf("Partition %d marked active.\n", pn);
	}

	return CMD_DIRTY;
}

int
Xmanual(char *args, struct mbr *mbr)
{
	char			*pager = "/usr/bin/less";
	char			*p;
	FILE			*f;
	sig_t			 opipe;

	opipe = signal(SIGPIPE, SIG_IGN);
	if ((p = getenv("PAGER")) != NULL && (*p != '\0'))
		pager = p;
	if (asprintf(&p, "gunzip -qc|%s", pager) != -1) {
		f = popen(p, "w");
		if (f) {
			fwrite(manpage, manpage_sz, 1, f);
			pclose(f);
		}
		free(p);
	}

	signal(SIGPIPE, opipe);

	return CMD_CONT;
}

int
ask_num(const char *str, int dflt, int low, int high)
{
	char			 lbuf[100];
	const char		*errstr;
	int			 num;

	if (dflt < low)
		dflt = low;
	else if (dflt > high)
		dflt = high;

	do {
		printf("%s [%d - %d]: [%d] ", str, low, high, dflt);
		string_from_line(lbuf, sizeof(lbuf), TRIMMED);

		if (lbuf[0] == '\0') {
			num = dflt;
			errstr = NULL;
		} else {
			num = (int)strtonum(lbuf, low, high, &errstr);
			if (errstr)
				printf("%s is %s: %s.\n", str, errstr, lbuf);
		}
	} while (errstr);

	return num;
}

int
ask_pid(const int dflt)
{
	char			lbuf[100];
	int			num;

	for (;;) {
		printf("Partition id ('0' to disable) [01 - FF]: [%02X] ", dflt);
		printf("(? for help) ");
		string_from_line(lbuf, sizeof(lbuf), TRIMMED);

		if (strlen(lbuf) == 0)
			return dflt;
		if (strcmp(lbuf, "?") == 0) {
			PRT_print_mbrtypes();
			continue;
		}

		num = hex_octet(lbuf);
		if (num != -1)
			return num;

		printf("'%s' is not a valid partition id.\n", lbuf);
	}
}

struct uuid *
ask_uuid(const struct uuid *olduuid)
{
	static struct uuid	 uuid;
	char			 lbuf[100];
	char			*dflt = NULL;
	int			 status;
	int			 num = 0;

	uuid = *olduuid;
	if (uuid_is_nil(&uuid, NULL) == 0) {
		num = PRT_uuid_to_type(&uuid);
		if (num == 0) {
			uuid_to_string(&uuid, &dflt, &status);
			if (status != uuid_s_ok) {
				printf("uuid_to_string() failed\n");
				goto done;
			}
		}
	}
	if (dflt == NULL) {
		if (asprintf(&dflt, "%X", num) == -1) {
			warn("asprintf()");
			goto done;
		}
	}

	for (;;) {
		printf("Partition id ('0' to disable) [01 - FF, <uuid>]: [%s] ",
		    dflt);
		printf("(? for help) ");
		string_from_line(lbuf, sizeof(lbuf), TRIMMED);

		if (strcmp(lbuf, "?") == 0) {
			PRT_print_gpttypes();
			continue;
		} else if (strlen(lbuf) == 0) {
			uuid = *olduuid;
			goto done;
		}

		uuid_from_string(lbuf, &uuid, &status);
		if (status == uuid_s_ok)
			goto done;

		num = hex_octet(lbuf);
		switch (num) {
		case -1:
			printf("'%s' is not a valid partition id\n", lbuf);
			break;
		case 0:
			uuid_create_nil(&uuid, NULL);
			goto done;
		default:
			uuid = *PRT_type_to_uuid(num);
			if (uuid_is_nil(&uuid, NULL) == 0)
				goto done;
			printf("'%s' has no associated UUID\n", lbuf);
			break;
		}
	}

 done:
	free(dflt);
	return &uuid;
}

char *
ask_string(const char *prompt, const char *oval)
{
	static char		buf[UUID_STR_LEN + 1];

	buf[0] = '\0';
	printf("%s: [%s] ", prompt, oval ? oval : "");
	string_from_line(buf, sizeof(buf), UNTRIMMED);

	if (buf[0] == '\0' && oval)
		strlcpy(buf, oval, sizeof(buf));

	return buf;
}
