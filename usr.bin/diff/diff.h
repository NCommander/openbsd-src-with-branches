/*	$OpenBSD: diff.h,v 1.18 2003/07/09 00:07:44 millert Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)diff.h	8.1 (Berkeley) 6/6/93
 */

/*
 * Output format options
 */
#define	D_NORMAL	0	/* Normal output */
#define	D_EDIT		-1	/* Editor script out */
#define	D_REVERSE	1	/* Reverse editor script */
#define	D_CONTEXT	2	/* Diff with context */
#define	D_UNIFIED	3	/* Unified context diff */
#define	D_IFDEF		4	/* Diff with merged #ifdef's */
#define	D_NREVERSE	5	/* Reverse ed script with numbered
				   lines and no trailing . */
#define	D_BRIEF		6	/* Say if the files differ */

/*
 * Output flags
 */
#define	D_HEADER	1	/* Print a header/footer between files */
#define	D_EMPTY1	2	/* Treat first file as empty (/dev/null) */
#define	D_EMPTY2	4	/* Treat second file as empty (/dev/null) */

/*
 * Status values for print_status() and diffreg() return values
 */
#define	D_SAME		0	/* Files are the same */
#define	D_DIFFER	1	/* Files are different */
#define	D_BINARY	2	/* Binary files are different */
#define	D_COMMON	3	/* Subdirectory common to both dirs */
#define	D_ONLY		4	/* Only exists in one directory */
#define	D_MISMATCH	5	/* One path was a dir, the other a file */
#define	D_ERROR		6	/* An error ocurred */

struct excludes {
	char *pattern;
	struct excludes *next;
};

extern int	aflag, bflag, iflag, lflag, Nflag, Pflag, rflag, sflag,
		tflag, wflag;
extern int	format, context, status, anychange;
extern char	*start, *ifdefname, *diffargs;
extern struct	stat stb1, stb2;
extern struct	excludes *excludes_list;

char	*splice(char *, char *);
int	diffreg(char *, char *, int);
int	easprintf(char **, const char *, ...);
void	*emalloc(size_t);
void	*erealloc(void *, size_t);
void	diffdir(char *, char *);
void	print_status(int, char *, char *, char *);
