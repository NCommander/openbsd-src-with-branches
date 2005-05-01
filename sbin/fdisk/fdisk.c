/*	$OpenBSD: fdisk.c,v 1.39 2004/08/03 09:22:03 otto Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include "disk.h"
#include "user.h"

#define _PATH_MBR _PATH_BOOTDIR "mbr"
static unsigned char builtin_mbr[] = {
#include "mbrcode.h"
};

static void
usage(void)
{
	extern char * __progname;

	fprintf(stderr, "usage: %s "
	    "[-ieu] [-c cylinders -h heads -s sectors] [-f mbrfile] device\n"
	    "\t-i: initialize disk with virgin MBR\n"
	    "\t-u: update MBR code, preserve partition table\n"
	    "\t-e: edit MBRs on disk interactively\n"
	    "\t-f: specify non-standard MBR template\n"
	    "\t-chs: specify disk geometry\n"
	    "`disk' may be of the forms: sd0 or /dev/rsd0c.\n",
	    __progname);
	exit(1);
}


int
main(int argc, char *argv[])
{
	int ch, fd;
	int i_flag = 0, m_flag = 0, u_flag = 0;
	int c_arg = 0, h_arg = 0, s_arg = 0;
	disk_t disk;
	DISK_metrics *usermetrics;
	char *mbrfile = _PATH_MBR;
	mbr_t mbr;
	char mbr_buf[DEV_BSIZE];

	while ((ch = getopt(argc, argv, "ieuf:c:h:s:")) != -1) {
		switch(ch) {
		case 'i':
			i_flag = 1;
			break;
		case 'u':
			u_flag = 1;
			break;
		case 'e':
			m_flag = 1;
			break;
		case 'f':
			mbrfile = optarg;
			break;
		case 'c':
			c_arg = atoi(optarg);
			if (c_arg < 1 || c_arg > 262144)
				errx(1, "Cylinder argument out of range "
				    "[1..262144].");
			break;
		case 'h':
			h_arg = atoi(optarg);
			if (h_arg < 1 || h_arg > 256)
				errx(1, "Head argument out of range "
				    "[1..256].");
			break;
		case 's':
			s_arg = atoi(optarg);
			if (s_arg < 1 || s_arg > 63)
				errx(1, "Sector argument out of range "
				    "[1..63].");
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Argument checking */
	if (argc != 1)
		usage();
	else
		disk.name = argv[0];

	/* Put in supplied geometry if there */
	if (c_arg | h_arg | s_arg) {
		usermetrics = malloc(sizeof(DISK_metrics));
		if (usermetrics != NULL) {
			if (c_arg && h_arg && s_arg) {
				usermetrics->cylinders = c_arg;
				usermetrics->heads = h_arg;
				usermetrics->sectors = s_arg;
				usermetrics->size = c_arg * h_arg * s_arg;
			} else
				errx(1, "Please specify a full geometry with [-chs].");
		}
	} else
		usermetrics = NULL;

	/* Get the geometry */
	disk.real = NULL;
	if (DISK_getmetrics(&disk, usermetrics))
		errx(1, "Can't get disk geometry, please use [-chs] to specify.");


	/* Print out current MBRs on disk */
	if ((i_flag + u_flag + m_flag) == 0)
		exit(USER_print_disk(&disk));

	/* Parse mbr template, to pass on later */
	if ((fd = open(mbrfile, O_RDONLY)) == -1) {
		warn("%s", mbrfile);
		warnx("using builtin MBR");
		memcpy(mbr_buf, builtin_mbr, sizeof(mbr_buf));
	} else {
		MBR_read(fd, 0, mbr_buf);
		close(fd);
	}
	MBR_parse(&disk, mbr_buf, 0, 0, &mbr);

	/* Now do what we are supposed to */
	if (i_flag || u_flag)
		USER_init(&disk, &mbr, u_flag);

	if (m_flag)
		USER_modify(&disk, &mbr, 0, 0);

	return (0);
}

