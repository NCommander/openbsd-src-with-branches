/*	$OpenBSD: extern.h,v 1.7 2002/02/16 21:27:07 millert Exp $	*/
/*	$NetBSD: extern.h,v 1.10 1995/05/21 13:38:27 mycroft Exp $	*/

/*-
 * Copyright (c) 1991, 1993, 1994
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
 *
 *	@(#)extern.h	8.3 (Berkeley) 4/2/94
 */

struct kinfo;
struct nlist;
struct var;
struct varent;

extern fixpt_t ccpu;
extern int eval, fscale, mempages, nlistread, rawcpu, maxslp;
extern int sumrusage, termwidth, totwidth;
extern VAR var[];
extern VARENT *vhead;

__BEGIN_DECLS
void	 command(KINFO *, VARENT *);
void	 cputime(KINFO *, VARENT *);
int	 donlist(void);
void	 evar(KINFO *, VARENT *);
void	 emulname(KINFO *, VARENT *);
void	 fmt_puts(char *, int *);
void	 fmt_putc(int, int *);
double	 getpcpu(KINFO *);
double	 getpmem(KINFO *);
void	 gname(KINFO *, VARENT *);
void	 logname(KINFO *, VARENT *);
void	 longtname(KINFO *, VARENT *);
void	 lstarted(KINFO *, VARENT *);
void	 maxrss(KINFO *, VARENT *);
void	 nlisterr(struct nlist *);
void	 p_rssize(KINFO *, VARENT *);
void	 pagein(KINFO *, VARENT *);
void	 parsefmt(char *);
void	 pcpu(KINFO *, VARENT *);
void	 pmem(KINFO *, VARENT *);
void	 pri(KINFO *, VARENT *);
void	 printheader(void);
void	 pvar(KINFO *, VARENT *);
void	 rgname(KINFO *, VARENT *);
void	 rssize(KINFO *, VARENT *);
void	 runame(KINFO *, VARENT *);
void	 rvar(KINFO *, VARENT *);
void	 showkey(void);
void	 started(KINFO *, VARENT *);
void	 state(KINFO *, VARENT *);
void	 tdev(KINFO *, VARENT *);
void	 tname(KINFO *, VARENT *);
void	 tsize(KINFO *, VARENT *);
void	 dsize(KINFO *, VARENT *);
void	 ssize(KINFO *, VARENT *);
void	 ucomm(KINFO *, VARENT *);
void	 uname(KINFO *, VARENT *);
void	 uvar(KINFO *, VARENT *);
void	 vsize(KINFO *, VARENT *);
void	 wchan(KINFO *, VARENT *);
__END_DECLS
