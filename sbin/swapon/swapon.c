/*	$OpenBSD: swapon.c,v 1.7 1995/03/18 15:01:18 cgd Exp $	*/
/*	$NetBSD: swapon.c,v 1.7 1995/03/18 15:01:18 cgd Exp $	*/

/*
 * Copyright (c) 1980, 1993
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)swapon.c	8.1 (Berkeley) 6/5/93";
#else
static char rcsid[] = "$OpenBSD: swapon.c,v 1.7 1995/03/18 15:01:18 cgd Exp $";
#endif
#endif /* not lint */

#include <fstab.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int add __P((char *, int));
void usage ();

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	register struct fstab *fsp;
	register int stat;
	int ch, doall;

	doall = 0;
	while ((ch = getopt(argc, argv, "a")) != EOF)
		switch((char)ch) {
		case 'a':
			doall = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;

	stat = 0;
	if (doall)
		while (fsp = getfsent()) {
			if (strcmp(fsp->fs_type, FSTAB_SW))
				continue;
			if (add(fsp->fs_spec, 1))
				stat = 1;
			else
				printf("swapon: adding %s as swap device\n",
				    fsp->fs_spec);
		}
	else if (!*argv)
		usage();
	for (; *argv; ++argv)
		stat |= add(*argv, 0);
	exit(stat);
}

int
add(name, ignoreebusy)
	char *name;
	int ignoreebusy;
{
	extern int errno;

	if (swapon(name) == -1) {
		switch (errno) {
		case EINVAL:
			fprintf(stderr, "swapon: %s: device not configured\n",
			    name);
			break;
		case EBUSY:
			if (!ignoreebusy)
				fprintf(stderr,
				    "swapon: %s: device already in use\n",
				     name);
			break;
		default:
			fprintf(stderr, "swapon: %s: ", name);
			perror((char *)NULL);
			break;
		}
		return(1);
	}
	return(0);
}

void
usage()
{
	fprintf(stderr, "usage: swapon [-a] [special_file ...]\n");
	exit(1);
}
