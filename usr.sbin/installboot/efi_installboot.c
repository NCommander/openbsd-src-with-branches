/*	$OpenBSD: armv7_installboot.c,v 1.11 2021/07/20 14:51:56 kettenis Exp $	*/
/*	$NetBSD: installboot.c,v 1.5 1995/11/17 23:23:50 gwr Exp $ */

/*
 * Copyright (c) 2011 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2010 Otto Moerbeek <otto@openbsd.org>
 * Copyright (c) 2003 Tom Cosgrove <tom.cosgrove@arches-consulting.com>
 * Copyright (c) 1997 Michael Shalayeff
 * Copyright (c) 1994 Paul Kranenburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>	/* DEV_BSIZE */
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <uuid.h>

#include "installboot.h"

static int	create_filesystem(struct disklabel *, char);
static void	write_filesystem(struct disklabel *, char);
static int	findgptefisys(int, struct disklabel *);
static int	findmbrfat(int, struct disklabel *);

void
md_init(void)
{
}

void
md_loadboot(void)
{
}

void
md_prepareboot(int devfd, char *dev)
{
	struct disklabel dl;
	int part;

	/* Get and check disklabel. */
	if (ioctl(devfd, DIOCGDINFO, &dl) == -1)
		err(1, "disklabel: %s", dev);
	if (dl.d_magic != DISKMAGIC)
		errx(1, "bad disklabel magic=0x%08x", dl.d_magic);

	/* Warn on unknown disklabel types. */
	if (dl.d_type == 0)
		warnx("disklabel type unknown");

	part = findgptefisys(devfd, &dl);
	if (part != -1) {
		create_filesystem(&dl, (char)part);
		return;
	}

	part = findmbrfat(devfd, &dl);
	if (part != -1) {
		create_filesystem(&dl, (char)part);
		return;
	}
}

void
md_installboot(int devfd, char *dev)
{
	struct disklabel dl;
	int part;

	/* Get and check disklabel. */
	if (ioctl(devfd, DIOCGDINFO, &dl) == -1)
		err(1, "disklabel: %s", dev);
	if (dl.d_magic != DISKMAGIC)
		errx(1, "bad disklabel magic=0x%08x", dl.d_magic);

	/* Warn on unknown disklabel types. */
	if (dl.d_type == 0)
		warnx("disklabel type unknown");

	part = findgptefisys(devfd, &dl);
	if (part != -1) {
		write_filesystem(&dl, (char)part);
		return;
	}

	part = findmbrfat(devfd, &dl);
	if (part != -1) {
		write_filesystem(&dl, (char)part);
		return;
	}
}

static int
create_filesystem(struct disklabel *dl, char part)
{
	static char *newfsfmt ="/sbin/newfs_msdos %s >/dev/null";
	struct msdosfs_args args;
	char cmd[60];
	int rslt;

	/* Mount <duid>.<part> as msdos filesystem. */
	memset(&args, 0, sizeof(args));
	rslt = asprintf(&args.fspec,
	    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx.%c",
            dl->d_uid[0], dl->d_uid[1], dl->d_uid[2], dl->d_uid[3],
            dl->d_uid[4], dl->d_uid[5], dl->d_uid[6], dl->d_uid[7],
	    part);
	if (rslt == -1) {
		warn("bad special device");
		return rslt;
	}

	rslt = snprintf(cmd, sizeof(cmd), newfsfmt, args.fspec);
	if (rslt >= sizeof(cmd)) {
		warnx("can't build newfs command");
		rslt = -1;
		return rslt;
	}

	if (verbose)
		fprintf(stderr, "%s %s\n",
		    (nowrite ? "would newfs" : "newfsing"), args.fspec);
	if (!nowrite) {
		rslt = system(cmd);
		if (rslt == -1) {
			warn("system('%s') failed", cmd);
			return rslt;
		}
	}

	return 0;
}

static void
write_filesystem(struct disklabel *dl, char part)
{
	static char *fsckfmt = "/sbin/fsck_msdos %s >/dev/null";
	struct msdosfs_args args;
	char cmd[60];
	char dst[PATH_MAX];
	char *src;
	size_t mntlen, pathlen, srclen;
	int rslt;

	src = NULL;

	/* Create directory for temporary mount point. */
	strlcpy(dst, "/tmp/installboot.XXXXXXXXXX", sizeof(dst));
	if (mkdtemp(dst) == NULL)
		err(1, "mkdtemp('%s') failed", dst);
	mntlen = strlen(dst);

	/* Mount <duid>.<part> as msdos filesystem. */
	memset(&args, 0, sizeof(args));
	rslt = asprintf(&args.fspec,
	    "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx.%c",
            dl->d_uid[0], dl->d_uid[1], dl->d_uid[2], dl->d_uid[3],
            dl->d_uid[4], dl->d_uid[5], dl->d_uid[6], dl->d_uid[7],
	    part);
	if (rslt == -1) {
		warn("bad special device");
		goto rmdir;
	}

	args.export_info.ex_root = -2;
	args.export_info.ex_flags = 0;
	args.flags = MSDOSFSMNT_LONGNAME;

	if (mount(MOUNT_MSDOS, dst, 0, &args) == -1) {
		/* Try fsck'ing it. */
		rslt = snprintf(cmd, sizeof(cmd), fsckfmt, args.fspec);
		if (rslt >= sizeof(cmd)) {
			warnx("can't build fsck command");
			rslt = -1;
			goto rmdir;
		}
		rslt = system(cmd);
		if (rslt == -1) {
			warn("system('%s') failed", cmd);
			goto rmdir;
		}
		if (mount(MOUNT_MSDOS, dst, 0, &args) == -1) {
			/* Try newfs'ing it. */
			rslt = create_filesystem(dl, part);
			if (rslt == -1)
				goto rmdir;
			rslt = mount(MOUNT_MSDOS, dst, 0, &args);
			if (rslt == -1) {
				warn("unable to mount EFI System partition");
				goto rmdir;
			}
		}
	}

	/* Create "/efi/boot" directory in <duid>.<part>. */
	if (strlcat(dst, "/efi", sizeof(dst)) >= sizeof(dst)) {
		rslt = -1;
		warn("unable to build /efi directory");
		goto umount;
	}
	rslt = mkdir(dst, 0755);
	if (rslt == -1 && errno != EEXIST) {
		warn("mkdir('%s') failed", dst);
		goto umount;
	}
	if (strlcat(dst, "/boot", sizeof(dst)) >= sizeof(dst)) {
		rslt = -1;
		warn("unable to build /boot directory");
		goto umount;
	}
	rslt = mkdir(dst, 0755);
	if (rslt == -1 && errno != EEXIST) {
		warn("mkdir('%s') failed", dst);
		goto umount;
	}

#ifdef __aarch64__
	/*
	 * Copy BOOTAA64.EFI to /efi/boot/bootaa64.efi.
	 */
	pathlen = strlen(dst);
	if (strlcat(dst, "/bootaa64.efi", sizeof(dst)) >= sizeof(dst)) {
		rslt = -1;
		warn("unable to build /bootaa64.efi path");
		goto umount;
	}
	src = fileprefix(root, "/usr/mdec/BOOTAA64.EFI");
	if (src == NULL) {
		rslt = -1;
		goto umount;
	}
#elif defined(__arm__)
	/*
	 * Copy BOOTARM.EFI to /efi/boot/bootarm.efi.
	 */
	pathlen = strlen(dst);
	if (strlcat(dst, "/bootarm.efi", sizeof(dst)) >= sizeof(dst)) {
		rslt = -1;
		warn("unable to build /bootarm.efi path");
		goto umount;
	}
	src = fileprefix(root, "/usr/mdec/BOOTARM.EFI");
	if (src == NULL) {
		rslt = -1;
		goto umount;
	}
#elif defined(__riscv)
	/*
	 * Copy BOOTRISCV64.EFI to /efi/boot/bootriscv64.efi.
	 */
	pathlen = strlen(dst);
	if (strlcat(dst, "/bootriscv64.efi", sizeof(dst)) >= sizeof(dst)) {
		rslt = -1;
		warn("unable to build /bootriscv64.efi path");
		goto umount;
	}
	src = fileprefix(root, "/usr/mdec/BOOTRISCV64.EFI");
	if (src == NULL) {
		rslt = -1;
		goto umount;
	}
#endif
	srclen = strlen(src);
	if (verbose)
		fprintf(stderr, "%s %s to %s\n",
		    (nowrite ? "would copy" : "copying"), src, dst);
	if (!nowrite) {
		rslt = filecopy(src, dst);
		if (rslt == -1)
			goto umount;
	}

	rslt = 0;

umount:
	dst[mntlen] = '\0';
	if (unmount(dst, MNT_FORCE) == -1)
		err(1, "unmount('%s') failed", dst);

rmdir:
	free(args.fspec);
	dst[mntlen] = '\0';
	if (rmdir(dst) == -1)
		err(1, "rmdir('%s') failed", dst);

	free(src);

	if (rslt == -1)
		exit(1);
}

/*
 * Returns 0 if the MBR with the provided partition array is a GPT protective
 * MBR, and returns 1 otherwise. A GPT protective MBR would have one and only
 * one MBR partition, an EFI partition that either covers the whole disk or as
 * much of it as is possible with a 32bit size field.
 *
 * NOTE: MS always uses a size of UINT32_MAX for the EFI partition!**
 */
static int
gpt_chk_mbr(struct dos_partition *dp, u_int64_t dsize)
{
	struct dos_partition *dp2;
	int efi, found, i;
	u_int32_t psize;

	found = efi = 0;
	for (dp2=dp, i=0; i < NDOSPART; i++, dp2++) {
		if (dp2->dp_typ == DOSPTYP_UNUSED)
			continue;
		found++;
		if (dp2->dp_typ != DOSPTYP_EFI)
			continue;
		if (letoh32(dp2->dp_start) != GPTSECTOR)
			continue;
		psize = letoh32(dp2->dp_size);
		if (psize <= (dsize - GPTSECTOR) || psize == UINT32_MAX)
			efi++;
	}
	if (found == 1 && efi == 1)
		return (0);

	return (1);
}

int
findgptefisys(int devfd, struct disklabel *dl)
{
	struct gpt_partition	 gp[NGPTPARTITIONS];
	struct gpt_header	 gh;
	struct dos_partition	 dp[NDOSPART];
	struct uuid		 efisys_uuid;
	const char		 efisys_uuid_code[] = GPT_UUID_EFI_SYSTEM;
	off_t			 off;
	ssize_t			 len;
	u_int64_t		 start;
	int			 i;
	uint32_t		 orig_csum, new_csum;
	uint32_t		 ghsize, ghpartsize, ghpartnum, ghpartspersec;
	u_int8_t		*secbuf;

	/* Prepare EFI System UUID */
	uuid_dec_be(efisys_uuid_code, &efisys_uuid);

	if ((secbuf = malloc(dl->d_secsize)) == NULL)
		err(1, NULL);

	/* Check that there is a protective MBR. */
	len = pread(devfd, secbuf, dl->d_secsize, 0);
	if (len != dl->d_secsize)
		err(4, "can't read mbr");
	memcpy(dp, &secbuf[DOSPARTOFF], sizeof(dp));
	if (gpt_chk_mbr(dp, DL_GETDSIZE(dl))) {
		free(secbuf);
		return (-1);
	}

	/* Check GPT Header. */
	off = dl->d_secsize;	/* Read header from sector 1. */
	len = pread(devfd, secbuf, dl->d_secsize, off);
	if (len != dl->d_secsize)
		err(4, "can't pread gpt header");

	memcpy(&gh, secbuf, sizeof(gh));
	free(secbuf);

	/* Check signature */
	if (letoh64(gh.gh_sig) != GPTSIGNATURE)
		return (-1);

	if (letoh32(gh.gh_rev) != GPTREVISION)
		return (-1);

	ghsize = letoh32(gh.gh_size);
	if (ghsize < GPTMINHDRSIZE || ghsize > sizeof(struct gpt_header))
		return (-1);

	/* Check checksum */
	orig_csum = gh.gh_csum;
	gh.gh_csum = 0;
	new_csum = crc32((unsigned char *)&gh, ghsize);
	gh.gh_csum = orig_csum;
	if (letoh32(orig_csum) != new_csum)
		return (-1);

	off = letoh64(gh.gh_part_lba) * dl->d_secsize;
	ghpartsize = letoh32(gh.gh_part_size);
	ghpartspersec = dl->d_secsize / ghpartsize;
	ghpartnum = letoh32(gh.gh_part_num);
	if ((secbuf = malloc(dl->d_secsize)) == NULL)
		err(1, NULL);
	for (i = 0; i < (ghpartnum + ghpartspersec - 1) / ghpartspersec; i++) {
		len = pread(devfd, secbuf, dl->d_secsize, off);
		if (len != dl->d_secsize) {
			free(secbuf);
			return (-1);
		}
		memcpy(gp + i * ghpartspersec, secbuf,
		    ghpartspersec * sizeof(struct gpt_partition));
		off += dl->d_secsize;
	}
	free(secbuf);
	new_csum = crc32((unsigned char *)&gp, ghpartnum * ghpartsize);
	if (new_csum != letoh32(gh.gh_part_csum))
		return (-1);

	start = 0;
	for (i = 0; i < ghpartnum && start == 0; i++) {
		if (memcmp(&gp[i].gp_type, &efisys_uuid,
		    sizeof(struct uuid)) == 0)
			start = letoh64(gp[i].gp_lba_start);
	}

	if (start) {
		for (i = 0; i < MAXPARTITIONS; i++) {
			if (DL_GETPSIZE(&dl->d_partitions[i]) > 0 &&
			    DL_GETPOFFSET(&dl->d_partitions[i]) == start)
				return ('a' + i);
		}
	}

	return (-1);
}

int
findmbrfat(int devfd, struct disklabel *dl)
{
	struct dos_partition	 dp[NDOSPART];
	ssize_t			 len;
	u_int64_t		 start = 0;
	int			 i;
	u_int8_t		*secbuf;

	if ((secbuf = malloc(dl->d_secsize)) == NULL)
		err(1, NULL);

	/* Read MBR. */
	len = pread(devfd, secbuf, dl->d_secsize, 0);
	if (len != dl->d_secsize)
		err(4, "can't read mbr");
	memcpy(dp, &secbuf[DOSPARTOFF], sizeof(dp));

	for (i = 0; i < NDOSPART; i++) {
		if (dp[i].dp_typ == DOSPTYP_UNUSED)
			continue;
		if (dp[i].dp_typ == DOSPTYP_FAT16L ||
		    dp[i].dp_typ == DOSPTYP_FAT32L ||
		    dp[i].dp_typ == DOSPTYP_EFISYS)
			start = dp[i].dp_start;
	}

	free(secbuf);

	if (start) {
		for (i = 0; i < MAXPARTITIONS; i++) {
			if (DL_GETPSIZE(&dl->d_partitions[i]) > 0 &&
			    DL_GETPOFFSET(&dl->d_partitions[i]) == start)
				return ('a' + i);
		}
	}

	return (-1);
}
