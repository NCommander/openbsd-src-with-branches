/*	$OpenBSD: rthread_sched.c,v 1.9 2011/12/28 04:59:31 guenther Exp $ */
/*
 * Copyright (c) 2004,2005 Ted Unangst <tedu@openbsd.org>
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * scheduling routines
 */

#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include <pthread.h>
#include <pthread_np.h>

#include "rthread.h"

int
pthread_getschedparam(pthread_t thread, int *policy,
    struct sched_param *param)
{
	*policy = thread->sched_policy;
	if (param)
		*param = thread->sched_param;

	return (0);
}

int
pthread_setschedparam(pthread_t thread, int policy,
    const struct sched_param *param)
{
	/* XXX return ENOTSUP for SCHED_{FIFO,RR}? */
	if (policy != SCHED_OTHER && policy != SCHED_FIFO &&
	    policy != SCHED_RR)
		return (EINVAL);
	thread->sched_policy = policy;
	if (param)
		thread->sched_param = *param;

	return (0);
}

int
pthread_attr_getschedparam(const pthread_attr_t *attrp, struct sched_param *param)
{
	*param = (*attrp)->sched_param;

	return (0);
}

int
pthread_attr_setschedparam(pthread_attr_t *attrp, const struct sched_param *param)
{
	(*attrp)->sched_param = *param;

	return (0);
}

int
pthread_attr_getschedpolicy(const pthread_attr_t *attrp, int *policy)
{
	*policy = (*attrp)->sched_policy;

	return (0);
}

int
pthread_attr_setschedpolicy(pthread_attr_t *attrp, int policy)
{
	/* XXX return ENOTSUP for SCHED_{FIFO,RR}? */
	if (policy != SCHED_OTHER && policy != SCHED_FIFO &&
	    policy != SCHED_RR)
		return (EINVAL);
	(*attrp)->sched_policy = policy;

	return (0);
}

int
pthread_attr_getinheritsched(const pthread_attr_t *attrp, int *inherit)
{
	*inherit = (*attrp)->sched_inherit;

	return (0);
}

int
pthread_attr_setinheritsched(pthread_attr_t *attrp, int inherit)
{
	if (inherit != PTHREAD_INHERIT_SCHED &&
	    inherit != PTHREAD_EXPLICIT_SCHED)
		return (EINVAL);
	(*attrp)->sched_inherit = inherit;

	return (0);
}

int
pthread_getprio(pthread_t thread)
{
	return (thread->sched_param.sched_priority);
}

int
pthread_setprio(pthread_t thread, int priority)
{
	thread->sched_param.sched_priority = priority;

	return (0);
}

void
pthread_yield(void)
{
	sched_yield();
}

int
pthread_suspend_np(pthread_t thread)
{
	int errn = 0;

	if (thread == pthread_self())
		return (EDEADLK);
	/*
	 * XXX Avoid a bug in current signal handling by refusing to
	 * suspend the main thread.
	 */
	if (thread != &_initial_thread)
		if (kill(thread->tid, SIGSTOP) == -1)
			errn = errno;
	return (errn);
}

void
pthread_suspend_all_np(void)
{
	pthread_t t;
	pthread_t self = pthread_self();

	_spinlock(&_thread_lock);
	LIST_FOREACH(t, &_thread_list, threads)
		if (t != self)
			pthread_suspend_np(t);
	_spinunlock(&_thread_lock);
}

int
pthread_resume_np(pthread_t thread)
{
	int errn = 0;

	/* XXX check if really suspended? */
	if (kill(thread->tid, SIGCONT) == -1)
		errn = errno;
	return (errn);
}

void
pthread_resume_all_np(void)
{
	pthread_t t;
	pthread_t self = pthread_self();

	_spinlock(&_thread_lock);
	LIST_FOREACH(t, &_thread_list, threads)
		if (t != self)
			pthread_resume_np(t);
	_spinunlock(&_thread_lock);
}

