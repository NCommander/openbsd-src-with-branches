/*	$OpenBSD: uthread_join.c,v 1.6 1999/11/25 07:01:37 d Exp $	*/
/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 * $FreeBSD: uthread_join.c,v 1.9 1999/08/28 00:03:37 peter Exp $
 */
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
pthread_join(pthread_t pthread, void **thread_return)
{
	struct pthread	*curthread = _get_curthread();
	int ret = 0;

	/* This is a cancellation point: */
	_thread_enter_cancellation_point();

	/* Check if the caller has specified an invalid thread: */
	if (pthread == NULL || pthread->magic != PTHREAD_MAGIC)
		/* Invalid thread: */
		ret = EINVAL;

	/* Check if the caller has specified itself: */
	else if (pthread == curthread)
		/* Avoid a deadlock condition: */
		ret = EDEADLK;

	/*
	 * Find the thread in the list of active threads or in the
	 * list of dead threads:
	 */
	else if (_find_thread(pthread) != 0 &&
	    _find_dead_thread(pthread) != 0)
		/* Return an error: */
		ret = ESRCH;

	/* Check if this thread has been detached: */
	else if ((pthread->attr.flags & PTHREAD_DETACHED) != 0)
		/* Return an error: */
		ret = ESRCH;

	/* Check if the thread is not dead: */
	else if (pthread->state != PS_DEAD) {
		/* Add the running thread to the join queue: */
		TAILQ_INSERT_TAIL(&(pthread->join_queue), curthread, qe);

		/* Schedule the next thread: */
		_thread_kern_sched_state(PS_JOIN, __FILE__, __LINE__);

		/* Check if the thread is not detached: */
		if ((pthread->attr.flags & PTHREAD_DETACHED) == 0) {
			/* Check if the return value is required: */
			if (thread_return)
				/* Return the thread's return value: */
				*thread_return = pthread->ret;
		}
		else
			/* Return an error: */
			ret = ESRCH;

	/* Check if the return value is required: */
	} else if (thread_return != NULL)
		/* Return the thread's return value: */
		*thread_return = pthread->ret;

	/* No longer in a cancellation point: */
	_thread_leave_cancellation_point();

	/* Return the completion status: */
	return (ret);
}
#endif
