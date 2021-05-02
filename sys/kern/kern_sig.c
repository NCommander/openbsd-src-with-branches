/*	$OpenBSD: kern_sig.c,v 1.278 2021/03/12 10:13:28 mpi Exp $	*/
/*	$NetBSD: kern_sig.c,v 1.54 1996/04/22 01:38:32 christos Exp $	*/

/*
 * Copyright (c) 1997 Theo de Raadt. All rights reserved. 
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_sig.c	8.7 (Berkeley) 4/18/94
 */

#include <sys/param.h>
#include <sys/signalvar.h>
#include <sys/resourcevar.h>
#include <sys/queue.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/event.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/acct.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/wait.h>
#include <sys/ktrace.h>
#include <sys/stat.h>
#include <sys/core.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/ptrace.h>
#include <sys/sched.h>
#include <sys/user.h>
#include <sys/syslog.h>
#include <sys/ttycom.h>
#include <sys/pledge.h>
#include <sys/witness.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>
#include <machine/tcb.h>

int	filt_sigattach(struct knote *kn);
void	filt_sigdetach(struct knote *kn);
int	filt_signal(struct knote *kn, long hint);

const struct filterops sig_filtops = {
	.f_flags	= 0,
	.f_attach	= filt_sigattach,
	.f_detach	= filt_sigdetach,
	.f_event	= filt_signal,
};

const int sigprop[NSIG + 1] = {
	0,			/* unused */
	SA_KILL,		/* SIGHUP */
	SA_KILL,		/* SIGINT */
	SA_KILL|SA_CORE,	/* SIGQUIT */
	SA_KILL|SA_CORE,	/* SIGILL */
	SA_KILL|SA_CORE,	/* SIGTRAP */
	SA_KILL|SA_CORE,	/* SIGABRT */
	SA_KILL|SA_CORE,	/* SIGEMT */
	SA_KILL|SA_CORE,	/* SIGFPE */
	SA_KILL,		/* SIGKILL */
	SA_KILL|SA_CORE,	/* SIGBUS */
	SA_KILL|SA_CORE,	/* SIGSEGV */
	SA_KILL|SA_CORE,	/* SIGSYS */
	SA_KILL,		/* SIGPIPE */
	SA_KILL,		/* SIGALRM */
	SA_KILL,		/* SIGTERM */
	SA_IGNORE,		/* SIGURG */
	SA_STOP,		/* SIGSTOP */
	SA_STOP|SA_TTYSTOP,	/* SIGTSTP */
	SA_IGNORE|SA_CONT,	/* SIGCONT */
	SA_IGNORE,		/* SIGCHLD */
	SA_STOP|SA_TTYSTOP,	/* SIGTTIN */
	SA_STOP|SA_TTYSTOP,	/* SIGTTOU */
	SA_IGNORE,		/* SIGIO */
	SA_KILL,		/* SIGXCPU */
	SA_KILL,		/* SIGXFSZ */
	SA_KILL,		/* SIGVTALRM */
	SA_KILL,		/* SIGPROF */
	SA_IGNORE,		/* SIGWINCH  */
	SA_IGNORE,		/* SIGINFO */
	SA_KILL,		/* SIGUSR1 */
	SA_KILL,		/* SIGUSR2 */
	SA_IGNORE,		/* SIGTHR */
};

#define	CONTSIGMASK	(sigmask(SIGCONT))
#define	STOPSIGMASK	(sigmask(SIGSTOP) | sigmask(SIGTSTP) | \
			    sigmask(SIGTTIN) | sigmask(SIGTTOU))

void setsigvec(struct proc *, int, struct sigaction *);

void proc_stop(struct proc *p, int);
void proc_stop_sweep(void *);
void *proc_stop_si;

void postsig(struct proc *, int);
int cansignal(struct proc *, struct process *, int);

struct pool sigacts_pool;	/* memory pool for sigacts structures */

void sigio_del(struct sigiolst *);
void sigio_unlink(struct sigio_ref *, struct sigiolst *);
struct mutex sigio_lock = MUTEX_INITIALIZER(IPL_HIGH);

/*
 * Can thread p, send the signal signum to process qr?
 */
int
cansignal(struct proc *p, struct process *qr, int signum)
{
	struct process *pr = p->p_p;
	struct ucred *uc = p->p_ucred;
	struct ucred *quc = qr->ps_ucred;

	if (uc->cr_uid == 0)
		return (1);		/* root can always signal */

	if (pr == qr)
		return (1);		/* process can always signal itself */

	/* optimization: if the same creds then the tests below will pass */
	if (uc == quc)
		return (1);

	if (signum == SIGCONT && qr->ps_session == pr->ps_session)
		return (1);		/* SIGCONT in session */

	/*
	 * Using kill(), only certain signals can be sent to setugid
	 * child processes
	 */
	if (qr->ps_flags & PS_SUGID) {
		switch (signum) {
		case 0:
		case SIGKILL:
		case SIGINT:
		case SIGTERM:
		case SIGALRM:
		case SIGSTOP:
		case SIGTTIN:
		case SIGTTOU:
		case SIGTSTP:
		case SIGHUP:
		case SIGUSR1:
		case SIGUSR2:
			if (uc->cr_ruid == quc->cr_ruid ||
			    uc->cr_uid == quc->cr_ruid)
				return (1);
		}
		return (0);
	}

	if (uc->cr_ruid == quc->cr_ruid ||
	    uc->cr_ruid == quc->cr_svuid ||
	    uc->cr_uid == quc->cr_ruid ||
	    uc->cr_uid == quc->cr_svuid)
		return (1);
	return (0);
}

/*
 * Initialize signal-related data structures.
 */
void
signal_init(void)
{
	proc_stop_si = softintr_establish(IPL_SOFTCLOCK, proc_stop_sweep,
	    NULL);
	if (proc_stop_si == NULL)
		panic("signal_init failed to register softintr");

	pool_init(&sigacts_pool, sizeof(struct sigacts), 0, IPL_NONE,
	    PR_WAITOK, "sigapl", NULL);
}

/*
 * Create an initial sigacts structure, using the same signal state
 * as pr.
 */
struct sigacts *
sigactsinit(struct process *pr)
{
	struct sigacts *ps;

	ps = pool_get(&sigacts_pool, PR_WAITOK);
	memcpy(ps, pr->ps_sigacts, sizeof(struct sigacts));
	return (ps);
}

/*
 * Initialize a new sigaltstack structure.
 */
void
sigstkinit(struct sigaltstack *ss)
{
	ss->ss_flags = SS_DISABLE;
	ss->ss_size = 0;
	ss->ss_sp = 0;
}

/*
 * Release a sigacts structure.
 */
void
sigactsfree(struct process *pr)
{
	struct sigacts *ps = pr->ps_sigacts;

	pr->ps_sigacts = NULL;

	pool_put(&sigacts_pool, ps);
}

int
sys_sigaction(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigaction_args /* {
		syscallarg(int) signum;
		syscallarg(const struct sigaction *) nsa;
		syscallarg(struct sigaction *) osa;
	} */ *uap = v;
	struct sigaction vec;
#ifdef KTRACE
	struct sigaction ovec;
#endif
	struct sigaction *sa;
	const struct sigaction *nsa;
	struct sigaction *osa;
	struct sigacts *ps = p->p_p->ps_sigacts;
	int signum;
	int bit, error;

	signum = SCARG(uap, signum);
	nsa = SCARG(uap, nsa);
	osa = SCARG(uap, osa);

	if (signum <= 0 || signum >= NSIG ||
	    (nsa && (signum == SIGKILL || signum == SIGSTOP)))
		return (EINVAL);
	sa = &vec;
	if (osa) {
		sa->sa_handler = ps->ps_sigact[signum];
		sa->sa_mask = ps->ps_catchmask[signum];
		bit = sigmask(signum);
		sa->sa_flags = 0;
		if ((ps->ps_sigonstack & bit) != 0)
			sa->sa_flags |= SA_ONSTACK;
		if ((ps->ps_sigintr & bit) == 0)
			sa->sa_flags |= SA_RESTART;
		if ((ps->ps_sigreset & bit) != 0)
			sa->sa_flags |= SA_RESETHAND;
		if ((ps->ps_siginfo & bit) != 0)
			sa->sa_flags |= SA_SIGINFO;
		if (signum == SIGCHLD) {
			if ((ps->ps_sigflags & SAS_NOCLDSTOP) != 0)
				sa->sa_flags |= SA_NOCLDSTOP;
			if ((ps->ps_sigflags & SAS_NOCLDWAIT) != 0)
				sa->sa_flags |= SA_NOCLDWAIT;
		}
		if ((sa->sa_mask & bit) == 0)
			sa->sa_flags |= SA_NODEFER;
		sa->sa_mask &= ~bit;
		error = copyout(sa, osa, sizeof (vec));
		if (error)
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ovec = vec;
#endif
	}
	if (nsa) {
		error = copyin(nsa, sa, sizeof (vec));
		if (error)
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrsigaction(p, sa);
#endif
		setsigvec(p, signum, sa);
	}
#ifdef KTRACE
	if (osa && KTRPOINT(p, KTR_STRUCT))
		ktrsigaction(p, &ovec);
#endif
	return (0);
}

void
setsigvec(struct proc *p, int signum, struct sigaction *sa)
{
	struct sigacts *ps = p->p_p->ps_sigacts;
	int bit;
	int s;

	bit = sigmask(signum);
	/*
	 * Change setting atomically.
	 */
	s = splhigh();
	ps->ps_sigact[signum] = sa->sa_handler;
	if ((sa->sa_flags & SA_NODEFER) == 0)
		sa->sa_mask |= sigmask(signum);
	ps->ps_catchmask[signum] = sa->sa_mask &~ sigcantmask;
	if (signum == SIGCHLD) {
		if (sa->sa_flags & SA_NOCLDSTOP)
			atomic_setbits_int(&ps->ps_sigflags, SAS_NOCLDSTOP);
		else
			atomic_clearbits_int(&ps->ps_sigflags, SAS_NOCLDSTOP);
		/*
		 * If the SA_NOCLDWAIT flag is set or the handler
		 * is SIG_IGN we reparent the dying child to PID 1
		 * (init) which will reap the zombie.  Because we use
		 * init to do our dirty work we never set SAS_NOCLDWAIT
		 * for PID 1.
		 * XXX exit1 rework means this is unnecessary?
		 */
		if (initprocess->ps_sigacts != ps &&
		    ((sa->sa_flags & SA_NOCLDWAIT) ||
		    sa->sa_handler == SIG_IGN))
			atomic_setbits_int(&ps->ps_sigflags, SAS_NOCLDWAIT);
		else
			atomic_clearbits_int(&ps->ps_sigflags, SAS_NOCLDWAIT);
	}
	if ((sa->sa_flags & SA_RESETHAND) != 0)
		ps->ps_sigreset |= bit;
	else
		ps->ps_sigreset &= ~bit;
	if ((sa->sa_flags & SA_SIGINFO) != 0)
		ps->ps_siginfo |= bit;
	else
		ps->ps_siginfo &= ~bit;
	if ((sa->sa_flags & SA_RESTART) == 0)
		ps->ps_sigintr |= bit;
	else
		ps->ps_sigintr &= ~bit;
	if ((sa->sa_flags & SA_ONSTACK) != 0)
		ps->ps_sigonstack |= bit;
	else
		ps->ps_sigonstack &= ~bit;
	/*
	 * Set bit in ps_sigignore for signals that are set to SIG_IGN,
	 * and for signals set to SIG_DFL where the default is to ignore.
	 * However, don't put SIGCONT in ps_sigignore,
	 * as we have to restart the process.
	 */
	if (sa->sa_handler == SIG_IGN ||
	    (sigprop[signum] & SA_IGNORE && sa->sa_handler == SIG_DFL)) {
		atomic_clearbits_int(&p->p_siglist, bit);
		atomic_clearbits_int(&p->p_p->ps_siglist, bit);
		if (signum != SIGCONT)
			ps->ps_sigignore |= bit;	/* easier in psignal */
		ps->ps_sigcatch &= ~bit;
	} else {
		ps->ps_sigignore &= ~bit;
		if (sa->sa_handler == SIG_DFL)
			ps->ps_sigcatch &= ~bit;
		else
			ps->ps_sigcatch |= bit;
	}
	splx(s);
}

/*
 * Initialize signal state for process 0;
 * set to ignore signals that are ignored by default.
 */
void
siginit(struct sigacts *ps)
{
	int i;

	for (i = 0; i < NSIG; i++)
		if (sigprop[i] & SA_IGNORE && i != SIGCONT)
			ps->ps_sigignore |= sigmask(i);
	ps->ps_sigflags = SAS_NOCLDWAIT | SAS_NOCLDSTOP;
}

/*
 * Reset signals for an exec by the specified thread.
 */
void
execsigs(struct proc *p)
{
	struct sigacts *ps;
	int nc, mask;

	ps = p->p_p->ps_sigacts;

	/*
	 * Reset caught signals.  Held signals remain held
	 * through p_sigmask (unless they were caught,
	 * and are now ignored by default).
	 */
	while (ps->ps_sigcatch) {
		nc = ffs((long)ps->ps_sigcatch);
		mask = sigmask(nc);
		ps->ps_sigcatch &= ~mask;
		if (sigprop[nc] & SA_IGNORE) {
			if (nc != SIGCONT)
				ps->ps_sigignore |= mask;
			atomic_clearbits_int(&p->p_siglist, mask);
			atomic_clearbits_int(&p->p_p->ps_siglist, mask);
		}
		ps->ps_sigact[nc] = SIG_DFL;
	}
	/*
	 * Reset stack state to the user stack.
	 * Clear set of signals caught on the signal stack.
	 */
	sigstkinit(&p->p_sigstk);
	atomic_clearbits_int(&ps->ps_sigflags, SAS_NOCLDWAIT);
	if (ps->ps_sigact[SIGCHLD] == SIG_IGN)
		ps->ps_sigact[SIGCHLD] = SIG_DFL;
}

/*
 * Manipulate signal mask.
 * Note that we receive new mask, not pointer,
 * and return old mask as return value;
 * the library stub does the rest.
 */
int
sys_sigprocmask(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigprocmask_args /* {
		syscallarg(int) how;
		syscallarg(sigset_t) mask;
	} */ *uap = v;
	int error = 0;
	sigset_t mask;

	KASSERT(p == curproc);

	*retval = p->p_sigmask;
	mask = SCARG(uap, mask) &~ sigcantmask;

	switch (SCARG(uap, how)) {
	case SIG_BLOCK:
		atomic_setbits_int(&p->p_sigmask, mask);
		break;
	case SIG_UNBLOCK:
		atomic_clearbits_int(&p->p_sigmask, mask);
		break;
	case SIG_SETMASK:
		p->p_sigmask = mask;
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

int
sys_sigpending(struct proc *p, void *v, register_t *retval)
{

	*retval = p->p_siglist | p->p_p->ps_siglist;
	return (0);
}

/*
 * Temporarily replace calling proc's signal mask for the duration of a
 * system call.  Original signal mask will be restored by userret().
 */
void
dosigsuspend(struct proc *p, sigset_t newmask)
{
	KASSERT(p == curproc);

	p->p_oldmask = p->p_sigmask;
	atomic_setbits_int(&p->p_flag, P_SIGSUSPEND);
	p->p_sigmask = newmask;
}

/*
 * Suspend process until signal, providing mask to be set
 * in the meantime.  Note nonstandard calling convention:
 * libc stub passes mask, not pointer, to save a copyin.
 */
int
sys_sigsuspend(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigsuspend_args /* {
		syscallarg(int) mask;
	} */ *uap = v;
	struct process *pr = p->p_p;
	struct sigacts *ps = pr->ps_sigacts;

	dosigsuspend(p, SCARG(uap, mask) &~ sigcantmask);
	while (tsleep_nsec(ps, PPAUSE|PCATCH, "sigsusp", INFSLP) == 0)
		/* void */;
	/* always return EINTR rather than ERESTART... */
	return (EINTR);
}

int
sigonstack(size_t stack)
{
	const struct sigaltstack *ss = &curproc->p_sigstk;

	return (ss->ss_flags & SS_DISABLE ? 0 :
	    (stack - (size_t)ss->ss_sp < ss->ss_size));
}

int
sys_sigaltstack(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigaltstack_args /* {
		syscallarg(const struct sigaltstack *) nss;
		syscallarg(struct sigaltstack *) oss;
	} */ *uap = v;
	struct sigaltstack ss;
	const struct sigaltstack *nss;
	struct sigaltstack *oss;
	int onstack = sigonstack(PROC_STACK(p));
	int error;

	nss = SCARG(uap, nss);
	oss = SCARG(uap, oss);

	if (oss != NULL) {
		ss = p->p_sigstk;
		if (onstack)
			ss.ss_flags |= SS_ONSTACK;
		if ((error = copyout(&ss, oss, sizeof(ss))))
			return (error);
	}
	if (nss == NULL)
		return (0);
	error = copyin(nss, &ss, sizeof(ss));
	if (error)
		return (error);
	if (onstack)
		return (EPERM);
	if (ss.ss_flags & ~SS_DISABLE)
		return (EINVAL);
	if (ss.ss_flags & SS_DISABLE) {
		p->p_sigstk.ss_flags = ss.ss_flags;
		return (0);
	}
	if (ss.ss_size < MINSIGSTKSZ)
		return (ENOMEM);

	error = uvm_map_remap_as_stack(p, (vaddr_t)ss.ss_sp, ss.ss_size);
	if (error)
		return (error);

	p->p_sigstk = ss;
	return (0);
}

int
sys_kill(struct proc *cp, void *v, register_t *retval)
{
	struct sys_kill_args /* {
		syscallarg(int) pid;
		syscallarg(int) signum;
	} */ *uap = v;
	struct process *pr;
	int pid = SCARG(uap, pid);
	int signum = SCARG(uap, signum);
	int error;
	int zombie = 0;

	if ((error = pledge_kill(cp, pid)) != 0)
		return (error);
	if (((u_int)signum) >= NSIG)
		return (EINVAL);
	if (pid > 0) {
		if ((pr = prfind(pid)) == NULL) {
			if ((pr = zombiefind(pid)) == NULL)
				return (ESRCH);
			else
				zombie = 1;
		}
		if (!cansignal(cp, pr, signum))
			return (EPERM);

		/* kill single process */
		if (signum && !zombie)
			prsignal(pr, signum);
		return (0);
	}
	switch (pid) {
	case -1:		/* broadcast signal */
		return (killpg1(cp, signum, 0, 1));
	case 0:			/* signal own process group */
		return (killpg1(cp, signum, 0, 0));
	default:		/* negative explicit process group */
		return (killpg1(cp, signum, -pid, 0));
	}
}

int
sys_thrkill(struct proc *cp, void *v, register_t *retval)
{
	struct sys_thrkill_args /* {
		syscallarg(pid_t) tid;
		syscallarg(int) signum;
		syscallarg(void *) tcb;
	} */ *uap = v;
	struct proc *p;
	int tid = SCARG(uap, tid);
	int signum = SCARG(uap, signum);
	void *tcb;

	if (((u_int)signum) >= NSIG)
		return (EINVAL);
	if (tid > THREAD_PID_OFFSET) {
		if ((p = tfind(tid - THREAD_PID_OFFSET)) == NULL)
			return (ESRCH);

		/* can only kill threads in the same process */
		if (p->p_p != cp->p_p)
			return (ESRCH);
	} else if (tid == 0)
		p = cp;
	else
		return (EINVAL);

	/* optionally require the target thread to have the given tcb addr */
	tcb = SCARG(uap, tcb);
	if (tcb != NULL && tcb != TCB_GET(p))
		return (ESRCH);

	if (signum)
		ptsignal(p, signum, STHREAD);
	return (0);
}

/*
 * Common code for kill process group/broadcast kill.
 * cp is calling process.
 */
int
killpg1(struct proc *cp, int signum, int pgid, int all)
{
	struct process *pr;
	struct pgrp *pgrp;
	int nfound = 0;

	if (all) {
		/* 
		 * broadcast
		 */
		LIST_FOREACH(pr, &allprocess, ps_list) {
			if (pr->ps_pid <= 1 ||
			    pr->ps_flags & (PS_SYSTEM | PS_NOBROADCASTKILL) ||
			    pr == cp->p_p || !cansignal(cp, pr, signum))
				continue;
			nfound++;
			if (signum)
				prsignal(pr, signum);
		}
	} else {
		if (pgid == 0)
			/*
			 * zero pgid means send to my process group.
			 */
			pgrp = cp->p_p->ps_pgrp;
		else {
			pgrp = pgfind(pgid);
			if (pgrp == NULL)
				return (ESRCH);
		}
		LIST_FOREACH(pr, &pgrp->pg_members, ps_pglist) {
			if (pr->ps_pid <= 1 || pr->ps_flags & PS_SYSTEM ||
			    !cansignal(cp, pr, signum))
				continue;
			nfound++;
			if (signum)
				prsignal(pr, signum);
		}
	}
	return (nfound ? 0 : ESRCH);
}

#define CANDELIVER(uid, euid, pr) \
	(euid == 0 || \
	(uid) == (pr)->ps_ucred->cr_ruid || \
	(uid) == (pr)->ps_ucred->cr_svuid || \
	(uid) == (pr)->ps_ucred->cr_uid || \
	(euid) == (pr)->ps_ucred->cr_ruid || \
	(euid) == (pr)->ps_ucred->cr_svuid || \
	(euid) == (pr)->ps_ucred->cr_uid)

#define CANSIGIO(cr, pr) \
	CANDELIVER((cr)->cr_ruid, (cr)->cr_uid, (pr))

/*
 * Send a signal to a process group.  If checktty is 1,
 * limit to members which have a controlling terminal.
 */
void
pgsignal(struct pgrp *pgrp, int signum, int checkctty)
{
	struct process *pr;

	if (pgrp)
		LIST_FOREACH(pr, &pgrp->pg_members, ps_pglist)
			if (checkctty == 0 || pr->ps_flags & PS_CONTROLT)
				prsignal(pr, signum);
}

/*
 * Send a SIGIO or SIGURG signal to a process or process group using stored
 * credentials rather than those of the current process.
 */
void
pgsigio(struct sigio_ref *sir, int sig, int checkctty)
{
	struct process *pr;
	struct sigio *sigio;

	if (sir->sir_sigio == NULL)
		return;

	KERNEL_LOCK();
	mtx_enter(&sigio_lock);
	sigio = sir->sir_sigio;
	if (sigio == NULL)
		goto out;
	if (sigio->sio_pgid > 0) {
		if (CANSIGIO(sigio->sio_ucred, sigio->sio_proc))
			prsignal(sigio->sio_proc, sig);
	} else if (sigio->sio_pgid < 0) {
		LIST_FOREACH(pr, &sigio->sio_pgrp->pg_members, ps_pglist) {
			if (CANSIGIO(sigio->sio_ucred, pr) &&
			    (checkctty == 0 || (pr->ps_flags & PS_CONTROLT)))
				prsignal(pr, sig);
		}
	}
out:
	mtx_leave(&sigio_lock);
	KERNEL_UNLOCK();
}

/*
 * Recalculate the signal mask and reset the signal disposition after
 * usermode frame for delivery is formed.
 */
void
postsig_done(struct proc *p, int signum, struct sigacts *ps)
{
	int mask = sigmask(signum);

	KERNEL_ASSERT_LOCKED();

	p->p_ru.ru_nsignals++;
	atomic_setbits_int(&p->p_sigmask, ps->ps_catchmask[signum]);
	if ((ps->ps_sigreset & mask) != 0) {
		ps->ps_sigcatch &= ~mask;
		if (signum != SIGCONT && sigprop[signum] & SA_IGNORE)
			ps->ps_sigignore |= mask;
		ps->ps_sigact[signum] = SIG_DFL;
	}
}

/*
 * Send a signal caused by a trap to the current thread
 * If it will be caught immediately, deliver it with correct code.
 * Otherwise, post it normally.
 */
void
trapsignal(struct proc *p, int signum, u_long trapno, int code,
    union sigval sigval)
{
	struct process *pr = p->p_p;
	struct sigacts *ps = pr->ps_sigacts;
	int mask;

	KERNEL_LOCK();
	switch (signum) {
	case SIGILL:
	case SIGBUS:
	case SIGSEGV:
		pr->ps_acflag |= ATRAP;
		break;
	}

	mask = sigmask(signum);
	if ((pr->ps_flags & PS_TRACED) == 0 &&
	    (ps->ps_sigcatch & mask) != 0 &&
	    (p->p_sigmask & mask) == 0) {
		siginfo_t si;
		initsiginfo(&si, signum, trapno, code, sigval);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_PSIG)) {
			ktrpsig(p, signum, ps->ps_sigact[signum],
			    p->p_sigmask, code, &si);
		}
#endif
		if (sendsig(ps->ps_sigact[signum], signum, p->p_sigmask, &si)) {
			sigexit(p, SIGILL);
			/* NOTREACHED */
		}
		postsig_done(p, signum, ps);
	} else {
		p->p_sisig = signum;
		p->p_sitrapno = trapno;	/* XXX for core dump/debugger */
		p->p_sicode = code;
		p->p_sigval = sigval;

		/*
		 * Signals like SIGBUS and SIGSEGV should not, when
		 * generated by the kernel, be ignorable or blockable.
		 * If it is and we're not being traced, then just kill
		 * the process.
		 */
		if ((pr->ps_flags & PS_TRACED) == 0 &&
		    (sigprop[signum] & SA_KILL) &&
		    ((p->p_sigmask & mask) || (ps->ps_sigignore & mask)))
			sigexit(p, signum);
		ptsignal(p, signum, STHREAD);
	}
	KERNEL_UNLOCK();
}

/*
 * Send the signal to the process.  If the signal has an action, the action
 * is usually performed by the target process rather than the caller; we add
 * the signal to the set of pending signals for the process.
 *
 * Exceptions:
 *   o When a stop signal is sent to a sleeping process that takes the
 *     default action, the process is stopped without awakening it.
 *   o SIGCONT restarts stopped processes (or puts them back to sleep)
 *     regardless of the signal action (eg, blocked or ignored).
 *
 * Other ignored signals are discarded immediately.
 */
void
psignal(struct proc *p, int signum)
{
	ptsignal(p, signum, SPROCESS);
}

/*
 * type = SPROCESS	process signal, can be diverted (sigwait())
 * type = STHREAD	thread signal, but should be propagated if unhandled
 * type = SPROPAGATED	propagated to this thread, so don't propagate again
 */
void
ptsignal(struct proc *p, int signum, enum signal_type type)
{
	int s, prop;
	sig_t action;
	int mask;
	int *siglist;
	struct process *pr = p->p_p;
	struct proc *q;
	int wakeparent = 0;

	KERNEL_ASSERT_LOCKED();

#ifdef DIAGNOSTIC
	if ((u_int)signum >= NSIG || signum == 0)
		panic("psignal signal number");
#endif

	/* Ignore signal if the target process is exiting */
	if (pr->ps_flags & PS_EXITING)
		return;

	mask = sigmask(signum);

	if (type == SPROCESS) {
		/* Accept SIGKILL to coredumping processes */
		if (pr->ps_flags & PS_COREDUMP && signum == SIGKILL) {
			atomic_setbits_int(&pr->ps_siglist, mask);
			return;
		}

		/*
		 * If the current thread can process the signal
		 * immediately (it's unblocked) then have it take it.
		 */
		q = curproc;
		if (q != NULL && q->p_p == pr && (q->p_flag & P_WEXIT) == 0 &&
		    (q->p_sigmask & mask) == 0)
			p = q;
		else {
			/*
			 * A process-wide signal can be diverted to a
			 * different thread that's in sigwait() for this
			 * signal.  If there isn't such a thread, then
			 * pick a thread that doesn't have it blocked so
			 * that the stop/kill consideration isn't
			 * delayed.  Otherwise, mark it pending on the
			 * main thread.
			 */
			TAILQ_FOREACH(q, &pr->ps_threads, p_thr_link) {
				/* ignore exiting threads */
				if (q->p_flag & P_WEXIT)
					continue;

				/* skip threads that have the signal blocked */
				if ((q->p_sigmask & mask) != 0)
					continue;

				/* okay, could send to this thread */
				p = q;

				/*
				 * sigsuspend, sigwait, ppoll/pselect, etc?
				 * Definitely go to this thread, as it's
				 * already blocked in the kernel.
				 */
				if (q->p_flag & P_SIGSUSPEND)
					break;
			}
		}
	}

	if (type != SPROPAGATED)
		KNOTE(&pr->ps_klist, NOTE_SIGNAL | signum);

	prop = sigprop[signum];

	/*
	 * If proc is traced, always give parent a chance.
	 */
	if (pr->ps_flags & PS_TRACED) {
		action = SIG_DFL;
	} else {
		/*
		 * If the signal is being ignored,
		 * then we forget about it immediately.
		 * (Note: we don't set SIGCONT in ps_sigignore,
		 * and if it is set to SIG_IGN,
		 * action will be SIG_DFL here.)
		 */
		if (pr->ps_sigacts->ps_sigignore & mask)
			return;
		if (p->p_sigmask & mask) {
			action = SIG_HOLD;
		} else if (pr->ps_sigacts->ps_sigcatch & mask) {
			action = SIG_CATCH;
		} else {
			action = SIG_DFL;

			if (prop & SA_KILL && pr->ps_nice > NZERO)
				 pr->ps_nice = NZERO;

			/*
			 * If sending a tty stop signal to a member of an
			 * orphaned process group, discard the signal here if
			 * the action is default; don't stop the process below
			 * if sleeping, and don't clear any pending SIGCONT.
			 */
			if (prop & SA_TTYSTOP && pr->ps_pgrp->pg_jobc == 0)
				return;
		}
	}
	/*
	 * If delivered to process, mark as pending there.  Continue and stop
	 * signals will be propagated to all threads.  So they are always
	 * marked at thread level.
	 */
	siglist = (type == SPROCESS) ? &pr->ps_siglist : &p->p_siglist;
	if (prop & SA_CONT) {
		siglist = &p->p_siglist;
		atomic_clearbits_int(siglist, STOPSIGMASK);
	}
	if (prop & SA_STOP) {
		siglist = &p->p_siglist;
		atomic_clearbits_int(siglist, CONTSIGMASK);
		atomic_clearbits_int(&p->p_flag, P_CONTINUED);
	}
	atomic_setbits_int(siglist, mask);

	/*
	 * XXX delay processing of SA_STOP signals unless action == SIG_DFL?
	 */
	if (prop & (SA_CONT | SA_STOP) && type != SPROPAGATED)
		TAILQ_FOREACH(q, &pr->ps_threads, p_thr_link)
			if (q != p)
				ptsignal(q, signum, SPROPAGATED);

	/*
	 * Defer further processing for signals which are held,
	 * except that stopped processes must be continued by SIGCONT.
	 */
	if (action == SIG_HOLD && ((prop & SA_CONT) == 0 || p->p_stat != SSTOP))
		return;

	SCHED_LOCK(s);

	switch (p->p_stat) {

	case SSLEEP:
		/*
		 * If process is sleeping uninterruptibly
		 * we can't interrupt the sleep... the signal will
		 * be noticed when the process returns through
		 * trap() or syscall().
		 */
		if ((p->p_flag & P_SINTR) == 0)
			goto out;
		/*
		 * Process is sleeping and traced... make it runnable
		 * so it can discover the signal in cursig() and stop
		 * for the parent.
		 */
		if (pr->ps_flags & PS_TRACED)
			goto run;
		/*
		 * If SIGCONT is default (or ignored) and process is
		 * asleep, we are finished; the process should not
		 * be awakened.
		 */
		if ((prop & SA_CONT) && action == SIG_DFL) {
			atomic_clearbits_int(siglist, mask);
			goto out;
		}
		/*
		 * When a sleeping process receives a stop
		 * signal, process immediately if possible.
		 */
		if ((prop & SA_STOP) && action == SIG_DFL) {
			/*
			 * If a child holding parent blocked,
			 * stopping could cause deadlock.
			 */
			if (pr->ps_flags & PS_PPWAIT)
				goto out;
			atomic_clearbits_int(siglist, mask);
			pr->ps_xsig = signum;
			proc_stop(p, 0);
			goto out;
		}
		/*
		 * All other (caught or default) signals
		 * cause the process to run.
		 */
		goto runfast;
		/*NOTREACHED*/

	case SSTOP:
		/*
		 * If traced process is already stopped,
		 * then no further action is necessary.
		 */
		if (pr->ps_flags & PS_TRACED)
			goto out;

		/*
		 * Kill signal always sets processes running.
		 */
		if (signum == SIGKILL) {
			atomic_clearbits_int(&p->p_flag, P_SUSPSIG);
			goto runfast;
		}

		if (prop & SA_CONT) {
			/*
			 * If SIGCONT is default (or ignored), we continue the
			 * process but don't leave the signal in p_siglist, as
			 * it has no further action.  If SIGCONT is held, we
			 * continue the process and leave the signal in
			 * p_siglist.  If the process catches SIGCONT, let it
			 * handle the signal itself.  If it isn't waiting on
			 * an event, then it goes back to run state.
			 * Otherwise, process goes back to sleep state.
			 */
			atomic_setbits_int(&p->p_flag, P_CONTINUED);
			atomic_clearbits_int(&p->p_flag, P_SUSPSIG);
			wakeparent = 1;
			if (action == SIG_DFL)
				atomic_clearbits_int(siglist, mask);
			if (action == SIG_CATCH)
				goto runfast;
			if (p->p_wchan == 0)
				goto run;
			p->p_stat = SSLEEP;
			goto out;
		}

		if (prop & SA_STOP) {
			/*
			 * Already stopped, don't need to stop again.
			 * (If we did the shell could get confused.)
			 */
			atomic_clearbits_int(siglist, mask);
			goto out;
		}

		/*
		 * If process is sleeping interruptibly, then simulate a
		 * wakeup so that when it is continued, it will be made
		 * runnable and can look at the signal.  But don't make
		 * the process runnable, leave it stopped.
		 */
		if (p->p_flag & P_SINTR)
			unsleep(p);
		goto out;

	case SONPROC:
		signotify(p);
		/* FALLTHROUGH */
	default:
		/*
		 * SRUN, SIDL, SDEAD do nothing with the signal,
		 * other than kicking ourselves if we are running.
		 * It will either never be noticed, or noticed very soon.
		 */
		goto out;
	}
	/*NOTREACHED*/

runfast:
	/*
	 * Raise priority to at least PUSER.
	 */
	if (p->p_usrpri > PUSER)
		p->p_usrpri = PUSER;
run:
	setrunnable(p);
out:
	SCHED_UNLOCK(s);
	if (wakeparent)
		wakeup(pr->ps_pptr);
}

/*
 * Determine signal that should be delivered to process p, the current
 * process, 0 if none.
 *
 * If the current process has received a signal (should be caught or cause
 * termination, should interrupt current syscall), return the signal number.
 * Stop signals with default action are processed immediately, then cleared;
 * they aren't returned.  This is checked after each entry to the system for
 * a syscall or trap. The normal call sequence is
 *
 *	while (signum = cursig(curproc))
 *		postsig(signum);
 *
 * Assumes that if the P_SINTR flag is set, we're holding both the
 * kernel and scheduler locks.
 */
int
cursig(struct proc *p)
{
	struct process *pr = p->p_p;
	int sigpending, signum, mask, prop;
	int dolock = (p->p_flag & P_SINTR) == 0;
	int s;

	KERNEL_ASSERT_LOCKED();

	sigpending = (p->p_siglist | pr->ps_siglist);
	if (sigpending == 0)
		return 0;

	if (!ISSET(pr->ps_flags, PS_TRACED) && SIGPENDING(p) == 0)
		return 0;

	for (;;) {
		mask = SIGPENDING(p);
		if (pr->ps_flags & PS_PPWAIT)
			mask &= ~STOPSIGMASK;
		if (mask == 0)	 	/* no signal to send */
			return (0);
		signum = ffs((long)mask);
		mask = sigmask(signum);
		atomic_clearbits_int(&p->p_siglist, mask);
		atomic_clearbits_int(&pr->ps_siglist, mask);

		/*
		 * We should see pending but ignored signals
		 * only if PS_TRACED was on when they were posted.
		 */
		if (mask & pr->ps_sigacts->ps_sigignore &&
		    (pr->ps_flags & PS_TRACED) == 0)
			continue;

		/*
		 * If traced, always stop, and stay stopped until released
		 * by the debugger.  If our parent process is waiting for
		 * us, don't hang as we could deadlock.
		 */
		if (((pr->ps_flags & (PS_TRACED | PS_PPWAIT)) == PS_TRACED) &&
		    signum != SIGKILL) {
			pr->ps_xsig = signum;

			single_thread_set(p, SINGLE_SUSPEND, 0);

			if (dolock)
				SCHED_LOCK(s);
			proc_stop(p, 1);
			if (dolock)
				SCHED_UNLOCK(s);

			single_thread_clear(p, 0);

			/*
			 * If we are no longer being traced, or the parent
			 * didn't give us a signal, look for more signals.
			 */
			if ((pr->ps_flags & PS_TRACED) == 0 ||
			    pr->ps_xsig == 0)
				continue;

			/*
			 * If the new signal is being masked, look for other
			 * signals.
			 */
			signum = pr->ps_xsig;
			mask = sigmask(signum);
			if ((p->p_sigmask & mask) != 0)
				continue;

			/* take the signal! */
			atomic_clearbits_int(&p->p_siglist, mask);
			atomic_clearbits_int(&pr->ps_siglist, mask);
		}

		prop = sigprop[signum];

		/*
		 * Decide whether the signal should be returned.
		 * Return the signal's number, or fall through
		 * to clear it from the pending mask.
		 */
		switch ((long)pr->ps_sigacts->ps_sigact[signum]) {
		case (long)SIG_DFL:
			/*
			 * Don't take default actions on system processes.
			 */
			if (pr->ps_pid <= 1) {
#ifdef DIAGNOSTIC
				/*
				 * Are you sure you want to ignore SIGSEGV
				 * in init? XXX
				 */
				printf("Process (pid %d) got signal"
				    " %d\n", pr->ps_pid, signum);
#endif
				break;		/* == ignore */
			}
			/*
			 * If there is a pending stop signal to process
			 * with default action, stop here,
			 * then clear the signal.  However,
			 * if process is member of an orphaned
			 * process group, ignore tty stop signals.
			 */
			if (prop & SA_STOP) {
				if (pr->ps_flags & PS_TRACED ||
		    		    (pr->ps_pgrp->pg_jobc == 0 &&
				    prop & SA_TTYSTOP))
					break;	/* == ignore */
				pr->ps_xsig = signum;
				if (dolock)
					SCHED_LOCK(s);
				proc_stop(p, 1);
				if (dolock)
					SCHED_UNLOCK(s);
				break;
			} else if (prop & SA_IGNORE) {
				/*
				 * Except for SIGCONT, shouldn't get here.
				 * Default action is to ignore; drop it.
				 */
				break;		/* == ignore */
			} else
				goto keep;
			/*NOTREACHED*/
		case (long)SIG_IGN:
			/*
			 * Masking above should prevent us ever trying
			 * to take action on an ignored signal other
			 * than SIGCONT, unless process is traced.
			 */
			if ((prop & SA_CONT) == 0 &&
			    (pr->ps_flags & PS_TRACED) == 0)
				printf("%s\n", __func__);
			break;		/* == ignore */
		default:
			/*
			 * This signal has an action, let
			 * postsig() process it.
			 */
			goto keep;
		}
	}
	/* NOTREACHED */

keep:
	atomic_setbits_int(&p->p_siglist, mask); /*leave the signal for later */
	return (signum);
}

/*
 * Put the argument process into the stopped state and notify the parent
 * via wakeup.  Signals are handled elsewhere.  The process must not be
 * on the run queue.
 */
void
proc_stop(struct proc *p, int sw)
{
	struct process *pr = p->p_p;

#ifdef MULTIPROCESSOR
	SCHED_ASSERT_LOCKED();
#endif

	p->p_stat = SSTOP;
	atomic_clearbits_int(&pr->ps_flags, PS_WAITED);
	atomic_setbits_int(&pr->ps_flags, PS_STOPPED);
	atomic_setbits_int(&p->p_flag, P_SUSPSIG);
	/*
	 * We need this soft interrupt to be handled fast.
	 * Extra calls to softclock don't hurt.
	 */
	softintr_schedule(proc_stop_si);
	if (sw)
		mi_switch();
}

/*
 * Called from a soft interrupt to send signals to the parents of stopped
 * processes.
 * We can't do this in proc_stop because it's called with nasty locks held
 * and we would need recursive scheduler lock to deal with that.
 */
void
proc_stop_sweep(void *v)
{
	struct process *pr;

	LIST_FOREACH(pr, &allprocess, ps_list) {
		if ((pr->ps_flags & PS_STOPPED) == 0)
			continue;
		atomic_clearbits_int(&pr->ps_flags, PS_STOPPED);

		if ((pr->ps_pptr->ps_sigacts->ps_sigflags & SAS_NOCLDSTOP) == 0)
			prsignal(pr->ps_pptr, SIGCHLD);
		wakeup(pr->ps_pptr);
	}
}

/*
 * Take the action for the specified signal
 * from the current set of pending signals.
 */
void
postsig(struct proc *p, int signum)
{
	struct process *pr = p->p_p;
	struct sigacts *ps = pr->ps_sigacts;
	sig_t action;
	u_long trapno;
	int mask, returnmask;
	siginfo_t si;
	union sigval sigval;
	int s, code;

	KASSERT(signum != 0);
	KERNEL_ASSERT_LOCKED();

	mask = sigmask(signum);
	atomic_clearbits_int(&p->p_siglist, mask);
	action = ps->ps_sigact[signum];
	sigval.sival_ptr = 0;

	if (p->p_sisig != signum) {
		trapno = 0;
		code = SI_USER;
		sigval.sival_ptr = 0;
	} else {
		trapno = p->p_sitrapno;
		code = p->p_sicode;
		sigval = p->p_sigval;
	}
	initsiginfo(&si, signum, trapno, code, sigval);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_PSIG)) {
		ktrpsig(p, signum, action, p->p_flag & P_SIGSUSPEND ?
		    p->p_oldmask : p->p_sigmask, code, &si);
	}
#endif
	if (action == SIG_DFL) {
		/*
		 * Default action, where the default is to kill
		 * the process.  (Other cases were ignored above.)
		 */
		sigexit(p, signum);
		/* NOTREACHED */
	} else {
		/*
		 * If we get here, the signal must be caught.
		 */
#ifdef DIAGNOSTIC
		if (action == SIG_IGN || (p->p_sigmask & mask))
			panic("postsig action");
#endif
		/*
		 * Set the new mask value and also defer further
		 * occurrences of this signal.
		 *
		 * Special case: user has done a sigpause.  Here the
		 * current mask is not of interest, but rather the
		 * mask from before the sigpause is what we want
		 * restored after the signal processing is completed.
		 */
#ifdef MULTIPROCESSOR
		s = splsched();
#else
		s = splhigh();
#endif
		if (p->p_flag & P_SIGSUSPEND) {
			atomic_clearbits_int(&p->p_flag, P_SIGSUSPEND);
			returnmask = p->p_oldmask;
		} else {
			returnmask = p->p_sigmask;
		}
		if (p->p_sisig == signum) {
			p->p_sisig = 0;
			p->p_sitrapno = 0;
			p->p_sicode = SI_USER;
			p->p_sigval.sival_ptr = NULL;
		}

		if (sendsig(action, signum, returnmask, &si)) {
			sigexit(p, SIGILL);
			/* NOTREACHED */
		}
		postsig_done(p, signum, ps);
		splx(s);
	}
}

/*
 * Force the current process to exit with the specified signal, dumping core
 * if appropriate.  We bypass the normal tests for masked and caught signals,
 * allowing unrecoverable failures to terminate the process without changing
 * signal state.  Mark the accounting record with the signal termination.
 * If dumping core, save the signal number for the debugger.  Calls exit and
 * does not return.
 */
void
sigexit(struct proc *p, int signum)
{
	/* Mark process as going away */
	atomic_setbits_int(&p->p_flag, P_WEXIT);

	p->p_p->ps_acflag |= AXSIG;
	if (sigprop[signum] & SA_CORE) {
		p->p_sisig = signum;

		/* if there are other threads, pause them */
		if (P_HASSIBLING(p))
			single_thread_set(p, SINGLE_SUSPEND, 1);

		if (coredump(p) == 0)
			signum |= WCOREFLAG;
	}
	exit1(p, 0, signum, EXIT_NORMAL);
	/* NOTREACHED */
}

/*
 * Send uncatchable SIGABRT for coredump.
 */
void
sigabort(struct proc *p)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_DFL;
	setsigvec(p, SIGABRT, &sa);
	atomic_clearbits_int(&p->p_sigmask, sigmask(SIGABRT));
	psignal(p, SIGABRT);
}

/*
 * Return 1 if `sig', a given signal, is ignored or masked for `p', a given
 * thread, and 0 otherwise.
 */
int
sigismasked(struct proc *p, int sig)
{
	struct process *pr = p->p_p;

	if ((pr->ps_sigacts->ps_sigignore & sigmask(sig)) ||
	    (p->p_sigmask & sigmask(sig)))
	    	return 1;

	return 0;
}

int nosuidcoredump = 1;

struct coredump_iostate {
	struct proc *io_proc;
	struct vnode *io_vp;
	struct ucred *io_cred;
	off_t io_offset;
};

/*
 * Dump core, into a file named "progname.core", unless the process was
 * setuid/setgid.
 */
int
coredump(struct proc *p)
{
#ifdef SMALL_KERNEL
	return EPERM;
#else
	struct process *pr = p->p_p;
	struct vnode *vp;
	struct ucred *cred = p->p_ucred;
	struct vmspace *vm = p->p_vmspace;
	struct nameidata nd;
	struct vattr vattr;
	struct coredump_iostate	io;
	int error, len, incrash = 0;
	char *name;
	const char *dir = "/var/crash";

	if (pr->ps_emul->e_coredump == NULL)
		return (EINVAL);

	atomic_setbits_int(&pr->ps_flags, PS_COREDUMP);

	/* Don't dump if will exceed file size limit. */
	if (USPACE + ptoa(vm->vm_dsize + vm->vm_ssize) >= lim_cur(RLIMIT_CORE))
		return (EFBIG);

	name = pool_get(&namei_pool, PR_WAITOK);

	/*
	 * If the process has inconsistent uids, nosuidcoredump
	 * determines coredump placement policy.
	 */
	if (((pr->ps_flags & PS_SUGID) && (error = suser(p))) ||
	   ((pr->ps_flags & PS_SUGID) && nosuidcoredump)) {
		if (nosuidcoredump == 3) {
			/*
			 * If the program directory does not exist, dumps of
			 * that core will silently fail.
			 */
			len = snprintf(name, MAXPATHLEN, "%s/%s/%u.core",
			    dir, pr->ps_comm, pr->ps_pid);
			incrash = KERNELPATH;
		} else if (nosuidcoredump == 2) {
			len = snprintf(name, MAXPATHLEN, "%s/%s.core",
			    dir, pr->ps_comm);
			incrash = KERNELPATH;
		} else {
			pool_put(&namei_pool, name);
			return (EPERM);
		}
	} else
		len = snprintf(name, MAXPATHLEN, "%s.core", pr->ps_comm);

	if (len >= MAXPATHLEN) {
		pool_put(&namei_pool, name);
		return (EACCES);
	}

	/*
	 * Control the UID used to write out.  The normal case uses
	 * the real UID.  If the sugid case is going to write into the
	 * controlled directory, we do so as root.
	 */
	if (incrash == 0) {
		cred = crdup(cred);
		cred->cr_uid = cred->cr_ruid;
		cred->cr_gid = cred->cr_rgid;
	} else {
		if (p->p_fd->fd_rdir) {
			vrele(p->p_fd->fd_rdir);
			p->p_fd->fd_rdir = NULL;
		}
		p->p_ucred = crdup(p->p_ucred);
		crfree(cred);
		cred = p->p_ucred;
		crhold(cred);
		cred->cr_uid = 0;
		cred->cr_gid = 0;
	}

	/* incrash should be 0 or KERNELPATH only */
	NDINIT(&nd, 0, incrash, UIO_SYSSPACE, name, p);

	error = vn_open(&nd, O_CREAT | FWRITE | O_NOFOLLOW | O_NONBLOCK,
	    S_IRUSR | S_IWUSR);

	if (error)
		goto out;

	/*
	 * Don't dump to non-regular files, files with links, or files
	 * owned by someone else.
	 */
	vp = nd.ni_vp;
	if ((error = VOP_GETATTR(vp, &vattr, cred, p)) != 0) {
		VOP_UNLOCK(vp);
		vn_close(vp, FWRITE, cred, p);
		goto out;
	}
	if (vp->v_type != VREG || vattr.va_nlink != 1 ||
	    vattr.va_mode & ((VREAD | VWRITE) >> 3 | (VREAD | VWRITE) >> 6) ||
	    vattr.va_uid != cred->cr_uid) {
		error = EACCES;
		VOP_UNLOCK(vp);
		vn_close(vp, FWRITE, cred, p);
		goto out;
	}
	VATTR_NULL(&vattr);
	vattr.va_size = 0;
	VOP_SETATTR(vp, &vattr, cred, p);
	pr->ps_acflag |= ACORE;

	io.io_proc = p;
	io.io_vp = vp;
	io.io_cred = cred;
	io.io_offset = 0;
	VOP_UNLOCK(vp);
	vref(vp);
	error = vn_close(vp, FWRITE, cred, p);
	if (error == 0)
		error = (*pr->ps_emul->e_coredump)(p, &io);
	vrele(vp);
out:
	crfree(cred);
	pool_put(&namei_pool, name);
	return (error);
#endif
}

#ifndef SMALL_KERNEL
int
coredump_write(void *cookie, enum uio_seg segflg, const void *data, size_t len)
{
	struct coredump_iostate *io = cookie;
	off_t coffset = 0;
	size_t csize;
	int chunk, error;

	csize = len;
	do {
		if (sigmask(SIGKILL) &
		    (io->io_proc->p_siglist | io->io_proc->p_p->ps_siglist))
			return (EINTR);

		/* Rest of the loop sleeps with lock held, so... */
		yield();

		chunk = MIN(csize, MAXPHYS);
		error = vn_rdwr(UIO_WRITE, io->io_vp,
		    (caddr_t)data + coffset, chunk,
		    io->io_offset + coffset, segflg,
		    IO_UNIT, io->io_cred, NULL, io->io_proc);
		if (error) {
			struct process *pr = io->io_proc->p_p;

			if (error == ENOSPC)
				log(LOG_ERR,
				    "coredump of %s(%d) failed, filesystem full\n",
				    pr->ps_comm, pr->ps_pid);
			else
				log(LOG_ERR,
				    "coredump of %s(%d), write failed: errno %d\n",
				    pr->ps_comm, pr->ps_pid, error);
			return (error);
		}

		coffset += chunk;
		csize -= chunk;
	} while (csize > 0);

	io->io_offset += len;
	return (0);
}

void
coredump_unmap(void *cookie, vaddr_t start, vaddr_t end)
{
	struct coredump_iostate *io = cookie;

	uvm_unmap(&io->io_proc->p_vmspace->vm_map, start, end);
}

#endif	/* !SMALL_KERNEL */

/*
 * Nonexistent system call-- signal process (may want to handle it).
 * Flag error in case process won't see signal immediately (blocked or ignored).
 */
int
sys_nosys(struct proc *p, void *v, register_t *retval)
{

	ptsignal(p, SIGSYS, STHREAD);
	return (ENOSYS);
}

int
sys___thrsigdivert(struct proc *p, void *v, register_t *retval)
{
	static int sigwaitsleep;
	struct sys___thrsigdivert_args /* {
		syscallarg(sigset_t) sigmask;
		syscallarg(siginfo_t *) info;
		syscallarg(const struct timespec *) timeout;
	} */ *uap = v;
	struct process *pr = p->p_p;
	sigset_t *m;
	sigset_t mask = SCARG(uap, sigmask) &~ sigcantmask;
	siginfo_t si;
	uint64_t nsecs = INFSLP;
	int timeinvalid = 0;
	int error = 0;

	memset(&si, 0, sizeof(si));

	if (SCARG(uap, timeout) != NULL) {
		struct timespec ts;
		if ((error = copyin(SCARG(uap, timeout), &ts, sizeof(ts))) != 0)
			return (error);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrreltimespec(p, &ts);
#endif
		if (!timespecisvalid(&ts))
			timeinvalid = 1;
		else
			nsecs = TIMESPEC_TO_NSEC(&ts);
	}

	dosigsuspend(p, p->p_sigmask &~ mask);
	for (;;) {
		si.si_signo = cursig(p);
		if (si.si_signo != 0) {
			sigset_t smask = sigmask(si.si_signo);
			if (smask & mask) {
				if (p->p_siglist & smask)
					m = &p->p_siglist;
				else if (pr->ps_siglist & smask)
					m = &pr->ps_siglist;
				else {
					/* signal got eaten by someone else? */
					continue;
				}
				atomic_clearbits_int(m, smask);
				error = 0;
				break;
			}
		}

		/* per-POSIX, delay this error until after the above */
		if (timeinvalid)
			error = EINVAL;

		if (SCARG(uap, timeout) != NULL && nsecs == INFSLP)
			error = EAGAIN;

		if (error != 0)
			break;

		error = tsleep_nsec(&sigwaitsleep, PPAUSE|PCATCH, "sigwait",
		    nsecs);
	}

	if (error == 0) {
		*retval = si.si_signo;
		if (SCARG(uap, info) != NULL)
			error = copyout(&si, SCARG(uap, info), sizeof(si));
	} else if (error == ERESTART && SCARG(uap, timeout) != NULL) {
		/*
		 * Restarting is wrong if there's a timeout, as it'll be
		 * for the same interval again
		 */
		error = EINTR;
	}

	return (error);
}

void
initsiginfo(siginfo_t *si, int sig, u_long trapno, int code, union sigval val)
{
	memset(si, 0, sizeof(*si));

	si->si_signo = sig;
	si->si_code = code;
	if (code == SI_USER) {
		si->si_value = val;
	} else {
		switch (sig) {
		case SIGSEGV:
		case SIGILL:
		case SIGBUS:
		case SIGFPE:
			si->si_addr = val.sival_ptr;
			si->si_trapno = trapno;
			break;
		case SIGXFSZ:
			break;
		}
	}
}

int
filt_sigattach(struct knote *kn)
{
	struct process *pr = curproc->p_p;
	int s;

	if (kn->kn_id >= NSIG)
		return EINVAL;

	kn->kn_ptr.p_process = pr;
	kn->kn_flags |= EV_CLEAR;		/* automatically set */

	s = splhigh();
	klist_insert_locked(&pr->ps_klist, kn);
	splx(s);

	return (0);
}

void
filt_sigdetach(struct knote *kn)
{
	struct process *pr = kn->kn_ptr.p_process;
	int s;

	s = splhigh();
	klist_remove_locked(&pr->ps_klist, kn);
	splx(s);
}

/*
 * signal knotes are shared with proc knotes, so we apply a mask to
 * the hint in order to differentiate them from process hints.  This
 * could be avoided by using a signal-specific knote list, but probably
 * isn't worth the trouble.
 */
int
filt_signal(struct knote *kn, long hint)
{

	if (hint & NOTE_SIGNAL) {
		hint &= ~NOTE_SIGNAL;

		if (kn->kn_id == hint)
			kn->kn_data++;
	}
	return (kn->kn_data != 0);
}

void
userret(struct proc *p)
{
	int signum;

	/* send SIGPROF or SIGVTALRM if their timers interrupted this thread */
	if (p->p_flag & P_PROFPEND) {
		atomic_clearbits_int(&p->p_flag, P_PROFPEND);
		KERNEL_LOCK();
		psignal(p, SIGPROF);
		KERNEL_UNLOCK();
	}
	if (p->p_flag & P_ALRMPEND) {
		atomic_clearbits_int(&p->p_flag, P_ALRMPEND);
		KERNEL_LOCK();
		psignal(p, SIGVTALRM);
		KERNEL_UNLOCK();
	}

	if (SIGPENDING(p) != 0) {
		KERNEL_LOCK();
		while ((signum = cursig(p)) != 0)
			postsig(p, signum);
		KERNEL_UNLOCK();
	}

	/*
	 * If P_SIGSUSPEND is still set here, then we still need to restore
	 * the original sigmask before returning to userspace.  Also, this
	 * might unmask some pending signals, so we need to check a second
	 * time for signals to post.
	 */
	if (p->p_flag & P_SIGSUSPEND) {
		atomic_clearbits_int(&p->p_flag, P_SIGSUSPEND);
		p->p_sigmask = p->p_oldmask;

		KERNEL_LOCK();
		while ((signum = cursig(p)) != 0)
			postsig(p, signum);
		KERNEL_UNLOCK();
	}

	if (p->p_flag & P_SUSPSINGLE)
		single_thread_check(p, 0);

	WITNESS_WARN(WARN_PANIC, NULL, "userret: returning");

	p->p_cpu->ci_schedstate.spc_curpriority = p->p_usrpri;
}

int
single_thread_check_locked(struct proc *p, int deep, int s)
{
	struct process *pr = p->p_p;

	SCHED_ASSERT_LOCKED();

	if (pr->ps_single != NULL && pr->ps_single != p) {
		do {
			/* if we're in deep, we need to unwind to the edge */
			if (deep) {
				if (pr->ps_flags & PS_SINGLEUNWIND)
					return (ERESTART);
				if (pr->ps_flags & PS_SINGLEEXIT)
					return (EINTR);
			}

			if (pr->ps_single == NULL)
				continue;

			if (atomic_dec_int_nv(&pr->ps_singlecount) == 0)
				wakeup(&pr->ps_singlecount);

			if (pr->ps_flags & PS_SINGLEEXIT) {
				SCHED_UNLOCK(s);
				KERNEL_LOCK();
				exit1(p, 0, 0, EXIT_THREAD_NOCHECK);
				/* NOTREACHED */
			}

			/* not exiting and don't need to unwind, so suspend */
			p->p_stat = SSTOP;
			mi_switch();
		} while (pr->ps_single != NULL);
	}

	return (0);
}

int
single_thread_check(struct proc *p, int deep)
{
	int s, error;

	SCHED_LOCK(s);
	error = single_thread_check_locked(p, deep, s);
	SCHED_UNLOCK(s);

	return error;
}

/*
 * Stop other threads in the process.  The mode controls how and
 * where the other threads should stop:
 *  - SINGLE_SUSPEND: stop wherever they are, will later either be told to exit
 *    (by setting to SINGLE_EXIT) or be released (via single_thread_clear())
 *  - SINGLE_UNWIND: just unwind to kernel boundary, will be told to exit
 *    or released as with SINGLE_SUSPEND
 *  - SINGLE_EXIT: unwind to kernel boundary and exit
 */
int
single_thread_set(struct proc *p, enum single_thread_mode mode, int wait)
{
	struct process *pr = p->p_p;
	struct proc *q;
	int error, s;

	KASSERT(curproc == p);

	SCHED_LOCK(s);
	error = single_thread_check_locked(p, (mode == SINGLE_UNWIND), s);
	if (error) {
		SCHED_UNLOCK(s);
		return error;
	}

	switch (mode) {
	case SINGLE_SUSPEND:
		break;
	case SINGLE_UNWIND:
		atomic_setbits_int(&pr->ps_flags, PS_SINGLEUNWIND);
		break;
	case SINGLE_EXIT:
		atomic_setbits_int(&pr->ps_flags, PS_SINGLEEXIT);
		atomic_clearbits_int(&pr->ps_flags, PS_SINGLEUNWIND);
		break;
#ifdef DIAGNOSTIC
	default:
		panic("single_thread_mode = %d", mode);
#endif
	}
	pr->ps_singlecount = 0;
	membar_producer();
	pr->ps_single = p;
	TAILQ_FOREACH(q, &pr->ps_threads, p_thr_link) {
		if (q == p)
			continue;
		if (q->p_flag & P_WEXIT) {
			if (mode == SINGLE_EXIT) {
				if (q->p_stat == SSTOP) {
					setrunnable(q);
					atomic_inc_int(&pr->ps_singlecount);
				}
			}
			continue;
		}
		atomic_setbits_int(&q->p_flag, P_SUSPSINGLE);
		switch (q->p_stat) {
		case SIDL:
		case SRUN:
			atomic_inc_int(&pr->ps_singlecount);
			break;
		case SSLEEP:
			/* if it's not interruptible, then just have to wait */
			if (q->p_flag & P_SINTR) {
				/* merely need to suspend?  just stop it */
				if (mode == SINGLE_SUSPEND) {
					q->p_stat = SSTOP;
					break;
				}
				/* need to unwind or exit, so wake it */
				setrunnable(q);
			}
			atomic_inc_int(&pr->ps_singlecount);
			break;
		case SSTOP:
			if (mode == SINGLE_EXIT) {
				setrunnable(q);
				atomic_inc_int(&pr->ps_singlecount);
			}
			break;
		case SDEAD:
			break;
		case SONPROC:
			atomic_inc_int(&pr->ps_singlecount);
			signotify(q);
			break;
		}
	}
	SCHED_UNLOCK(s);

	if (wait)
		single_thread_wait(pr, 1);

	return 0;
}

/*
 * Wait for other threads to stop. If recheck is false then the function
 * returns non-zero if the caller needs to restart the check else 0 is
 * returned. If recheck is true the return value is always 0.
 */
int
single_thread_wait(struct process *pr, int recheck)
{
	struct sleep_state sls;
	int wait;

	/* wait until they're all suspended */
	wait = pr->ps_singlecount > 0;
	while (wait) {
		sleep_setup(&sls, &pr->ps_singlecount, PWAIT, "suspend", 0);
		wait = pr->ps_singlecount > 0;
		sleep_finish(&sls, wait);
		if (!recheck)
			break;
	}

	return wait;
}

void
single_thread_clear(struct proc *p, int flag)
{
	struct process *pr = p->p_p;
	struct proc *q;
	int s;

	KASSERT(pr->ps_single == p);
	KASSERT(curproc == p);

	SCHED_LOCK(s);
	pr->ps_single = NULL;
	atomic_clearbits_int(&pr->ps_flags, PS_SINGLEUNWIND | PS_SINGLEEXIT);
	TAILQ_FOREACH(q, &pr->ps_threads, p_thr_link) {
		if (q == p || (q->p_flag & P_SUSPSINGLE) == 0)
			continue;
		atomic_clearbits_int(&q->p_flag, P_SUSPSINGLE);

		/*
		 * if the thread was only stopped for single threading
		 * then clearing that either makes it runnable or puts
		 * it back into some sleep queue
		 */
		if (q->p_stat == SSTOP && (q->p_flag & flag) == 0) {
			if (q->p_wchan == 0)
				setrunnable(q);
			else
				q->p_stat = SSLEEP;
		}
	}
	SCHED_UNLOCK(s);
}

void
sigio_del(struct sigiolst *rmlist)
{
	struct sigio *sigio;

	while ((sigio = LIST_FIRST(rmlist)) != NULL) {
		LIST_REMOVE(sigio, sio_pgsigio);
		crfree(sigio->sio_ucred);
		free(sigio, M_SIGIO, sizeof(*sigio));
	}
}

void
sigio_unlink(struct sigio_ref *sir, struct sigiolst *rmlist)
{
	struct sigio *sigio;

	MUTEX_ASSERT_LOCKED(&sigio_lock);

	sigio = sir->sir_sigio;
	if (sigio != NULL) {
		KASSERT(sigio->sio_myref == sir);
		sir->sir_sigio = NULL;

		if (sigio->sio_pgid > 0)
			sigio->sio_proc = NULL;
		else
			sigio->sio_pgrp = NULL;
		LIST_REMOVE(sigio, sio_pgsigio);

		LIST_INSERT_HEAD(rmlist, sigio, sio_pgsigio);
	}
}

void
sigio_free(struct sigio_ref *sir)
{
	struct sigiolst rmlist;

	if (sir->sir_sigio == NULL)
		return;

	LIST_INIT(&rmlist);

	mtx_enter(&sigio_lock);
	sigio_unlink(sir, &rmlist);
	mtx_leave(&sigio_lock);

	sigio_del(&rmlist);
}

void
sigio_freelist(struct sigiolst *sigiolst)
{
	struct sigiolst rmlist;
	struct sigio *sigio;

	if (LIST_EMPTY(sigiolst))
		return;

	LIST_INIT(&rmlist);

	mtx_enter(&sigio_lock);
	while ((sigio = LIST_FIRST(sigiolst)) != NULL)
		sigio_unlink(sigio->sio_myref, &rmlist);
	mtx_leave(&sigio_lock);

	sigio_del(&rmlist);
}

int
sigio_setown(struct sigio_ref *sir, u_long cmd, caddr_t data)
{
	struct sigiolst rmlist;
	struct proc *p = curproc;
	struct pgrp *pgrp = NULL;
	struct process *pr = NULL;
	struct sigio *sigio;
	int error;
	pid_t pgid = *(int *)data;

	if (pgid == 0) {
		sigio_free(sir);
		return (0);
	}

	if (cmd == TIOCSPGRP) {
		if (pgid < 0)
			return (EINVAL);
		pgid = -pgid;
	}

	sigio = malloc(sizeof(*sigio), M_SIGIO, M_WAITOK);
	sigio->sio_pgid = pgid;
	sigio->sio_ucred = crhold(p->p_ucred);
	sigio->sio_myref = sir;

	LIST_INIT(&rmlist);

	/*
	 * The kernel lock, and not sleeping between prfind()/pgfind() and
	 * linking of the sigio ensure that the process or process group does
	 * not disappear unexpectedly.
	 */
	KERNEL_LOCK();
	mtx_enter(&sigio_lock);

	if (pgid > 0) {
		pr = prfind(pgid);
		if (pr == NULL) {
			error = ESRCH;
			goto fail;
		}

		/*
		 * Policy - Don't allow a process to FSETOWN a process
		 * in another session.
		 *
		 * Remove this test to allow maximum flexibility or
		 * restrict FSETOWN to the current process or process
		 * group for maximum safety.
		 */
		if (pr->ps_session != p->p_p->ps_session) {
			error = EPERM;
			goto fail;
		}

		if ((pr->ps_flags & PS_EXITING) != 0) {
			error = ESRCH;
			goto fail;
		}
	} else /* if (pgid < 0) */ {
		pgrp = pgfind(-pgid);
		if (pgrp == NULL) {
			error = ESRCH;
			goto fail;
		}

		/*
		 * Policy - Don't allow a process to FSETOWN a process
		 * in another session.
		 *
		 * Remove this test to allow maximum flexibility or
		 * restrict FSETOWN to the current process or process
		 * group for maximum safety.
		 */
		if (pgrp->pg_session != p->p_p->ps_session) {
			error = EPERM;
			goto fail;
		}
	}

	if (pgid > 0) {
		sigio->sio_proc = pr;
		LIST_INSERT_HEAD(&pr->ps_sigiolst, sigio, sio_pgsigio);
	} else {
		sigio->sio_pgrp = pgrp;
		LIST_INSERT_HEAD(&pgrp->pg_sigiolst, sigio, sio_pgsigio);
	}

	sigio_unlink(sir, &rmlist);
	sir->sir_sigio = sigio;

	mtx_leave(&sigio_lock);
	KERNEL_UNLOCK();

	sigio_del(&rmlist);

	return (0);

fail:
	mtx_leave(&sigio_lock);
	KERNEL_UNLOCK();

	crfree(sigio->sio_ucred);
	free(sigio, M_SIGIO, sizeof(*sigio));

	return (error);
}

void
sigio_getown(struct sigio_ref *sir, u_long cmd, caddr_t data)
{
	struct sigio *sigio;
	pid_t pgid = 0;

	mtx_enter(&sigio_lock);
	sigio = sir->sir_sigio;
	if (sigio != NULL)
		pgid = sigio->sio_pgid;
	mtx_leave(&sigio_lock);

	if (cmd == TIOCGPGRP)
		pgid = -pgid;

	*(int *)data = pgid;
}

void
sigio_copy(struct sigio_ref *dst, struct sigio_ref *src)
{
	struct sigiolst rmlist;
	struct sigio *newsigio, *sigio;

	sigio_free(dst);

	if (src->sir_sigio == NULL)
		return;

	newsigio = malloc(sizeof(*newsigio), M_SIGIO, M_WAITOK);
	LIST_INIT(&rmlist);

	mtx_enter(&sigio_lock);

	sigio = src->sir_sigio;
	if (sigio == NULL) {
		mtx_leave(&sigio_lock);
		free(newsigio, M_SIGIO, sizeof(*newsigio));
		return;
	}

	newsigio->sio_pgid = sigio->sio_pgid;
	newsigio->sio_ucred = crhold(sigio->sio_ucred);
	newsigio->sio_myref = dst;
	if (newsigio->sio_pgid > 0) {
		newsigio->sio_proc = sigio->sio_proc;
		LIST_INSERT_HEAD(&newsigio->sio_proc->ps_sigiolst, newsigio,
		    sio_pgsigio);
	} else {
		newsigio->sio_pgrp = sigio->sio_pgrp;
		LIST_INSERT_HEAD(&newsigio->sio_pgrp->pg_sigiolst, newsigio,
		    sio_pgsigio);
	}

	sigio_unlink(dst, &rmlist);
	dst->sir_sigio = newsigio;

	mtx_leave(&sigio_lock);

	sigio_del(&rmlist);
}
