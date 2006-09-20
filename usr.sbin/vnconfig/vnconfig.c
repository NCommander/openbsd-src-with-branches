/*	$OpenBSD: vnconfig.c,v 1.18 2006/07/01 07:36:27 jmc Exp $	*/
/*
 * Copyright (c) 1993 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: vnconfig.c 1.1 93/12/15$
 *
 *	@(#)vnconfig.c	8.1 (Berkeley) 12/15/93
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <dev/vndioctl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "pkcs5_pbkdf2.h"

#define DEFAULT_VND	"vnd0"

#define VND_CONFIG	1
#define VND_UNCONFIG	2
#define VND_GET		3

int verbose = 0;

__dead void usage(void);
int config(char *, char *, int, char *, size_t);
int getinfo(const char *);

int
main(int argc, char **argv)
{
	int ch, rv, rounds, action = VND_CONFIG;
	char *key = NULL;
	size_t keylen = 0;
	const char *errstr;

	while ((ch = getopt(argc, argv, "cluvK:k")) != -1) {
		switch (ch) {
		case 'c':
			action = VND_CONFIG;
			break;
		case 'l':
			action = VND_GET;
			break;
		case 'u':
			action = VND_UNCONFIG;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'K':
		{
			char keybuf[128];
			char saltbuf[128];
			char saltfilebuf[PATH_MAX];
			char *saltfile;

			rounds = strtonum(optarg, 1000, INT_MAX, &errstr);
			if (errstr)
				err(1, "rounds: %s", errstr);
			key = getpass("Encryption key: ");
			if (!key || strlen(key) == 0)
				errx(1, "Need an encryption key");
			strncpy(keybuf, key, sizeof(keybuf));
			printf("Salt file: ");
			fflush(stdout);
			saltfile = fgets(saltfilebuf, sizeof(saltfilebuf),
			    stdin);
			if (!saltfile || saltfile[0] == '\n') {
				warnx("Skipping salt file, insecure");
				saltfile = 0;
			} else {
				size_t len = strlen(saltfile);
				if (saltfile[len - 1] == '\n')
					saltfile[len - 1] = 0;
			}
			if (saltfile) {
				int fd;
				
				fd = open(saltfile, O_RDONLY);
				if (fd == -1) {
					int *s;

					fprintf(stderr, "Salt file not found, attempting to create one\n");
					fd = open(saltfile,
					    O_RDWR|O_CREAT|O_EXCL, 0600);
					if (fd == -1)
						err(1, "Unable to create salt file: '%s'", saltfile);
					for (s = (int *)saltbuf; s <
					    (int *)(saltbuf + sizeof(saltbuf));
					    s++)
						*s = arc4random();
					if (write(fd, saltbuf, sizeof(saltbuf))
					    != sizeof(saltbuf))
						err(1, "Unable to write salt file: '%s'", key);
					fprintf(stderr, "Salt file created as '%s'\n", saltfile);
				} else {
					if (read(fd, saltbuf, sizeof(saltbuf))
					    != sizeof(saltbuf))
						err(1, "Unable to read salt file: '%s'", saltfile);
				}
				close(fd);
			} else {
				memset(saltbuf, 0, sizeof(saltbuf));
			}
			if (pkcs5_pbkdf2((u_int8_t**)&key, 128, keybuf,
			    sizeof(keybuf), saltbuf, sizeof(saltbuf),
			    rounds, 0))
				errx(1, "pkcs5_pbkdf2 failed");
			keylen = 128;
			break;
		}
		case 'k':
			key = getpass("Encryption key: ");
			keylen = strlen(key);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (action == VND_CONFIG && argc == 2)
		rv = config(argv[0], argv[1], action, key, keylen);
	else if (action == VND_UNCONFIG && argc == 1)
		rv = config(argv[0], NULL, action, key, keylen);
	else if (action == VND_GET)
		rv = getinfo(argc ? argv[0] : NULL);
	else
		usage();

	exit(rv);
}

int
getinfo(const char *vname)
{
	int vd, print_all = 0;
	struct vnd_user vnu;

	if (vname == NULL) {
		vname = DEFAULT_VND;
		print_all = 1;
	}

	vd = opendev((char *)vname, O_RDONLY, OPENDEV_PART, NULL);
	if (vd < 0)
		err(1, "open: %s", vname);

	vnu.vnu_unit = -1;

query:
	if (ioctl(vd, VNDIOCGET, &vnu) == -1) {
		close(vd);
		return (!(errno == ENXIO && print_all));
	}

	fprintf(stdout, "vnd%d: ", vnu.vnu_unit);

	if (!vnu.vnu_ino)
		fprintf(stdout, "not in use\n");
	else
		fprintf(stdout, "covering %s on %s, inode %d\n", vnu.vnu_file,
		    devname(vnu.vnu_dev, S_IFBLK), vnu.vnu_ino);

	if (print_all) {
		vnu.vnu_unit++;
		goto query;
	}

	close(vd);

	return (0);
}

int
config(char *dev, char *file, int action, char *key, size_t keylen)
{
	struct vnd_ioctl vndio;
	FILE *f;
	char *rdev;
	int rv;

	if (opendev(dev, O_RDONLY, OPENDEV_PART, &rdev) < 0)
		err(4, "%s", rdev);
	f = fopen(rdev, "r");
	if (f == NULL) {
		warn("%s", rdev);
		rv = -1;
		goto out;
	}
	vndio.vnd_file = file;
	vndio.vnd_key = (u_char *)key;
	vndio.vnd_keylen = keylen;

	/*
	 * Clear (un-configure) the device
	 */
	if (action == VND_UNCONFIG) {
		rv = ioctl(fileno(f), VNDIOCCLR, &vndio);
		if (rv)
			warn("VNDIOCCLR");
		else if (verbose)
			printf("%s: cleared\n", dev);
	}
	/*
	 * Configure the device
	 */
	if (action == VND_CONFIG) {
		rv = ioctl(fileno(f), VNDIOCSET, &vndio);
		if (rv)
			warn("VNDIOCSET");
		else if (verbose)
			printf("%s: %llu bytes on %s\n", dev, vndio.vnd_size,
			    file);
	}

	fclose(f);
	fflush(stdout);
 out:
	if (key)
		memset(key, 0, keylen);
	return (rv < 0);
}

__dead void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-ckluv] [-K rounds] rawdev regular_file\n",
	    __progname);
	exit(1);
}
