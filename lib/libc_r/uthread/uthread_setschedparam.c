/*	$OpenBSD$	*/
/*
 * Copyright (c) 1998 Daniel Eischen <eischen@vigrid.com>.
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
 *	This product includes software developed by Daniel Eischen.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS'' AND
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
 */
#include <errno.h>
#include <sys/param.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
pthread_setschedparam(pthread_t pthread, int policy, const struct sched_param *param)
{
	int old_prio, in_readyq = 0, ret = 0;

	if ((param == NULL) || (param->sched_priority < PTHREAD_MIN_PRIORITY) ||
	    (param->sched_priority > PTHREAD_MAX_PRIORITY) ||
	    (policy < SCHED_FIFO) || (policy > SCHED_RR))
		/* Return an invalid argument error: */
		ret = EINVAL;

	/* Find the thread in the list of active threads: */
	else if ((ret = _find_thread(pthread)) == 0) {
		/*
		 * Guard against being preempted by a scheduling
		 * signal:
		 */
		_thread_kern_sched_defer();

		if (param->sched_priority != pthread->base_priority) {
			/*
			 * Remove the thread from its current priority
			 * queue before any adjustments are made to its
			 * active priority:
			 */
			if ((pthread != _thread_run) &&
			   (pthread->state == PS_RUNNING)) {
				in_readyq = 1;
				old_prio = pthread->active_priority;
				PTHREAD_PRIOQ_REMOVE(pthread);
			}

			/* Set the thread base priority: */
			pthread->base_priority = param->sched_priority;

			/* Recalculate the active priority: */
			pthread->active_priority = MAX(pthread->base_priority,
			    pthread->inherited_priority);

			if (in_readyq) {
				if ((pthread->priority_mutex_count > 0) &&
				    (old_prio > pthread->active_priority)) {
					/*
					 * POSIX states that if the priority is
					 * being lowered, the thread must be
					 * inserted at the head of the queue for
					 * its priority if it owns any priority
					 * protection or inheritence mutexes.
					 */
					PTHREAD_PRIOQ_INSERT_HEAD(pthread);
				}
				else
					PTHREAD_PRIOQ_INSERT_TAIL(pthread);
			}

			/*
			 * Check for any mutex priority adjustments.  This
			 * includes checking for a priority mutex on which
			 * this thread is waiting.
			 */
			_mutex_notify_priochange(pthread);
		}

		/* Set the scheduling policy: */
		pthread->attr.sched_policy = policy;

		/*
		 * Renable preemption and yield if a scheduling signal
		 * arrived while in the critical region:
		 */
		_thread_kern_sched_undefer();
	}
	return(ret);
}
#endif
