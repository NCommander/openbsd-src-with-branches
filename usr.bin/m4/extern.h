/*	$OpenBSD: extern.h,v 1.27 2001/09/27 11:40:33 espie Exp $	*/
/*	$NetBSD: extern.h,v 1.3 1996/01/13 23:25:24 pk Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ozan Yigit at York University.
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
 *
 *	@(#)extern.h	8.1 (Berkeley) 6/6/93
 */

/* eval.c */
extern void	eval __P((const char *[], int, int));
extern void	dodefine __P((const char *, const char *));
extern unsigned long expansion_id;

/* expr.c */
extern int	expr __P((const char *));

/* gnum4.c */
extern void 	addtoincludepath __P((const char *));
extern struct input_file *fopen_trypath __P((struct input_file *, const char *));
extern void doindir __P((const char *[], int));
extern void dobuiltin __P((const char *[], int));
extern void dopatsubst __P((const char *[], int));
extern void doregexp __P((const char *[], int));

extern void doprintlineno __P((struct input_file *));
extern void doprintfilename __P((struct input_file *));

extern void doesyscmd __P((const char *));
 

/* look.c */
extern ndptr	addent __P((const char *));
extern unsigned	hash __P((const char *));
extern ndptr	lookup __P((const char *));
extern void	remhash __P((const char *, int));

/* main.c */
extern void outputstr __P((const char *));
extern int builtin_type __P((const char *));
extern char *builtin_realname __P((int));

/* misc.c */
extern void	chrsave __P((int));
extern char 	*compute_prevep __P((void));
extern void	getdiv __P((int));
extern ptrdiff_t indx __P((const char *, const char *));
extern void 	initspaces __P((void));
extern void	killdiv __P((void));
extern void	onintr __P((int));
extern void	pbnum __P((int));
extern void	pbunsigned __P((unsigned long));
extern void	pbstr __P((const char *));
extern void	putback __P((int));
extern void	*xalloc __P((size_t));
extern char	*xstrdup __P((const char *));
extern void	usage __P((void));
extern void	resizedivs __P((int));
extern size_t	buffer_mark __P((void));
extern void	dump_buffer __P((FILE *, size_t));

extern int 	obtain_char __P((struct input_file *));
extern void	set_input __P((struct input_file *, FILE *, const char *));
extern void	release_input __P((struct input_file *));

/* speeded-up versions of chrsave/putback */
#define PUTBACK(c)				\
	do {					\
		if (bp >= endpbb)		\
			enlarge_bufspace();	\
		*bp++ = (c);			\
	} while(0)
	
#define CHRSAVE(c)				\
	do {					\
		if (ep >= endest)		\
			enlarge_strspace();	\
		*ep++ = (c);			\
	} while(0)

/* and corresponding exposure for local symbols */
extern void enlarge_bufspace __P((void));
extern void enlarge_strspace __P((void));
extern char *endpbb;
extern char *endest;

/* trace.c */
extern void mark_traced __P((const char *, int));
extern int is_traced __P((const char *));
extern void trace_file __P((const char *));
extern ssize_t trace __P((const char **, int, struct input_file *));
extern void finish_trace __P((size_t));
extern int traced_macros;
extern void set_trace_flags __P((const char *));
extern FILE *traceout;

extern ndptr hashtab[];		/* hash table for macros etc. */
extern stae *mstack;		/* stack of m4 machine */
extern char *sstack;		/* shadow stack, for string space extension */
extern FILE *active;		/* active output file pointer */
extern struct input_file infile[];/* input file stack (0=stdin) */
extern FILE **outfile;		/* diversion array(0=bitbucket) */
extern int maxout;		/* maximum number of diversions */
extern int fp; 			/* m4 call frame pointer */
extern int ilevel;		/* input file stack pointer */
extern int oindex;		/* diversion index. */
extern int sp;			/* current m4 stack pointer */
extern char *bp;		/* first available character */
extern char *buf;		/* push-back buffer */
extern char *bufbase;		/* buffer base for this ilevel */
extern char *bbase[];		/* buffer base per ilevel */
extern char ecommt[MAXCCHARS+1];/* end character for comment */
extern char *ep;		/* first free char in strspace */
extern char lquote[MAXCCHARS+1];/* left quote character (`) */
extern char *m4wraps;		/* m4wrap string default. */
extern char *null;		/* as it says.. just a null. */
extern char rquote[MAXCCHARS+1];/* right quote character (') */
extern char scommt[MAXCCHARS+1];/* start character for comment */

extern int mimic_gnu;		/* behaves like gnu-m4 */
