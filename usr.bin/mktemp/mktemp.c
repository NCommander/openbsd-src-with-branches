/*	$OpenBSD: mktemp.c,v 1.1 1996/11/21 07:59:33 millert Exp $	*/

/*
 * Copyright (c) 1996 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *	This product includes software developed by Todd C. Miller.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint                                                              
static char rcsid[] = "$OpenBSD: mktemp.c,v 1.1 1996/11/21 07:59:33 millert Exp $";
#endif /* not lint */                                                        

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

extern char *__progname;

void
usage()
{
	(void) fprintf(stderr, "Usage: %s [-u] [-q] template\n", __progname);
	exit(1);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	char *template;
	int ch, uflag = 0, qflag = 0;

	while ((ch = getopt(argc, argv, "uq")) != -1)
		switch(ch) {
		case 'u':
			uflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case '?':
		default:
			usage();
	}

	if (argc - optind != 1)
		usage();

	if ((template = strdup(argv[optind])) == NULL) {
		if (qflag)
			exit(1);
		else
			errx(1, "Cannot allocate memory");
	}

	if (mkstemp(template) < 0) {
		if (qflag)
			exit(1);
		else
			err(1, "Cannot create temp file %s", template);
	}

	if (uflag)
		(void) unlink(template);

	(void) puts(template);
	free(template);

	exit(0);
}
