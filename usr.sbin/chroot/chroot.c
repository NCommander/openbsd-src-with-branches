/*	$OpenBSD: chroot.c,v 1.6 2002/10/25 19:23:48 millert Exp $	*/
/*	$NetBSD: chroot.c,v 1.11 2001/04/06 02:34:04 lukem Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/cdefs.h>
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)chroot.c	8.1 (Berkeley) 6/9/93";
#else
static const char rcsid[] = "$OpenBSD: chroot.c,v 1.6 2002/10/25 19:23:48 millert Exp $";
#endif
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int		main(int, char **);
__dead void	usage(void);

int
main(int argc, char **argv)
{
	struct group	*gp;
	struct passwd	*pw;
	const char	*shell;
	char		*fulluser, *user, *group, *grouplist, *endp, *p;
	gid_t		gid, gidlist[NGROUPS_MAX];
	uid_t		uid;
	int		ch, gids;
	unsigned long	ul;

	gid = 0;
	uid = 0;
	gids = 0;
	user = fulluser = group = grouplist = NULL;
	while ((ch = getopt(argc, argv, "G:g:U:u:")) != -1) {
		switch(ch) {
		case 'U':
			fulluser = optarg;
			if (*fulluser == '\0')
				usage();
			break;
		case 'u':
			user = optarg;
			if (*user == '\0')
				usage();
			break;
		case 'g':
			group = optarg;
			if (*group == '\0')
				usage();
			break;
		case 'G':
			grouplist = optarg;
			if (*grouplist == '\0')
				usage();
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();
	if (fulluser && (user || group || grouplist))
		errx(1,
		    "the -U option may not be specified with any other option");

	if (group != NULL) {
		if ((gp = getgrnam(group)) != NULL)
			gid = gp->gr_gid;
		else if (isdigit((unsigned char)*group)) {
			errno = 0;
			ul = strtoul(group, &endp, 10);
			if (*endp != '\0' ||
			    (ul == ULONG_MAX && errno == ERANGE))
				errx(1, "invalid group ID `%s'", group);
			gid = (gid_t)ul;
		} else
			errx(1, "no such group `%s'", group);
		if (grouplist != NULL)
			gidlist[gids++] = gid;
		if (setgid(gid) != 0)
			err(1, "setgid");
	}

	while ((p = strsep(&grouplist, ",")) != NULL && gids < NGROUPS_MAX) {
		if (*p == '\0')
			continue;

		if ((gp = getgrnam(p)) != NULL)
			gidlist[gids] = gp->gr_gid;
		else if (isdigit((unsigned char)*p)) {
			errno = 0;
			ul = strtoul(p, &endp, 10);
			if (*endp != '\0' ||
			    (ul == ULONG_MAX && errno == ERANGE))
				errx(1, "invalid group ID `%s'", p);
			gidlist[gids] = (gid_t)ul;
		} else
			errx(1, "no such group `%s'", p);
		/*
		 * Ignore primary group if specified; we already added it above.
		 */
		if (group == NULL || gidlist[gids] != gid)
			gids++;
	}
	if (p != NULL && gids == NGROUPS_MAX)
		errx(1, "too many supplementary groups provided");
	if (gids && setgroups(gids, gidlist) != 0)
		err(1, "setgroups");

	if (user != NULL) {
		if ((pw = getpwnam(user)) != NULL)
			uid = pw->pw_uid;
		else if (isdigit((unsigned char)*user)) {
			errno = 0;
			ul = strtoul(user, &endp, 10);
			if (*endp != '\0' ||
			    (ul == ULONG_MAX && errno == ERANGE))
				errx(1, "invalid user ID `%s'", user);
			uid = (uid_t)ul;
		} else
			errx(1, "no such user `%s'", user);
	}

	if (fulluser != NULL) {
		if ((pw = getpwnam(fulluser)) == NULL)
			errx(1, "no such user `%s'", fulluser);
		uid = pw->pw_uid;
		gid = pw->pw_gid;
		if (setgid(gid) != 0)
			err(1, "setgid");
		if (initgroups(fulluser, gid) == -1)
			err(1, "initgroups");
	}

	if (chroot(argv[0]) != 0 || chdir("/") != 0)
		err(1, "%s", argv[0]);

	if ((user || fulluser) && setuid(uid) != 0)
		err(1, "setuid");

	if (argv[1]) {
		execvp(argv[1], &argv[1]);
		err(1, "%s", argv[1]);
	}

	if ((shell = getenv("SHELL")) == NULL)
		shell = _PATH_BSHELL;
	execlp(shell, shell, "-i", (char *)NULL);
	err(1, "%s", shell);
	/* NOTREACHED */
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-g group] [-G group,group,...] "
	    "[-u user] [-U user] newroot [command]\n", __progname);
	exit(1);
}
