/*	$OpenBSD: mount_ados.c,v 1.12 2003/07/03 22:41:40 tedu Exp $	*/
/*	$NetBSD: mount_ados.c,v 1.5 1996/04/13 01:30:59 jtc Exp $	*/

/*
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou.
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

#ifndef lint
static char rcsid[] = "$OpenBSD: mount_ados.c,v 1.12 2003/07/03 22:41:40 tedu Exp $";
#endif /* not lint */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mntopts.h"

const struct mntopt mopts[] = {
	MOPT_STDOPTS,
	{ NULL }
};

gid_t	a_gid(char *);
uid_t	a_uid(char *);
mode_t	a_mask(char *);
void	usage(void);

int
main(int argc, char *argv[])
{
	struct adosfs_args args;
	struct stat sb;
	int c, mntflags, set_gid, set_uid, set_mask;
	char *dev, dir[MAXPATHLEN];

	mntflags = set_gid = set_uid = set_mask = 0;
	(void)memset(&args, '\0', sizeof(args));

	while ((c = getopt(argc, argv, "u:g:m:o:")) != -1) {
		switch (c) {
		case 'u':
			args.uid = a_uid(optarg);
			set_uid = 1;
			break;
		case 'g':
			args.gid = a_gid(optarg);
			set_gid = 1;
			break;
		case 'm':
			args.mask = a_mask(optarg);
			set_mask = 1;
			break;
		case 'o':
			getmntopts(optarg, mopts, &mntflags);
			break;
		case '?':
		default:
			usage();
			break;
		}
	}

	if (optind + 2 != argc)
		usage();

	dev = argv[optind];
	if (realpath(argv[optind + 1], dir) == NULL)
		err(1, "realpath %s", argv[optind + 1]);

	args.fspec = dev;
	args.export_info.ex_root = -2;	/* unchecked anyway on DOS fs */
	if (mntflags & MNT_RDONLY)
		args.export_info.ex_flags = MNT_EXRDONLY;
	else
		args.export_info.ex_flags = 0;
	if (!set_gid || !set_uid || !set_mask) {
		if (stat(dir, &sb) == -1)
			err(1, "stat %s", dir);

		if (!set_uid)
			args.uid = sb.st_uid;
		if (!set_gid)
			args.gid = sb.st_gid;
		if (!set_mask)
			args.mask = sb.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
	}

	if (mount(MOUNT_ADOSFS, dir, mntflags, &args) < 0) {
		if (errno == EOPNOTSUPP)
			errx(1, "%s: Filesystem not supported by kernel", dir);
		else
			err(1, "%s", dir);
	}
	exit (0);
}

gid_t
a_gid(char *s)
{
	struct group *gr;
	char *gname;
	gid_t gid;

	if ((gr = getgrnam(s)) != NULL)
		gid = gr->gr_gid;
	else {
		for (gname = s; *s && isdigit(*s); ++s);
		if (!*s)
			gid = atoi(gname);
		else
			errx(1, "unknown group id: %s", gname);
	}
	return (gid);
}

uid_t
a_uid(char *s)
{
	struct passwd *pw;
	char *uname;
	uid_t uid;

	if ((pw = getpwnam(s)) != NULL)
		uid = pw->pw_uid;
	else {
		for (uname = s; *s && isdigit(*s); ++s);
		if (!*s)
			uid = atoi(uname);
		else
			errx(1, "unknown user id: %s", uname);
	}
	return (uid);
}

mode_t
a_mask(char *s)
{
	int done, rv;
	char *ep;

	done = 0;
	if (*s >= '0' && *s <= '7') {
		done = 1;
		rv = strtol(optarg, &ep, 8);
	}
	if (!done || rv < 0 || *ep)
		errx(1, "invalid file mode: %s", s);
	return (rv);
}

void
usage(void)
{

	fprintf(stderr,
	    "usage: mount_ados [-o options] [-u user] [-g group] [-m mask] bdev dir\n");
	exit(1);
}
