/*	$OpenBSD: kern_event.c,v 1.161 2021/02/24 14:59:52 visa Exp $	*/

/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon <jlemon@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/kern/kern_event.c,v 1.22 2001/02/23 20:32:42 jlemon Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/pledge.h>
#include <sys/malloc.h>
#include <sys/unistd.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/fcntl.h>
#include <sys/selinfo.h>
#include <sys/queue.h>
#include <sys/event.h>
#include <sys/eventvar.h>
#include <sys/ktrace.h>
#include <sys/pool.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/mount.h>
#include <sys/poll.h>
#include <sys/syscallargs.h>
#include <sys/time.h>
#include <sys/timeout.h>
#include <sys/wait.h>

#ifdef DIAGNOSTIC
#define KLIST_ASSERT_LOCKED(kl) do {					\
	if ((kl)->kl_ops != NULL)					\
		(kl)->kl_ops->klo_assertlk((kl)->kl_arg);		\
	else								\
		KERNEL_ASSERT_LOCKED();					\
} while (0)
#else
#define KLIST_ASSERT_LOCKED(kl)	((void)(kl))
#endif

struct	kqueue *kqueue_alloc(struct filedesc *);
void	kqueue_terminate(struct proc *p, struct kqueue *);
void	kqueue_init(void);
void	KQREF(struct kqueue *);
void	KQRELE(struct kqueue *);

int	kqueue_sleep(struct kqueue *, struct timespec *);

int	kqueue_read(struct file *, struct uio *, int);
int	kqueue_write(struct file *, struct uio *, int);
int	kqueue_ioctl(struct file *fp, u_long com, caddr_t data,
		    struct proc *p);
int	kqueue_poll(struct file *fp, int events, struct proc *p);
int	kqueue_kqfilter(struct file *fp, struct knote *kn);
int	kqueue_stat(struct file *fp, struct stat *st, struct proc *p);
int	kqueue_close(struct file *fp, struct proc *p);
void	kqueue_wakeup(struct kqueue *kq);

#ifdef KQUEUE_DEBUG
void	kqueue_do_check(struct kqueue *kq, const char *func, int line);
#define kqueue_check(kq)	kqueue_do_check((kq), __func__, __LINE__)
#else
#define kqueue_check(kq)	do {} while (0)
#endif

void	kqpoll_dequeue(struct proc *p);

static int	filter_attach(struct knote *kn);
static void	filter_detach(struct knote *kn);
static int	filter_event(struct knote *kn, long hint);
static int	filter_modify(struct kevent *kev, struct knote *kn);
static int	filter_process(struct knote *kn, struct kevent *kev);
static void	kqueue_expand_hash(struct kqueue *kq);
static void	kqueue_expand_list(struct kqueue *kq, int fd);
static void	kqueue_task(void *);
static int	klist_lock(struct klist *);
static void	klist_unlock(struct klist *, int);

const struct fileops kqueueops = {
	.fo_read	= kqueue_read,
	.fo_write	= kqueue_write,
	.fo_ioctl	= kqueue_ioctl,
	.fo_poll	= kqueue_poll,
	.fo_kqfilter	= kqueue_kqfilter,
	.fo_stat	= kqueue_stat,
	.fo_close	= kqueue_close
};

void	knote_attach(struct knote *kn);
void	knote_detach(struct knote *kn);
void	knote_drop(struct knote *kn, struct proc *p);
void	knote_enqueue(struct knote *kn);
void	knote_dequeue(struct knote *kn);
int	knote_acquire(struct knote *kn, struct klist *, int);
void	knote_release(struct knote *kn);
void	knote_activate(struct knote *kn);
void	knote_remove(struct proc *p, struct knlist *list, int purge);

void	filt_kqdetach(struct knote *kn);
int	filt_kqueue(struct knote *kn, long hint);
int	filt_procattach(struct knote *kn);
void	filt_procdetach(struct knote *kn);
int	filt_proc(struct knote *kn, long hint);
int	filt_fileattach(struct knote *kn);
void	filt_timerexpire(void *knx);
int	filt_timerattach(struct knote *kn);
void	filt_timerdetach(struct knote *kn);
int	filt_timer(struct knote *kn, long hint);
void	filt_seltruedetach(struct knote *kn);

const struct filterops kqread_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= NULL,
	.f_detach	= filt_kqdetach,
	.f_event	= filt_kqueue,
};

const struct filterops proc_filtops = {
	.f_flags	= 0,
	.f_attach	= filt_procattach,
	.f_detach	= filt_procdetach,
	.f_event	= filt_proc,
};

const struct filterops file_filtops = {
	.f_flags	= FILTEROP_ISFD,
	.f_attach	= filt_fileattach,
	.f_detach	= NULL,
	.f_event	= NULL,
};

const struct filterops timer_filtops = {
	.f_flags	= 0,
	.f_attach	= filt_timerattach,
	.f_detach	= filt_timerdetach,
	.f_event	= filt_timer,
};

struct	pool knote_pool;
struct	pool kqueue_pool;
int kq_ntimeouts = 0;
int kq_timeoutmax = (4 * 1024);

#define KN_HASH(val, mask)	(((val) ^ (val >> 8)) & (mask))

/*
 * Table for for all system-defined filters.
 */
const struct filterops *const sysfilt_ops[] = {
	&file_filtops,			/* EVFILT_READ */
	&file_filtops,			/* EVFILT_WRITE */
	NULL, /*&aio_filtops,*/		/* EVFILT_AIO */
	&file_filtops,			/* EVFILT_VNODE */
	&proc_filtops,			/* EVFILT_PROC */
	&sig_filtops,			/* EVFILT_SIGNAL */
	&timer_filtops,			/* EVFILT_TIMER */
	&file_filtops,			/* EVFILT_DEVICE */
	&file_filtops,			/* EVFILT_EXCEPT */
};

void
KQREF(struct kqueue *kq)
{
	atomic_inc_int(&kq->kq_refs);
}

void
KQRELE(struct kqueue *kq)
{
	struct filedesc *fdp;

	if (atomic_dec_int_nv(&kq->kq_refs) > 0)
		return;

	fdp = kq->kq_fdp;
	if (rw_status(&fdp->fd_lock) == RW_WRITE) {
		LIST_REMOVE(kq, kq_next);
	} else {
		fdplock(fdp);
		LIST_REMOVE(kq, kq_next);
		fdpunlock(fdp);
	}

	KASSERT(TAILQ_EMPTY(&kq->kq_head));

	free(kq->kq_knlist, M_KEVENT, kq->kq_knlistsize *
	    sizeof(struct knlist));
	hashfree(kq->kq_knhash, KN_HASHSIZE, M_KEVENT);
	pool_put(&kqueue_pool, kq);
}

void
kqueue_init(void)
{
	pool_init(&kqueue_pool, sizeof(struct kqueue), 0, IPL_MPFLOOR,
	    PR_WAITOK, "kqueuepl", NULL);
	pool_init(&knote_pool, sizeof(struct knote), 0, IPL_MPFLOOR,
	    PR_WAITOK, "knotepl", NULL);
}

int
filt_fileattach(struct knote *kn)
{
	struct file *fp = kn->kn_fp;

	return fp->f_ops->fo_kqfilter(fp, kn);
}

int
kqueue_kqfilter(struct file *fp, struct knote *kn)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	if (kn->kn_filter != EVFILT_READ)
		return (EINVAL);

	kn->kn_fop = &kqread_filtops;
	klist_insert_locked(&kq->kq_sel.si_note, kn);
	return (0);
}

void
filt_kqdetach(struct knote *kn)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	klist_remove_locked(&kq->kq_sel.si_note, kn);
}

int
filt_kqueue(struct knote *kn, long hint)
{
	struct kqueue *kq = kn->kn_fp->f_data;

	kn->kn_data = kq->kq_count;
	return (kn->kn_data > 0);
}

int
filt_procattach(struct knote *kn)
{
	struct process *pr;
	int s;

	if ((curproc->p_p->ps_flags & PS_PLEDGE) &&
	    (curproc->p_p->ps_pledge & PLEDGE_PROC) == 0)
		return pledge_fail(curproc, EPERM, PLEDGE_PROC);

	if (kn->kn_id > PID_MAX)
		return ESRCH;

	pr = prfind(kn->kn_id);
	if (pr == NULL)
		return (ESRCH);

	/* exiting processes can't be specified */
	if (pr->ps_flags & PS_EXITING)
		return (ESRCH);

	kn->kn_ptr.p_process = pr;
	kn->kn_flags |= EV_CLEAR;		/* automatically set */

	/*
	 * internal flag indicating registration done by kernel
	 */
	if (kn->kn_flags & EV_FLAG1) {
		kn->kn_data = kn->kn_sdata;		/* ppid */
		kn->kn_fflags = NOTE_CHILD;
		kn->kn_flags &= ~EV_FLAG1;
	}

	s = splhigh();
	klist_insert_locked(&pr->ps_klist, kn);
	splx(s);

	return (0);
}

/*
 * The knote may be attached to a different process, which may exit,
 * leaving nothing for the knote to be attached to.  So when the process
 * exits, the knote is marked as DETACHED and also flagged as ONESHOT so
 * it will be deleted when read out.  However, as part of the knote deletion,
 * this routine is called, so a check is needed to avoid actually performing
 * a detach, because the original process does not exist any more.
 */
void
filt_procdetach(struct knote *kn)
{
	struct process *pr = kn->kn_ptr.p_process;
	int s;

	if (kn->kn_status & KN_DETACHED)
		return;

	s = splhigh();
	klist_remove_locked(&pr->ps_klist, kn);
	splx(s);
}

int
filt_proc(struct knote *kn, long hint)
{
	u_int event;

	/*
	 * mask off extra data
	 */
	event = (u_int)hint & NOTE_PCTRLMASK;

	/*
	 * if the user is interested in this event, record it.
	 */
	if (kn->kn_sfflags & event)
		kn->kn_fflags |= event;

	/*
	 * process is gone, so flag the event as finished and remove it
	 * from the process's klist
	 */
	if (event == NOTE_EXIT) {
		struct process *pr = kn->kn_ptr.p_process;
		int s;

		s = splhigh();
		kn->kn_status |= KN_DETACHED;
		kn->kn_flags |= (EV_EOF | EV_ONESHOT);
		kn->kn_data = W_EXITCODE(pr->ps_xexit, pr->ps_xsig);
		klist_remove_locked(&pr->ps_klist, kn);
		splx(s);
		return (1);
	}

	/*
	 * process forked, and user wants to track the new process,
	 * so attach a new knote to it, and immediately report an
	 * event with the parent's pid.
	 */
	if ((event == NOTE_FORK) && (kn->kn_sfflags & NOTE_TRACK)) {
		struct kevent kev;
		int error;

		/*
		 * register knote with new process.
		 */
		memset(&kev, 0, sizeof(kev));
		kev.ident = hint & NOTE_PDATAMASK;	/* pid */
		kev.filter = kn->kn_filter;
		kev.flags = kn->kn_flags | EV_ADD | EV_ENABLE | EV_FLAG1;
		kev.fflags = kn->kn_sfflags;
		kev.data = kn->kn_id;			/* parent */
		kev.udata = kn->kn_udata;		/* preserve udata */
		error = kqueue_register(kn->kn_kq, &kev, NULL);
		if (error)
			kn->kn_fflags |= NOTE_TRACKERR;
	}

	return (kn->kn_fflags != 0);
}

static void
filt_timer_timeout_add(struct knote *kn)
{
	struct timeval tv;
	struct timeout *to = kn->kn_hook;
	int tticks;

	tv.tv_sec = kn->kn_sdata / 1000;
	tv.tv_usec = (kn->kn_sdata % 1000) * 1000;
	tticks = tvtohz(&tv);
	/* Remove extra tick from tvtohz() if timeout has fired before. */
	if (timeout_triggered(to))
		tticks--;
	timeout_add(to, (tticks > 0) ? tticks : 1);
}

void
filt_timerexpire(void *knx)
{
	struct knote *kn = knx;

	kn->kn_data++;
	knote_activate(kn);

	if ((kn->kn_flags & EV_ONESHOT) == 0)
		filt_timer_timeout_add(kn);
}


/*
 * data contains amount of time to sleep, in milliseconds
 */
int
filt_timerattach(struct knote *kn)
{
	struct timeout *to;

	if (kq_ntimeouts > kq_timeoutmax)
		return (ENOMEM);
	kq_ntimeouts++;

	kn->kn_flags |= EV_CLEAR;	/* automatically set */
	to = malloc(sizeof(*to), M_KEVENT, M_WAITOK);
	timeout_set(to, filt_timerexpire, kn);
	kn->kn_hook = to;
	filt_timer_timeout_add(kn);

	return (0);
}

void
filt_timerdetach(struct knote *kn)
{
	struct timeout *to;

	to = (struct timeout *)kn->kn_hook;
	timeout_del(to);
	free(to, M_KEVENT, sizeof(*to));
	kq_ntimeouts--;
}

int
filt_timer(struct knote *kn, long hint)
{
	return (kn->kn_data != 0);
}


/*
 * filt_seltrue:
 *
 *	This filter "event" routine simulates seltrue().
 */
int
filt_seltrue(struct knote *kn, long hint)
{

	/*
	 * We don't know how much data can be read/written,
	 * but we know that it *can* be.  This is about as
	 * good as select/poll does as well.
	 */
	kn->kn_data = 0;
	return (1);
}

int
filt_seltruemodify(struct kevent *kev, struct knote *kn)
{
	knote_modify(kev, kn);
	return (1);
}

int
filt_seltrueprocess(struct knote *kn, struct kevent *kev)
{
	knote_submit(kn, kev);
	return (1);
}

/*
 * This provides full kqfilter entry for device switch tables, which
 * has same effect as filter using filt_seltrue() as filter method.
 */
void
filt_seltruedetach(struct knote *kn)
{
	/* Nothing to do */
}

const struct filterops seltrue_filtops = {
	.f_flags	= FILTEROP_ISFD | FILTEROP_MPSAFE,
	.f_attach	= NULL,
	.f_detach	= filt_seltruedetach,
	.f_event	= filt_seltrue,
	.f_modify	= filt_seltruemodify,
	.f_process	= filt_seltrueprocess,
};

int
seltrue_kqfilter(dev_t dev, struct knote *kn)
{
	switch (kn->kn_filter) {
	case EVFILT_READ:
	case EVFILT_WRITE:
		kn->kn_fop = &seltrue_filtops;
		break;
	default:
		return (EINVAL);
	}

	/* Nothing more to do */
	return (0);
}

static int
filt_dead(struct knote *kn, long hint)
{
	kn->kn_flags |= (EV_EOF | EV_ONESHOT);
	if (kn->kn_flags & __EV_POLL)
		kn->kn_flags |= __EV_HUP;
	kn->kn_data = 0;
	return (1);
}

static void
filt_deaddetach(struct knote *kn)
{
	/* Nothing to do */
}

const struct filterops dead_filtops = {
	.f_flags	= FILTEROP_ISFD | FILTEROP_MPSAFE,
	.f_attach	= NULL,
	.f_detach	= filt_deaddetach,
	.f_event	= filt_dead,
	.f_modify	= filt_seltruemodify,
	.f_process	= filt_seltrueprocess,
};

static int
filt_badfd(struct knote *kn, long hint)
{
	kn->kn_flags |= (EV_ERROR | EV_ONESHOT);
	kn->kn_data = EBADF;
	return (1);
}

/* For use with kqpoll. */
const struct filterops badfd_filtops = {
	.f_flags	= FILTEROP_ISFD | FILTEROP_MPSAFE,
	.f_attach	= NULL,
	.f_detach	= filt_deaddetach,
	.f_event	= filt_badfd,
	.f_modify	= filt_seltruemodify,
	.f_process	= filt_seltrueprocess,
};

static int
filter_attach(struct knote *kn)
{
	int error;

	if (kn->kn_fop->f_flags & FILTEROP_MPSAFE) {
		error = kn->kn_fop->f_attach(kn);
	} else {
		KERNEL_LOCK();
		error = kn->kn_fop->f_attach(kn);
		KERNEL_UNLOCK();
	}
	return (error);
}

static void
filter_detach(struct knote *kn)
{
	if (kn->kn_fop->f_flags & FILTEROP_MPSAFE) {
		kn->kn_fop->f_detach(kn);
	} else {
		KERNEL_LOCK();
		kn->kn_fop->f_detach(kn);
		KERNEL_UNLOCK();
	}
}

static int
filter_event(struct knote *kn, long hint)
{
	if ((kn->kn_fop->f_flags & FILTEROP_MPSAFE) == 0)
		KERNEL_ASSERT_LOCKED();

	return (kn->kn_fop->f_event(kn, hint));
}

static int
filter_modify(struct kevent *kev, struct knote *kn)
{
	int active, s;

	if (kn->kn_fop->f_flags & FILTEROP_MPSAFE) {
		active = kn->kn_fop->f_modify(kev, kn);
	} else {
		KERNEL_LOCK();
		if (kn->kn_fop->f_modify != NULL) {
			active = kn->kn_fop->f_modify(kev, kn);
		} else {
			/* Emulate f_modify using f_event. */
			s = splhigh();
			knote_modify(kev, kn);
			active = kn->kn_fop->f_event(kn, 0);
			splx(s);
		}
		KERNEL_UNLOCK();
	}
	return (active);
}

static int
filter_process(struct knote *kn, struct kevent *kev)
{
	int active, s;

	if (kn->kn_fop->f_flags & FILTEROP_MPSAFE) {
		active = kn->kn_fop->f_process(kn, kev);
	} else {
		KERNEL_LOCK();
		if (kn->kn_fop->f_process != NULL) {
			active = kn->kn_fop->f_process(kn, kev);
		} else {
			/* Emulate f_process using f_event. */
			s = splhigh();
			/*
			 * If called from kqueue_scan(), skip f_event
			 * when EV_ONESHOT is set, to preserve old behaviour.
			 */
			if (kev != NULL && (kn->kn_flags & EV_ONESHOT))
				active = 1;
			else
				active = kn->kn_fop->f_event(kn, 0);
			if (active)
				knote_submit(kn, kev);
			splx(s);
		}
		KERNEL_UNLOCK();
	}
	return (active);
}

void
kqpoll_init(void)
{
	struct proc *p = curproc;
	struct filedesc *fdp;

	if (p->p_kq != NULL) {
		/*
		 * Discard any knotes that have been enqueued after
		 * previous scan.
		 * This prevents accumulation of enqueued badfd knotes
		 * in case scan does not make progress for some reason.
		 */
		kqpoll_dequeue(p);
		return;
	}

	p->p_kq = kqueue_alloc(p->p_fd);
	p->p_kq_serial = arc4random();
	fdp = p->p_fd;
	fdplock(fdp);
	LIST_INSERT_HEAD(&fdp->fd_kqlist, p->p_kq, kq_next);
	fdpunlock(fdp);
}

void
kqpoll_exit(void)
{
	struct proc *p = curproc;

	if (p->p_kq == NULL)
		return;

	kqueue_purge(p, p->p_kq);
	/* Clear any detached knotes that remain in the queue. */
	kqpoll_dequeue(p);
	kqueue_terminate(p, p->p_kq);
	KASSERT(p->p_kq->kq_refs == 1);
	KQRELE(p->p_kq);
	p->p_kq = NULL;
}

void
kqpoll_dequeue(struct proc *p)
{
	struct knote *kn;
	struct kqueue *kq = p->p_kq;
	int s;

	s = splhigh();
	while ((kn = TAILQ_FIRST(&kq->kq_head)) != NULL) {
		/* This kqueue should not be scanned by other threads. */
		KASSERT(kn->kn_filter != EVFILT_MARKER);

		if (!knote_acquire(kn, NULL, 0))
			continue;

		kqueue_check(kq);
		TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
		kn->kn_status &= ~KN_QUEUED;
		kq->kq_count--;

		splx(s);
		kn->kn_fop->f_detach(kn);
		knote_drop(kn, p);
		s = splhigh();
		kqueue_check(kq);
	}
	splx(s);
}

struct kqueue *
kqueue_alloc(struct filedesc *fdp)
{
	struct kqueue *kq;

	kq = pool_get(&kqueue_pool, PR_WAITOK | PR_ZERO);
	kq->kq_refs = 1;
	kq->kq_fdp = fdp;
	TAILQ_INIT(&kq->kq_head);
	task_set(&kq->kq_task, kqueue_task, kq);

	return (kq);
}

int
sys_kqueue(struct proc *p, void *v, register_t *retval)
{
	struct filedesc *fdp = p->p_fd;
	struct kqueue *kq;
	struct file *fp;
	int fd, error;

	kq = kqueue_alloc(fdp);

	fdplock(fdp);
	error = falloc(p, &fp, &fd);
	if (error)
		goto out;
	fp->f_flag = FREAD | FWRITE;
	fp->f_type = DTYPE_KQUEUE;
	fp->f_ops = &kqueueops;
	fp->f_data = kq;
	*retval = fd;
	LIST_INSERT_HEAD(&fdp->fd_kqlist, kq, kq_next);
	kq = NULL;
	fdinsert(fdp, fd, 0, fp);
	FRELE(fp, p);
out:
	fdpunlock(fdp);
	if (kq != NULL)
		pool_put(&kqueue_pool, kq);
	return (error);
}

int
sys_kevent(struct proc *p, void *v, register_t *retval)
{
	struct kqueue_scan_state scan;
	struct filedesc* fdp = p->p_fd;
	struct sys_kevent_args /* {
		syscallarg(int)	fd;
		syscallarg(const struct kevent *) changelist;
		syscallarg(int)	nchanges;
		syscallarg(struct kevent *) eventlist;
		syscallarg(int)	nevents;
		syscallarg(const struct timespec *) timeout;
	} */ *uap = v;
	struct kevent *kevp;
	struct kqueue *kq;
	struct file *fp;
	struct timespec ts;
	struct timespec *tsp = NULL;
	int i, n, nerrors, error;
	int ready, total;
	struct kevent kev[KQ_NEVENTS];

	if ((fp = fd_getfile(fdp, SCARG(uap, fd))) == NULL)
		return (EBADF);

	if (fp->f_type != DTYPE_KQUEUE) {
		error = EBADF;
		goto done;
	}

	if (SCARG(uap, timeout) != NULL) {
		error = copyin(SCARG(uap, timeout), &ts, sizeof(ts));
		if (error)
			goto done;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrreltimespec(p, &ts);
#endif
		if (ts.tv_sec < 0 || !timespecisvalid(&ts)) {
			error = EINVAL;
			goto done;
		}
		tsp = &ts;
	}

	kq = fp->f_data;
	nerrors = 0;

	while ((n = SCARG(uap, nchanges)) > 0) {
		if (n > nitems(kev))
			n = nitems(kev);
		error = copyin(SCARG(uap, changelist), kev,
		    n * sizeof(struct kevent));
		if (error)
			goto done;
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrevent(p, kev, n);
#endif
		for (i = 0; i < n; i++) {
			kevp = &kev[i];
			kevp->flags &= ~EV_SYSFLAGS;
			error = kqueue_register(kq, kevp, p);
			if (error || (kevp->flags & EV_RECEIPT)) {
				if (SCARG(uap, nevents) != 0) {
					kevp->flags = EV_ERROR;
					kevp->data = error;
					copyout(kevp, SCARG(uap, eventlist),
					    sizeof(*kevp));
					SCARG(uap, eventlist)++;
					SCARG(uap, nevents)--;
					nerrors++;
				} else {
					goto done;
				}
			}
		}
		SCARG(uap, nchanges) -= n;
		SCARG(uap, changelist) += n;
	}
	if (nerrors) {
		*retval = nerrors;
		error = 0;
		goto done;
	}

	kqueue_scan_setup(&scan, kq);
	FRELE(fp, p);
	/*
	 * Collect as many events as we can.  The timeout on successive
	 * loops is disabled (kqueue_scan() becomes non-blocking).
	 */
	total = 0;
	error = 0;
	while ((n = SCARG(uap, nevents) - total) > 0) {
		if (n > nitems(kev))
			n = nitems(kev);
		ready = kqueue_scan(&scan, n, kev, tsp, p, &error);
		if (ready == 0)
			break;
		error = copyout(kev, SCARG(uap, eventlist) + total,
		    sizeof(struct kevent) * ready);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_STRUCT))
			ktrevent(p, kev, ready);
#endif
		total += ready;
		if (error || ready < n)
			break;
	}
	kqueue_scan_finish(&scan);
	*retval = total;
	return (error);

 done:
	FRELE(fp, p);
	return (error);
}

#ifdef KQUEUE_DEBUG
void
kqueue_do_check(struct kqueue *kq, const char *func, int line)
{
	struct knote *kn;
	int count = 0, nmarker = 0;

	KERNEL_ASSERT_LOCKED();
	splassert(IPL_HIGH);

	TAILQ_FOREACH(kn, &kq->kq_head, kn_tqe) {
		if (kn->kn_filter == EVFILT_MARKER) {
			if ((kn->kn_status & KN_QUEUED) != 0)
				panic("%s:%d: kq=%p kn=%p marker QUEUED",
				    func, line, kq, kn);
			nmarker++;
		} else {
			if ((kn->kn_status & KN_ACTIVE) == 0)
				panic("%s:%d: kq=%p kn=%p knote !ACTIVE",
				    func, line, kq, kn);
			if ((kn->kn_status & KN_QUEUED) == 0)
				panic("%s:%d: kq=%p kn=%p knote !QUEUED",
				    func, line, kq, kn);
			if (kn->kn_kq != kq)
				panic("%s:%d: kq=%p kn=%p kn_kq=%p != kq",
				    func, line, kq, kn, kn->kn_kq);
			count++;
			if (count > kq->kq_count)
				goto bad;
		}
	}
	if (count != kq->kq_count) {
bad:
		panic("%s:%d: kq=%p kq_count=%d count=%d nmarker=%d",
		    func, line, kq, kq->kq_count, count, nmarker);
	}
}
#endif

int
kqueue_register(struct kqueue *kq, struct kevent *kev, struct proc *p)
{
	struct filedesc *fdp = kq->kq_fdp;
	const struct filterops *fops = NULL;
	struct file *fp = NULL;
	struct knote *kn = NULL, *newkn = NULL;
	struct knlist *list = NULL;
	int s, error = 0;

	if (kev->filter < 0) {
		if (kev->filter + EVFILT_SYSCOUNT < 0)
			return (EINVAL);
		fops = sysfilt_ops[~kev->filter];	/* to 0-base index */
	}

	if (fops == NULL) {
		/*
		 * XXX
		 * filter attach routine is responsible for ensuring that
		 * the identifier can be attached to it.
		 */
		return (EINVAL);
	}

	if (fops->f_flags & FILTEROP_ISFD) {
		/* validate descriptor */
		if (kev->ident > INT_MAX)
			return (EBADF);
	}

	if (kev->flags & EV_ADD)
		newkn = pool_get(&knote_pool, PR_WAITOK | PR_ZERO);

again:
	if (fops->f_flags & FILTEROP_ISFD) {
		if ((fp = fd_getfile(fdp, kev->ident)) == NULL) {
			error = EBADF;
			goto done;
		}
		if (kev->flags & EV_ADD)
			kqueue_expand_list(kq, kev->ident);
		if (kev->ident < kq->kq_knlistsize)
			list = &kq->kq_knlist[kev->ident];
	} else {
		if (kev->flags & EV_ADD)
			kqueue_expand_hash(kq);
		if (kq->kq_knhashmask != 0) {
			list = &kq->kq_knhash[
			    KN_HASH((u_long)kev->ident, kq->kq_knhashmask)];
		}
	}
	if (list != NULL) {
		SLIST_FOREACH(kn, list, kn_link) {
			if (kev->filter == kn->kn_filter &&
			    kev->ident == kn->kn_id) {
				s = splhigh();
				if (!knote_acquire(kn, NULL, 0)) {
					splx(s);
					if (fp != NULL) {
						FRELE(fp, p);
						fp = NULL;
					}
					goto again;
				}
				splx(s);
				break;
			}
		}
	}
	KASSERT(kn == NULL || (kn->kn_status & KN_PROCESSING) != 0);

	if (kn == NULL && ((kev->flags & EV_ADD) == 0)) {
		error = ENOENT;
		goto done;
	}

	/*
	 * kn now contains the matching knote, or NULL if no match.
	 * If adding a new knote, sleeping is not allowed until the knote
	 * has been inserted.
	 */
	if (kev->flags & EV_ADD) {
		if (kn == NULL) {
			kn = newkn;
			newkn = NULL;
			kn->kn_status = KN_PROCESSING;
			kn->kn_fp = fp;
			kn->kn_kq = kq;
			kn->kn_fop = fops;

			/*
			 * apply reference count to knote structure, and
			 * do not release it at the end of this routine.
			 */
			fp = NULL;

			kn->kn_sfflags = kev->fflags;
			kn->kn_sdata = kev->data;
			kev->fflags = 0;
			kev->data = 0;
			kn->kn_kevent = *kev;

			knote_attach(kn);
			error = filter_attach(kn);
			if (error != 0) {
				knote_drop(kn, p);
				goto done;
			}

			/*
			 * If this is a file descriptor filter, check if
			 * fd was closed while the knote was being added.
			 * knote_fdclose() has missed kn if the function
			 * ran before kn appeared in kq_knlist.
			 */
			if ((fops->f_flags & FILTEROP_ISFD) &&
			    fd_checkclosed(fdp, kev->ident, kn->kn_fp)) {
				/*
				 * Drop the knote silently without error
				 * because another thread might already have
				 * seen it. This corresponds to the insert
				 * happening in full before the close.
				 */
				filter_detach(kn);
				knote_drop(kn, p);
				goto done;
			}

			/* Check if there is a pending event. */
			if (filter_process(kn, NULL))
				knote_activate(kn);
		} else {
			/*
			 * The user may change some filter values after the
			 * initial EV_ADD, but doing so will not reset any
			 * filters which have already been triggered.
			 */
			if (filter_modify(kev, kn))
				knote_activate(kn);
			if (kev->flags & EV_ERROR) {
				error = kev->data;
				goto release;
			}
		}
	} else if (kev->flags & EV_DELETE) {
		filter_detach(kn);
		knote_drop(kn, p);
		goto done;
	}

	if ((kev->flags & EV_DISABLE) &&
	    ((kn->kn_status & KN_DISABLED) == 0)) {
		s = splhigh();
		kn->kn_status |= KN_DISABLED;
		splx(s);
	}

	if ((kev->flags & EV_ENABLE) && (kn->kn_status & KN_DISABLED)) {
		s = splhigh();
		kn->kn_status &= ~KN_DISABLED;
		splx(s);
		/* Check if there is a pending event. */
		if (filter_process(kn, NULL))
			knote_activate(kn);
	}

release:
	s = splhigh();
	knote_release(kn);
	splx(s);
done:
	if (fp != NULL)
		FRELE(fp, p);
	if (newkn != NULL)
		pool_put(&knote_pool, newkn);
	return (error);
}

int
kqueue_sleep(struct kqueue *kq, struct timespec *tsp)
{
	struct timespec elapsed, start, stop;
	uint64_t nsecs;
	int error;

	splassert(IPL_HIGH);

	if (tsp != NULL) {
		getnanouptime(&start);
		nsecs = MIN(TIMESPEC_TO_NSEC(tsp), MAXTSLP);
	} else
		nsecs = INFSLP;
	error = tsleep_nsec(kq, PSOCK | PCATCH, "kqread", nsecs);
	if (tsp != NULL) {
		getnanouptime(&stop);
		timespecsub(&stop, &start, &elapsed);
		timespecsub(tsp, &elapsed, tsp);
		if (tsp->tv_sec < 0)
			timespecclear(tsp);
	}

	return (error);
}

/*
 * Scan the kqueue, blocking if necessary until the target time is reached.
 * If tsp is NULL we block indefinitely.  If tsp->ts_secs/nsecs are both
 * 0 we do not block at all.
 */
int
kqueue_scan(struct kqueue_scan_state *scan, int maxevents,
    struct kevent *kevp, struct timespec *tsp, struct proc *p, int *errorp)
{
	struct kqueue *kq = scan->kqs_kq;
	struct knote *kn;
	int s, error = 0, nkev = 0;

	if (maxevents == 0)
		goto done;
retry:
	KASSERT(nkev == 0);

	error = 0;

	if (kq->kq_state & KQ_DYING) {
		error = EBADF;
		goto done;
	}

	s = splhigh();
	if (kq->kq_count == 0) {
		/*
		 * Successive loops are only necessary if there are more
		 * ready events to gather, so they don't need to block.
		 */
		if ((tsp != NULL && !timespecisset(tsp)) ||
		    scan->kqs_nevent != 0) {
			splx(s);
			error = 0;
			goto done;
		}
		kq->kq_state |= KQ_SLEEP;
		error = kqueue_sleep(kq, tsp);
		splx(s);
		if (error == 0 || error == EWOULDBLOCK)
			goto retry;
		/* don't restart after signals... */
		if (error == ERESTART)
			error = EINTR;
		goto done;
	}

	/*
	 * Put the end marker in the queue to limit the scan to the events
	 * that are currently active.  This prevents events from being
	 * recollected if they reactivate during scan.
	 *
	 * If a partial scan has been performed already but no events have
	 * been collected, reposition the end marker to make any new events
	 * reachable.
	 */
	if (!scan->kqs_queued) {
		TAILQ_INSERT_TAIL(&kq->kq_head, &scan->kqs_end, kn_tqe);
		scan->kqs_queued = 1;
	} else if (scan->kqs_nevent == 0) {
		TAILQ_REMOVE(&kq->kq_head, &scan->kqs_end, kn_tqe);
		TAILQ_INSERT_TAIL(&kq->kq_head, &scan->kqs_end, kn_tqe);
	}

	TAILQ_INSERT_HEAD(&kq->kq_head, &scan->kqs_start, kn_tqe);
	while (nkev < maxevents) {
		kn = TAILQ_NEXT(&scan->kqs_start, kn_tqe);
		if (kn->kn_filter == EVFILT_MARKER) {
			if (kn == &scan->kqs_end)
				break;

			/* Move start marker past another thread's marker. */
			TAILQ_REMOVE(&kq->kq_head, &scan->kqs_start, kn_tqe);
			TAILQ_INSERT_AFTER(&kq->kq_head, kn, &scan->kqs_start,
			    kn_tqe);
			continue;
		}

		if (!knote_acquire(kn, NULL, 0))
			continue;

		kqueue_check(kq);
		TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
		kn->kn_status &= ~KN_QUEUED;
		kq->kq_count--;
		kqueue_check(kq);

		if (kn->kn_status & KN_DISABLED) {
			knote_release(kn);
			continue;
		}

		splx(s);

		memset(kevp, 0, sizeof(*kevp));
		if (filter_process(kn, kevp) == 0) {
			s = splhigh();
			if ((kn->kn_status & KN_QUEUED) == 0)
				kn->kn_status &= ~KN_ACTIVE;
			knote_release(kn);
			kqueue_check(kq);
			continue;
		}

		/*
		 * Post-event action on the note
		 */
		if (kevp->flags & EV_ONESHOT) {
			filter_detach(kn);
			knote_drop(kn, p);
			s = splhigh();
		} else if (kevp->flags & (EV_CLEAR | EV_DISPATCH)) {
			s = splhigh();
			if (kevp->flags & EV_DISPATCH)
				kn->kn_status |= KN_DISABLED;
			if ((kn->kn_status & KN_QUEUED) == 0)
				kn->kn_status &= ~KN_ACTIVE;
			KASSERT(kn->kn_status & KN_ATTACHED);
			knote_release(kn);
		} else {
			s = splhigh();
			if ((kn->kn_status & KN_QUEUED) == 0) {
				kqueue_check(kq);
				kq->kq_count++;
				kn->kn_status |= KN_QUEUED;
				TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
			}
			KASSERT(kn->kn_status & KN_ATTACHED);
			knote_release(kn);
		}
		kqueue_check(kq);

		kevp++;
		nkev++;
		scan->kqs_nevent++;
	}
	TAILQ_REMOVE(&kq->kq_head, &scan->kqs_start, kn_tqe);
	splx(s);
	if (scan->kqs_nevent == 0)
		goto retry;
done:
	*errorp = error;
	return (nkev);
}

void
kqueue_scan_setup(struct kqueue_scan_state *scan, struct kqueue *kq)
{
	memset(scan, 0, sizeof(*scan));

	KQREF(kq);
	scan->kqs_kq = kq;
	scan->kqs_start.kn_filter = EVFILT_MARKER;
	scan->kqs_start.kn_status = KN_PROCESSING;
	scan->kqs_end.kn_filter = EVFILT_MARKER;
	scan->kqs_end.kn_status = KN_PROCESSING;
}

void
kqueue_scan_finish(struct kqueue_scan_state *scan)
{
	struct kqueue *kq = scan->kqs_kq;
	int s;

	KASSERT(scan->kqs_start.kn_filter == EVFILT_MARKER);
	KASSERT(scan->kqs_start.kn_status == KN_PROCESSING);
	KASSERT(scan->kqs_end.kn_filter == EVFILT_MARKER);
	KASSERT(scan->kqs_end.kn_status == KN_PROCESSING);

	if (scan->kqs_queued) {
		scan->kqs_queued = 0;
		s = splhigh();
		TAILQ_REMOVE(&kq->kq_head, &scan->kqs_end, kn_tqe);
		splx(s);
	}
	KQRELE(kq);
}

/*
 * XXX
 * This could be expanded to call kqueue_scan, if desired.
 */
int
kqueue_read(struct file *fp, struct uio *uio, int fflags)
{
	return (ENXIO);
}

int
kqueue_write(struct file *fp, struct uio *uio, int fflags)
{
	return (ENXIO);
}

int
kqueue_ioctl(struct file *fp, u_long com, caddr_t data, struct proc *p)
{
	return (ENOTTY);
}

int
kqueue_poll(struct file *fp, int events, struct proc *p)
{
	struct kqueue *kq = (struct kqueue *)fp->f_data;
	int revents = 0;
	int s = splhigh();

	if (events & (POLLIN | POLLRDNORM)) {
		if (kq->kq_count) {
			revents |= events & (POLLIN | POLLRDNORM);
		} else {
			selrecord(p, &kq->kq_sel);
			kq->kq_state |= KQ_SEL;
		}
	}
	splx(s);
	return (revents);
}

int
kqueue_stat(struct file *fp, struct stat *st, struct proc *p)
{
	struct kqueue *kq = fp->f_data;

	memset(st, 0, sizeof(*st));
	st->st_size = kq->kq_count;
	st->st_blksize = sizeof(struct kevent);
	st->st_mode = S_IFIFO;
	return (0);
}

void
kqueue_purge(struct proc *p, struct kqueue *kq)
{
	int i;

	KERNEL_ASSERT_LOCKED();

	for (i = 0; i < kq->kq_knlistsize; i++)
		knote_remove(p, &kq->kq_knlist[i], 1);
	if (kq->kq_knhashmask != 0) {
		for (i = 0; i < kq->kq_knhashmask + 1; i++)
			knote_remove(p, &kq->kq_knhash[i], 1);
	}
}

void
kqueue_terminate(struct proc *p, struct kqueue *kq)
{
	struct knote *kn;

	/*
	 * Any remaining entries should be scan markers.
	 * They are removed when the ongoing scans finish.
	 */
	KASSERT(kq->kq_count == 0);
	TAILQ_FOREACH(kn, &kq->kq_head, kn_tqe)
		KASSERT(kn->kn_filter == EVFILT_MARKER);

	kq->kq_state |= KQ_DYING;
	kqueue_wakeup(kq);

	KASSERT(klist_empty(&kq->kq_sel.si_note));
	task_del(systq, &kq->kq_task);

}

int
kqueue_close(struct file *fp, struct proc *p)
{
	struct kqueue *kq = fp->f_data;

	KERNEL_LOCK();
	kqueue_purge(p, kq);
	kqueue_terminate(p, kq);
	fp->f_data = NULL;

	KQRELE(kq);

	KERNEL_UNLOCK();

	return (0);
}

static void
kqueue_task(void *arg)
{
	struct kqueue *kq = arg;

	if (kq->kq_state & KQ_SEL) {
		kq->kq_state &= ~KQ_SEL;
		selwakeup(&kq->kq_sel);
	} else {
		KNOTE(&kq->kq_sel.si_note, 0);
	}
	KQRELE(kq);
}

void
kqueue_wakeup(struct kqueue *kq)
{

	if (kq->kq_state & KQ_SLEEP) {
		kq->kq_state &= ~KQ_SLEEP;
		wakeup(kq);
	}
	if ((kq->kq_state & KQ_SEL) || !klist_empty(&kq->kq_sel.si_note)) {
		/* Defer activation to avoid recursion. */
		KQREF(kq);
		if (!task_add(systq, &kq->kq_task))
			KQRELE(kq);
	}
}

static void
kqueue_expand_hash(struct kqueue *kq)
{
	struct knlist *hash;
	u_long hashmask;

	if (kq->kq_knhashmask == 0) {
		hash = hashinit(KN_HASHSIZE, M_KEVENT, M_WAITOK, &hashmask);
		if (kq->kq_knhashmask == 0) {
			kq->kq_knhash = hash;
			kq->kq_knhashmask = hashmask;
		} else {
			/* Another thread has allocated the hash. */
			hashfree(hash, KN_HASHSIZE, M_KEVENT);
		}
	}
}

static void
kqueue_expand_list(struct kqueue *kq, int fd)
{
	struct knlist *list;
	int size;

	if (kq->kq_knlistsize <= fd) {
		size = kq->kq_knlistsize;
		while (size <= fd)
			size += KQEXTENT;
		list = mallocarray(size, sizeof(*list), M_KEVENT, M_WAITOK);
		if (kq->kq_knlistsize <= fd) {
			memcpy(list, kq->kq_knlist,
			    kq->kq_knlistsize * sizeof(*list));
			memset(&list[kq->kq_knlistsize], 0,
			    (size - kq->kq_knlistsize) * sizeof(*list));
			free(kq->kq_knlist, M_KEVENT,
			    kq->kq_knlistsize * sizeof(*list));
			kq->kq_knlist = list;
			kq->kq_knlistsize = size;
		} else {
			/* Another thread has expanded the list. */
			free(list, M_KEVENT, size * sizeof(*list));
		}
	}
}

/*
 * Acquire a knote, return non-zero on success, 0 on failure.
 *
 * If we cannot acquire the knote we sleep and return 0.  The knote
 * may be stale on return in this case and the caller must restart
 * whatever loop they are in.
 *
 * If we are about to sleep and klist is non-NULL, the list is unlocked
 * before sleep and remains unlocked on return.
 */
int
knote_acquire(struct knote *kn, struct klist *klist, int ls)
{
	splassert(IPL_HIGH);
	KASSERT(kn->kn_filter != EVFILT_MARKER);

	if (kn->kn_status & KN_PROCESSING) {
		kn->kn_status |= KN_WAITING;
		if (klist != NULL)
			klist_unlock(klist, ls);
		tsleep_nsec(kn, 0, "kqepts", SEC_TO_NSEC(1));
		/* knote may be stale now */
		return (0);
	}
	kn->kn_status |= KN_PROCESSING;
	return (1);
}

/*
 * Release an acquired knote, clearing KN_PROCESSING.
 */
void
knote_release(struct knote *kn)
{
	splassert(IPL_HIGH);
	KASSERT(kn->kn_filter != EVFILT_MARKER);
	KASSERT(kn->kn_status & KN_PROCESSING);

	if (kn->kn_status & KN_WAITING) {
		kn->kn_status &= ~KN_WAITING;
		wakeup(kn);
	}
	kn->kn_status &= ~KN_PROCESSING;
	/* kn should not be accessed anymore */
}

/*
 * activate one knote.
 */
void
knote_activate(struct knote *kn)
{
	int s;

	s = splhigh();
	kn->kn_status |= KN_ACTIVE;
	if ((kn->kn_status & (KN_QUEUED | KN_DISABLED)) == 0)
		knote_enqueue(kn);
	splx(s);
}

/*
 * walk down a list of knotes, activating them if their event has triggered.
 */
void
knote(struct klist *list, long hint)
{
	struct knote *kn, *kn0;

	KLIST_ASSERT_LOCKED(list);

	SLIST_FOREACH_SAFE(kn, &list->kl_list, kn_selnext, kn0)
		if (filter_event(kn, hint))
			knote_activate(kn);
}

/*
 * remove all knotes from a specified knlist
 */
void
knote_remove(struct proc *p, struct knlist *list, int purge)
{
	struct knote *kn;
	int s;

	while ((kn = SLIST_FIRST(list)) != NULL) {
		s = splhigh();
		if (!knote_acquire(kn, NULL, 0)) {
			splx(s);
			continue;
		}
		splx(s);
		filter_detach(kn);

		/*
		 * Notify poll(2) and select(2) when a monitored
		 * file descriptor is closed.
		 *
		 * This reuses the original knote for delivering the
		 * notification so as to avoid allocating memory.
		 * The knote will be reachable only through the queue
		 * of active knotes and is freed either by kqueue_scan()
		 * or kqpoll_dequeue().
		 */
		if (!purge && (kn->kn_flags & __EV_POLL) != 0) {
			KASSERT(kn->kn_fop->f_flags & FILTEROP_ISFD);
			knote_detach(kn);
			FRELE(kn->kn_fp, p);
			kn->kn_fp = NULL;

			kn->kn_fop = &badfd_filtops;
			filter_event(kn, 0);
			knote_activate(kn);
			s = splhigh();
			knote_release(kn);
			splx(s);
			continue;
		}

		knote_drop(kn, p);
	}
}

/*
 * remove all knotes referencing a specified fd
 */
void
knote_fdclose(struct proc *p, int fd)
{
	struct filedesc *fdp = p->p_p->ps_fd;
	struct kqueue *kq;
	struct knlist *list;

	/*
	 * fdplock can be ignored if the file descriptor table is being freed
	 * because no other thread can access the fdp.
	 */
	if (fdp->fd_refcnt != 0)
		fdpassertlocked(fdp);

	if (LIST_EMPTY(&fdp->fd_kqlist))
		return;

	KERNEL_LOCK();
	LIST_FOREACH(kq, &fdp->fd_kqlist, kq_next) {
		if (fd >= kq->kq_knlistsize)
			continue;

		list = &kq->kq_knlist[fd];
		knote_remove(p, list, 0);
	}
	KERNEL_UNLOCK();
}

/*
 * handle a process exiting, including the triggering of NOTE_EXIT notes
 * XXX this could be more efficient, doing a single pass down the klist
 */
void
knote_processexit(struct proc *p)
{
	struct process *pr = p->p_p;

	KASSERT(p == curproc);

	KNOTE(&pr->ps_klist, NOTE_EXIT);

	/* remove other knotes hanging off the process */
	klist_invalidate(&pr->ps_klist);
}

void
knote_attach(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;
	struct knlist *list;
	int s;

	KASSERT(kn->kn_status & KN_PROCESSING);
	KASSERT((kn->kn_status & KN_ATTACHED) == 0);

	s = splhigh();
	kn->kn_status |= KN_ATTACHED;
	splx(s);

	if (kn->kn_fop->f_flags & FILTEROP_ISFD) {
		KASSERT(kq->kq_knlistsize > kn->kn_id);
		list = &kq->kq_knlist[kn->kn_id];
	} else {
		KASSERT(kq->kq_knhashmask != 0);
		list = &kq->kq_knhash[KN_HASH(kn->kn_id, kq->kq_knhashmask)];
	}
	SLIST_INSERT_HEAD(list, kn, kn_link);
}

void
knote_detach(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;
	struct knlist *list;
	int s;

	KASSERT(kn->kn_status & KN_PROCESSING);

	if ((kn->kn_status & KN_ATTACHED) == 0)
		return;

	if (kn->kn_fop->f_flags & FILTEROP_ISFD)
		list = &kq->kq_knlist[kn->kn_id];
	else
		list = &kq->kq_knhash[KN_HASH(kn->kn_id, kq->kq_knhashmask)];
	SLIST_REMOVE(list, kn, knote, kn_link);

	s = splhigh();
	kn->kn_status &= ~KN_ATTACHED;
	splx(s);
}

/*
 * should be called at spl == 0, since we don't want to hold spl
 * while calling FRELE and pool_put.
 */
void
knote_drop(struct knote *kn, struct proc *p)
{
	int s;

	KASSERT(kn->kn_filter != EVFILT_MARKER);

	knote_detach(kn);

	s = splhigh();
	if (kn->kn_status & KN_QUEUED)
		knote_dequeue(kn);
	if (kn->kn_status & KN_WAITING) {
		kn->kn_status &= ~KN_WAITING;
		wakeup(kn);
	}
	splx(s);
	if ((kn->kn_fop->f_flags & FILTEROP_ISFD) && kn->kn_fp != NULL)
		FRELE(kn->kn_fp, p);
	pool_put(&knote_pool, kn);
}


void
knote_enqueue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;

	splassert(IPL_HIGH);
	KASSERT(kn->kn_filter != EVFILT_MARKER);
	KASSERT((kn->kn_status & KN_QUEUED) == 0);

	kqueue_check(kq);
	TAILQ_INSERT_TAIL(&kq->kq_head, kn, kn_tqe);
	kn->kn_status |= KN_QUEUED;
	kq->kq_count++;
	kqueue_check(kq);
	kqueue_wakeup(kq);
}

void
knote_dequeue(struct knote *kn)
{
	struct kqueue *kq = kn->kn_kq;

	splassert(IPL_HIGH);
	KASSERT(kn->kn_filter != EVFILT_MARKER);
	KASSERT(kn->kn_status & KN_QUEUED);

	kqueue_check(kq);
	TAILQ_REMOVE(&kq->kq_head, kn, kn_tqe);
	kn->kn_status &= ~KN_QUEUED;
	kq->kq_count--;
	kqueue_check(kq);
}

/*
 * Modify the knote's parameters.
 *
 * The knote's object lock must be held.
 */
void
knote_modify(const struct kevent *kev, struct knote *kn)
{
	kn->kn_sfflags = kev->fflags;
	kn->kn_sdata = kev->data;
	kn->kn_udata = kev->udata;
}

/*
 * Submit the knote's event for delivery.
 *
 * The knote's object lock must be held.
 */
void
knote_submit(struct knote *kn, struct kevent *kev)
{
	if (kev != NULL) {
		*kev = kn->kn_kevent;
		if (kn->kn_flags & EV_CLEAR) {
			kn->kn_fflags = 0;
			kn->kn_data = 0;
		}
	}
}

void
klist_init(struct klist *klist, const struct klistops *ops, void *arg)
{
	SLIST_INIT(&klist->kl_list);
	klist->kl_ops = ops;
	klist->kl_arg = arg;
}

void
klist_free(struct klist *klist)
{
	KASSERT(SLIST_EMPTY(&klist->kl_list));
}

void
klist_insert(struct klist *klist, struct knote *kn)
{
	int ls;

	ls = klist_lock(klist);
	SLIST_INSERT_HEAD(&klist->kl_list, kn, kn_selnext);
	klist_unlock(klist, ls);
}

void
klist_insert_locked(struct klist *klist, struct knote *kn)
{
	KLIST_ASSERT_LOCKED(klist);

	SLIST_INSERT_HEAD(&klist->kl_list, kn, kn_selnext);
}

void
klist_remove(struct klist *klist, struct knote *kn)
{
	int ls;

	ls = klist_lock(klist);
	SLIST_REMOVE(&klist->kl_list, kn, knote, kn_selnext);
	klist_unlock(klist, ls);
}

void
klist_remove_locked(struct klist *klist, struct knote *kn)
{
	KLIST_ASSERT_LOCKED(klist);

	SLIST_REMOVE(&klist->kl_list, kn, knote, kn_selnext);
}

int
klist_empty(struct klist *klist)
{
	return (SLIST_EMPTY(&klist->kl_list));
}

/*
 * Detach all knotes from klist. The knotes are rewired to indicate EOF.
 *
 * The caller of this function must not hold any locks that can block
 * filterops callbacks that run with KN_PROCESSING.
 * Otherwise this function might deadlock.
 */
void
klist_invalidate(struct klist *list)
{
	struct knote *kn;
	struct proc *p = curproc;
	int ls, s;

	NET_ASSERT_UNLOCKED();

	s = splhigh();
	ls = klist_lock(list);
	while ((kn = SLIST_FIRST(&list->kl_list)) != NULL) {
		if (!knote_acquire(kn, list, ls)) {
			/* knote_acquire() has unlocked list. */
			ls = klist_lock(list);
			continue;
		}
		klist_unlock(list, ls);
		splx(s);
		filter_detach(kn);
		if (kn->kn_fop->f_flags & FILTEROP_ISFD) {
			kn->kn_fop = &dead_filtops;
			filter_event(kn, 0);
			knote_activate(kn);
			s = splhigh();
			knote_release(kn);
		} else {
			knote_drop(kn, p);
			s = splhigh();
		}
		ls = klist_lock(list);
	}
	klist_unlock(list, ls);
	splx(s);
}

static int
klist_lock(struct klist *list)
{
	int ls = 0;

	if (list->kl_ops != NULL) {
		ls = list->kl_ops->klo_lock(list->kl_arg);
	} else {
		KERNEL_LOCK();
		ls = splhigh();
	}
	return ls;
}

static void
klist_unlock(struct klist *list, int ls)
{
	if (list->kl_ops != NULL) {
		list->kl_ops->klo_unlock(list->kl_arg, ls);
	} else {
		splx(ls);
		KERNEL_UNLOCK();
	}
}

static void
klist_mutex_assertlk(void *arg)
{
	struct mutex *mtx = arg;

	(void)mtx;

	MUTEX_ASSERT_LOCKED(mtx);
}

static int
klist_mutex_lock(void *arg)
{
	struct mutex *mtx = arg;

	mtx_enter(mtx);
	return 0;
}

static void
klist_mutex_unlock(void *arg, int s)
{
	struct mutex *mtx = arg;

	mtx_leave(mtx);
}

static const struct klistops mutex_klistops = {
	.klo_assertlk	= klist_mutex_assertlk,
	.klo_lock	= klist_mutex_lock,
	.klo_unlock	= klist_mutex_unlock,
};

void
klist_init_mutex(struct klist *klist, struct mutex *mtx)
{
	klist_init(klist, &mutex_klistops, mtx);
}

static void
klist_rwlock_assertlk(void *arg)
{
	struct rwlock *rwl = arg;

	(void)rwl;

	rw_assert_wrlock(rwl);
}

static int
klist_rwlock_lock(void *arg)
{
	struct rwlock *rwl = arg;

	rw_enter_write(rwl);
	return 0;
}

static void
klist_rwlock_unlock(void *arg, int s)
{
	struct rwlock *rwl = arg;

	rw_exit_write(rwl);
}

static const struct klistops rwlock_klistops = {
	.klo_assertlk	= klist_rwlock_assertlk,
	.klo_lock	= klist_rwlock_lock,
	.klo_unlock	= klist_rwlock_unlock,
};

void
klist_init_rwlock(struct klist *klist, struct rwlock *rwl)
{
	klist_init(klist, &rwlock_klistops, rwl);
}
