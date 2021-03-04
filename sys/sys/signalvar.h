/*	$OpenBSD: signalvar.h,v 1.45 2020/11/08 20:37:24 mpi Exp $	*/
/*	$NetBSD: signalvar.h,v 1.17 1996/04/22 01:23:31 christos Exp $	*/

/*
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
 *	@(#)signalvar.h	8.3 (Berkeley) 1/4/94
 */

#ifndef	_SYS_SIGNALVAR_H_		/* tmp for user.h */
#define	_SYS_SIGNALVAR_H_

/*
 * Kernel signal definitions and data structures,
 * not exported to user programs.
 */

/*
 * Process signal actions and state, needed only within the process
 * (not necessarily resident).
 */
struct	sigacts {
	sig_t	ps_sigact[NSIG];	/* disposition of signals */
	sigset_t ps_catchmask[NSIG];	/* signals to be blocked */
	sigset_t ps_sigonstack;		/* signals to take on sigstack */
	sigset_t ps_sigintr;		/* signals that interrupt syscalls */
	sigset_t ps_sigreset;		/* signals that reset when caught */
	sigset_t ps_siginfo;		/* signals that provide siginfo */
	sigset_t ps_sigignore;		/* signals being ignored */
	sigset_t ps_sigcatch;		/* signals being caught by user */
	int	ps_sigflags;		/* signal flags, below */
};

/* signal flags */
#define	SAS_NOCLDSTOP	0x01	/* No SIGCHLD when children stop. */
#define	SAS_NOCLDWAIT	0x02	/* No zombies if child dies */

/* additional signal action values, used only temporarily/internally */
#define	SIG_CATCH	(void (*)(int))2
#define	SIG_HOLD	(void (*)(int))3

/*
 * Check if process p has an unmasked signal pending.
 * Return mask of pending signals.
 */
#define SIGPENDING(p)							\
	(((p)->p_siglist | (p)->p_p->ps_siglist) & ~(p)->p_sigmask)

/*
 * Clear a pending signal from a process.
 */
#define CLRSIG(p, sig)	do {						\
	int __mask = sigmask(sig);					\
	atomic_clearbits_int(&(p)->p_siglist, __mask);			\
	atomic_clearbits_int(&(p)->p_p->ps_siglist, __mask);		\
} while (0)

/*
 * Signal properties and actions.
 * The array below categorizes the signals and their default actions
 * according to the following properties:
 */
#define	SA_KILL		0x01		/* terminates process by default */
#define	SA_CORE		0x02		/* ditto and coredumps */
#define	SA_STOP		0x04		/* suspend process */
#define	SA_TTYSTOP	0x08		/* ditto, from tty */
#define	SA_IGNORE	0x10		/* ignore by default */
#define	SA_CONT		0x20		/* continue if suspended */
#define	SA_CANTMASK	0x40		/* non-maskable, catchable */

#define	sigcantmask	(sigmask(SIGKILL) | sigmask(SIGSTOP))

#ifdef _KERNEL
enum signal_type { SPROCESS, STHREAD, SPROPAGATED };

struct sigio_ref;

/*
 * Machine-independent functions:
 */
int	coredump(struct proc *p);
void	execsigs(struct proc *p);
int	cursig(struct proc *p);
void	pgsigio(struct sigio_ref *sir, int sig, int checkctty);
void	pgsignal(struct pgrp *pgrp, int sig, int checkctty);
void	psignal(struct proc *p, int sig);
void	ptsignal(struct proc *p, int sig, enum signal_type type);
#define prsignal(pr,sig)	ptsignal((pr)->ps_mainproc, (sig), SPROCESS)
void	siginit(struct sigacts *);
void	trapsignal(struct proc *p, int sig, u_long code, int type,
	    union sigval val);
void	sigexit(struct proc *, int);
void	sigabort(struct proc *);
int	sigismasked(struct proc *, int);
int	sigonstack(size_t);
int	killpg1(struct proc *, int, int, int);

void	signal_init(void);

struct sigacts *sigactsinit(struct process *);
void	sigstkinit(struct sigaltstack *);
void	sigactsfree(struct process *);

/*
 * Machine-dependent functions:
 */
int	sendsig(sig_t _catcher, int _sig, sigset_t _mask, const siginfo_t *_si);
#endif	/* _KERNEL */
#endif	/* !_SYS_SIGNALVAR_H_ */
