/*	$NetBSD: unstr.c,v 1.3 1995/03/23 08:29:00 cgd Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ken Arnold.
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
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)unstr.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*
 *	This program un-does what "strfile" makes, thereby obtaining the
 * original file again.  This can be invoked with the name of the output
 * file, the input file, or both. If invoked with only a single argument
 * ending in ".dat", it is pressumed to be the input file and the output
 * file will be the same stripped of the ".dat".  If the single argument
 * doesn't end in ".dat", then it is presumed to be the output file, and
 * the input file is that name prepended by a ".dat".  If both are given
 * they are treated literally as the input and output files.
 *
 *	Ken Arnold		Aug 13, 1978
 */

# include	<machine/endian.h>
# include	<sys/param.h>
# include	"strfile.h"
# include	<stdio.h>
# include	<ctype.h>

# ifndef MAXPATHLEN
# define	MAXPATHLEN	1024
# endif	/* MAXPATHLEN */

char	*Infile,			/* name of input file */
	Datafile[MAXPATHLEN],		/* name of data file */
	Delimch;			/* delimiter character */

FILE	*Inf, *Dataf;

char	*strcat(), *strcpy();

/* ARGSUSED */
main(ac, av)
int	ac;
char	**av;
{
	static STRFILE	tbl;		/* description table */

	getargs(av);
	if ((Inf = fopen(Infile, "r")) == NULL) {
		perror(Infile);
		exit(1);
	}
	if ((Dataf = fopen(Datafile, "r")) == NULL) {
		perror(Datafile);
		exit(1);
	}
	(void) fread(&tbl.str_version,  sizeof(tbl.str_version),  1, Dataf);
	(void) fread(&tbl.str_numstr,   sizeof(tbl.str_numstr),   1, Dataf);
	(void) fread(&tbl.str_longlen,  sizeof(tbl.str_longlen),  1, Dataf);
	(void) fread(&tbl.str_shortlen, sizeof(tbl.str_shortlen), 1, Dataf);
	(void) fread(&tbl.str_flags,    sizeof(tbl.str_flags),    1, Dataf);
	(void) fread( tbl.stuff,	sizeof(tbl.stuff),	  1, Dataf);
	if (!(tbl.str_flags & (STR_ORDERED | STR_RANDOM))) {
		fprintf(stderr, "nothing to do -- table in file order\n");
		exit(1);
	}
	Delimch = tbl.str_delim;
	order_unstr(&tbl);
	(void) fclose(Inf);
	(void) fclose(Dataf);
	exit(0);
}

getargs(av)
register char	*av[];
{
	if (!*++av) {
		(void) fprintf(stderr, "usage: unstr datafile\n");
		exit(1);
	}
	Infile = *av;
	(void) strcpy(Datafile, Infile);
	(void) strcat(Datafile, ".dat");
}

order_unstr(tbl)
register STRFILE	*tbl;
{
	register int	i;
	register char	*sp;
	auto int32_t	pos;
	char		buf[BUFSIZ];

	for (i = 0; i < tbl->str_numstr; i++) {
		(void) fread((char *) &pos, 1, sizeof pos, Dataf);
		(void) fseek(Inf, ntohl(pos), 0);
		if (i != 0)
			(void) printf("%c\n", Delimch);
		for (;;) {
			sp = fgets(buf, sizeof buf, Inf);
			if (sp == NULL || STR_ENDSTRING(sp, *tbl))
				break;
			else
				fputs(sp, stdout);
		}
	}
}
