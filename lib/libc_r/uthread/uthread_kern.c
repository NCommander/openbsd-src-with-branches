/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: uthread_kern.c,v 1.15 1998/11/15 09:58:26 jb Exp $
 * $OpenBSD: uthread_kern.c,v 1.6 1999/01/17 23:49:49 d Exp $
 *
 */
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static function prototype definitions: */
static void 
_thread_kern_select(int wait_reqd);

void
_thread_kern_sched(struct sigcontext * scp)
{
	int             prio = -1;
	pthread_t       pthread;
	pthread_t       pthread_h = NULL;
	pthread_t       pthread_s = NULL;
	struct itimerval itimer;
	struct timespec ts;
	struct timespec ts1;
	struct timeval  tv;
	struct timeval  tv1;
#ifdef _THREAD_RUSAGE
	struct rusage	ru;
	static struct rusage ru_prev;
#endif

	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid a scheduler signal from interrupting this
	 * execution and calling the scheduler again.
	 */
	_thread_kern_in_sched = 1;

	/* Check if this function was called from the signal handler: */
	if (scp != NULL) {
		/*
		 * Copy the signal context to the current thread's jump
		 * buffer: 
		 */
		memcpy(&_thread_run->saved_sigcontext, scp, sizeof(_thread_run->saved_sigcontext));

		/* Save the floating point data: */
		_thread_machdep_save_float_state(_thread_run);

		/* Flag the signal context as the last state saved: */
		_thread_run->sig_saved = 1;
	}
	/* Save the state of the current thread: */
	else if (_thread_machdep_setjmp(_thread_run->saved_jmp_buf) != 0) {
		/*
		 * This point is reached when a longjmp() is called to
		 * restore the state of a thread. 
		 *
		 * This is the normal way out of the scheduler.
		 */
		_thread_kern_in_sched = 0;

		if (!(_thread_run->flags & PTHREAD_AT_CANCEL_POINT) &&
		    (_thread_run->canceltype == PTHREAD_CANCEL_ASYNCHRONOUS)) {
			/* 
			 * Cancelations override signals.
			 *
			 * Stick a cancellation point at the start of
			 * each async-cancellable thread's resumption.
			 *
			 * We allow threads woken at cancel points to do their
			 * own checks.
			 */
			_thread_cancellation_point();
		}

		/*
		 * There might be pending signals for this thread, so
		 * dispatch any that aren't blocked:
		 */
		_dispatch_signals();
		return;
	} else
		/* Flag the jump buffer was the last state saved: */
		_thread_run->sig_saved = 0;

	/* Save errno. */
	_thread_run->error = errno;

#ifdef _THREAD_RUSAGE
	/* Accumulate time spent */
	if (getrusage(RUSAGE_SELF, &ru))
		PANIC("Cannot get resource usage");
	timersub(&ru.ru_utime, &ru_prev.ru_utime, &tv);
	timeradd(&tv, &_thread_run->ru_utime, &_thread_run->ru_utime);
	timersub(&ru.ru_stime, &ru_prev.ru_stime, &tv);
	timeradd(&tv, &_thread_run->ru_stime, &_thread_run->ru_stime);
	memcpy(&ru_prev.ru_utime, &ru.ru_utime, sizeof ru_prev.ru_utime);
	memcpy(&ru_prev.ru_stime, &ru.ru_stime, sizeof ru_prev.ru_stime);
#endif /* _THREAD_RUSAGE */

	/*
	 * Enter a the scheduling loop that finds the next thread that is
	 * ready to run. This loop completes when there are no more threads
	 * in the global list or when a thread has its state restored by
	 * either a sigreturn (if the state was saved as a sigcontext) or a
	 * longjmp (if the state was saved by a setjmp). 
	 */
	while (_thread_link_list != NULL) {
		/* Get the current time of day: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &ts);

		/*
		 * Poll file descriptors to update the state of threads
		 * waiting on file I/O where data may be available: 
		 */
		_thread_kern_select(0);

		/*
		 * Enter a loop to look for sleeping threads that are ready:
		 */
		for (pthread = _thread_link_list; pthread != NULL;
		    pthread = pthread->nxt) {
			/* Check if this thread is to timeout: */
			if (pthread->state == PS_COND_WAIT ||
			    pthread->state == PS_SLEEP_WAIT ||
			    pthread->state == PS_FDR_WAIT ||
			    pthread->state == PS_FDW_WAIT ||
			    pthread->state == PS_SELECT_WAIT) {
				/* Check if this thread is to wait forever: */
				if (pthread->wakeup_time.tv_sec == -1) {
				}
				/*
				 * Check if this thread is to wakeup
				 * immediately or if it is past its wakeup
				 * time: 
				 */
				else if ((pthread->wakeup_time.tv_sec == 0 &&
					pthread->wakeup_time.tv_nsec == 0) ||
					 (ts.tv_sec > pthread->wakeup_time.tv_sec) ||
					 ((ts.tv_sec == pthread->wakeup_time.tv_sec) &&
					  (ts.tv_nsec >= pthread->wakeup_time.tv_nsec))) {
					/*
					 * Check if this thread is waiting on
					 * select: 
					 */
					if (pthread->state == PS_SELECT_WAIT) {
						/*
						 * The select has timed out,
						 * so zero the file
						 * descriptor sets: 
						 */
						FD_ZERO(&pthread->data.select_data->readfds);
						FD_ZERO(&pthread->data.select_data->writefds);
						FD_ZERO(&pthread->data.select_data->exceptfds);
						pthread->data.select_data->nfds = 0;
					}
					/*
					 * Return an error as an interrupted
					 * wait: 
					 */
					_thread_seterrno(pthread, EINTR);

					/*
					 * Flag the timeout in the thread
					 * structure: 
					 */
					pthread->timeout = 1;

					/*
					 * Change the threads state to allow
					 * it to be restarted: 
					 */
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				}
			}
		}

		/* Check if there is a current thread: */
		if (_thread_run != _thread_kern_threadp) {
			/*
			 * Save the current time as the time that the thread
			 * became inactive: 
			 */
			_thread_run->last_inactive.tv_sec = tv.tv_sec;
			_thread_run->last_inactive.tv_usec = tv.tv_usec;

			/*
			 * Accumulate the number of microseconds that this
			 * thread has run for: 
			 */
			if (_thread_run->slice_usec != -1) {
				if (timerisset(&_thread_run->last_active)) {
					struct timeval s;

					timersub(&_thread_run->last_inactive,
					    &_thread_run->last_active,
					    &s);
					_thread_run->slice_usec = 
					    s.tv_usec + 1000000 * s.tv_sec;
					if (_thread_run->slice_usec < 0)
						PANIC("slice_usec");
				} else
					_thread_run->slice_usec = -1;
                        }

			/*
			 * Check if this thread has reached its allocated
			 * time slice period: 
			 */
			if (_thread_run->slice_usec > TIMESLICE_USEC) {
				/*
				 * Flag the allocated time slice period as
				 * up: 
				 */
				_thread_run->slice_usec = -1;
			}
		}
		/* Check if an incremental priority update is required: */
		if (((tv.tv_sec - kern_inc_prio_time.tv_sec) * 1000000 +
		 tv.tv_usec - kern_inc_prio_time.tv_usec) > INC_PRIO_USEC) {
			/*
			 * Enter a loop to look for run-enabled threads that
			 * have not run since the last time that an
			 * incremental priority update was performed: 
			 */
			for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
				/* Check if this thread is unable to run: */
				if (pthread->state != PS_RUNNING) {
				}
				/*
				 * Check if the last time that this thread
				 * was run (as indicated by the last time it
				 * became inactive) is before the time that
				 * the last incremental priority check was
				 * made: 
				 */
				else if (timercmp(&pthread->last_inactive, &kern_inc_prio_time, <)) {
					/*
					 * Increment the incremental priority
					 * for this thread in the hope that
					 * it will eventually get a chance to
					 * run: 
					 */
					(pthread->inc_prio)++;
				}
			}

			/* Save the new incremental priority update time: */
			kern_inc_prio_time.tv_sec = tv.tv_sec;
			kern_inc_prio_time.tv_usec = tv.tv_usec;
		}
		/*
		 * Enter a loop to look for the first thread of the highest
		 * priority that is ready to run: 
		 */
		for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
			/* Check if the current thread is unable to run: */
			if (pthread->state != PS_RUNNING) {
			}
			/*
			 * Check if no run-enabled thread has been seen or if
			 * the current thread has a priority higher than the
			 * highest seen so far: 
			 */
			else if (pthread_h == NULL || (pthread->pthread_priority + pthread->inc_prio) > prio) {
				/*
				 * Save this thread as the highest priority
				 * thread seen so far: 
				 */
				pthread_h = pthread;
				prio = pthread->pthread_priority + pthread->inc_prio;
			}
		}

		/*
		 * Enter a loop to look for a thread that: 1. Is run-enabled.
		 * 2. Has the required agregate priority. 3. Has not been
		 * allocated its allocated time slice. 4. Became inactive
		 * least recently. 
		 */
		for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
			/* Check if the current thread is unable to run: */
			if (pthread->state != PS_RUNNING) {
				/* Ignore threads that are not ready to run. */
			}

			/*
			 * Check if the current thread as an agregate
			 * priority not equal to the highest priority found
			 * above: 
			 */
			else if ((pthread->pthread_priority + pthread->inc_prio) != prio) {
				/*
				 * Ignore threads which have lower agregate
				 * priority. 
				 */
			}

			/*
			 * Check if the current thread reached its time slice
			 * allocation last time it ran (or if it has not run
			 * yet): 
			 */
			else if (pthread->slice_usec == -1) {
			}

			/*
			 * Check if an eligible thread has not been found
			 * yet, or if the current thread has an inactive time
			 * earlier than the last one seen: 
			 */
			else if (pthread_s == NULL || timercmp(&pthread->last_inactive, &tv1, <)) {
				/*
				 * Save the pointer to the current thread as
				 * the most eligible thread seen so far: 
				 */
				pthread_s = pthread;

				/*
				 * Save the time that the selected thread
				 * became inactive: 
				 */
				tv1.tv_sec = pthread->last_inactive.tv_sec;
				tv1.tv_usec = pthread->last_inactive.tv_usec;
			}
		}

		/*
		 * Check if no thread was selected according to incomplete
		 * time slice allocation: 
		 */
		if (pthread_s == NULL) {
			/*
			 * Enter a loop to look for any other thread that: 1.
			 * Is run-enabled. 2. Has the required agregate
			 * priority. 3. Became inactive least recently. 
			 */
			for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
				/*
				 * Check if the current thread is unable to
				 * run: 
				 */
				if (pthread->state != PS_RUNNING) {
					/*
					 * Ignore threads that are not ready
					 * to run. 
					 */
				}
				/*
				 * Check if the current thread as an agregate
				 * priority not equal to the highest priority
				 * found above: 
				 */
				else if ((pthread->pthread_priority + pthread->inc_prio) != prio) {
					/*
					 * Ignore threads which have lower
					 * agregate priority.   
					 */
				}
				/*
				 * Check if an eligible thread has not been
				 * found yet, or if the current thread has an
				 * inactive time earlier than the last one
				 * seen: 
				 */
				else if (pthread_s == NULL || timercmp(&pthread->last_inactive, &tv1, <)) {
					/*
					 * Save the pointer to the current
					 * thread as the most eligible thread
					 * seen so far: 
					 */
					pthread_s = pthread;

					/*
					 * Save the time that the selected
					 * thread became inactive: 
					 */
					tv1.tv_sec = pthread->last_inactive.tv_sec;
					tv1.tv_usec = pthread->last_inactive.tv_usec;
				}
			}
		}
		/* Check if there are no threads ready to run: */
		if (pthread_s == NULL) {
			/*
			 * Lock the pthread kernel by changing the pointer to
			 * the running thread to point to the global kernel
			 * thread structure: 
			 */
			_thread_run = _thread_kern_threadp;

			/*
			 * There are no threads ready to run, so wait until
			 * something happens that changes this condition: 
			 */
			_thread_kern_select(1);
		} else {
			/* Make the selected thread the current thread: */
			_thread_run = pthread_s;

			/*
			 * Save the current time as the time that the thread
			 * became active: 
			 */
			_thread_run->last_active.tv_sec = tv.tv_sec;
			_thread_run->last_active.tv_usec = tv.tv_usec;

			/*
			 * Check if this thread is running for the first time
			 * or running again after using its full time slice
			 * allocation: 
			 */
			if (_thread_run->slice_usec == -1) {
				/* Reset the accumulated time slice period: */
				_thread_run->slice_usec = 0;
			}
			/*
			 * Reset the incremental priority now that this
			 * thread has been given the chance to run: 
			 */
			_thread_run->inc_prio = 0;

			/* Check if there is more than one thread: */
			if (_thread_run != _thread_link_list || _thread_run->nxt != NULL) {
				/*
				 * Define the maximum time before a SIGVTALRM
				 * is required: 
				 */
				itimer.it_value.tv_sec = 0;
				itimer.it_value.tv_usec = TIMESLICE_USEC;

				/*
				 * The interval timer is not reloaded when it
				 * times out. The interval time needs to be
				 * calculated every time. 
				 */
				timerclear(&itimer.it_interval);

				/*
				 * Enter a loop to look for threads waiting
				 * for a time: 
				 */
				for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
					/*
					 * Check if this thread is to
					 * timeout: 
					 */
					if (pthread->state == PS_COND_WAIT ||
					  pthread->state == PS_SLEEP_WAIT ||
					    pthread->state == PS_FDR_WAIT ||
					    pthread->state == PS_FDW_WAIT ||
					 pthread->state == PS_SELECT_WAIT) {
						/*
						 * Check if this thread is to
						 * wait forever: 
						 */
						if (pthread->wakeup_time.tv_sec == -1) {
						}
						/*
						 * Check if this thread is to
						 * wakeup immediately: 
						 */
						else if (pthread->wakeup_time.tv_sec == 0 &&
							 pthread->wakeup_time.tv_nsec == 0) {
						}
						/*
						 * Check if the current time
						 * is after the wakeup time: 
						 */
						else if (timespeccmp(&ts,
						    &pthread->wakeup_time, > )){
						} else {
							/*
							 * Calculate the time
							 * until this thread
							 * is ready, allowing
							 * for the clock
							 * resolution: 
							 */
							struct timespec
							 clock_res
							  = {0,CLOCK_RES_NSEC};
							timespecsub(
							  &pthread->wakeup_time,
							  &ts, &ts1);
							timespecadd(
							  &ts1, &clock_res,
							  &ts1);
							/*
							 * Convert the
							 * timespec structure
							 * to a timeval
							 * structure: 
							 */
							TIMESPEC_TO_TIMEVAL(&tv, &ts1);

							/*
							 * Check if the
							 * thread will be
							 * ready sooner than
							 * the earliest one
							 * found so far: 
							 */
							if (timercmp(&tv, &itimer.it_value, <)) {
								/*
								 * Update the
								 * time
								 * value: 
								 */
								itimer.it_value.tv_sec = tv.tv_sec;
								itimer.it_value.tv_usec = tv.tv_usec;
							}
						}
					}
				}

				/*
				 * Start the interval timer for the
				 * calculated time interval: 
				 */
				if (setitimer(ITIMER_VIRTUAL, &itimer, NULL) != 0) {
					/*
					 * Cannot initialise the timer, so
					 * abort this process: 
					 */
					PANIC("Cannot set virtual timer");
				}
			}

			/* Restore errno. */
			errno = _thread_run->error;

			/* Check if a signal context was saved: */
			if (_thread_run->sig_saved == 1) {

				/* Restore the floating point state: */
				_thread_machdep_restore_float_state(_thread_run);

				/*
				 * Do a sigreturn to restart the thread that
				 * was interrupted by a signal: 
				 */
		                _thread_kern_in_sched = 0;
				_thread_sys_sigreturn(&_thread_run->saved_sigcontext);
			} else
				/*
				 * Do a longjmp to restart the thread that
				 * was context switched out (by a longjmp to
				 * a different thread): 
				 */
				_thread_machdep_longjmp(_thread_run->saved_jmp_buf, 1);

			/* This point should not be reached. */
			PANIC("Thread has returned from sigreturn or longjmp");
		}
	}

	/* There are no more threads, so exit this process: */
	exit(0);
}

void
_thread_kern_sched_state(enum pthread_state state, const char *fname, int lineno)
{
	/* Change the state of the current thread: */
	_thread_run->state = state;
	_thread_run->fname = fname;
	_thread_run->lineno = lineno;

	/* Schedule the next thread that is ready: */
	_thread_kern_sched(NULL);
	return;
}

void
_thread_kern_sched_state_unlock(enum pthread_state state,
    spinlock_t *lock, char *fname, int lineno)
{
	/* Change the state of the current thread: */
	_thread_run->state = state;
	_thread_run->fname = fname;
	_thread_run->lineno = lineno;

	_SPINUNLOCK(lock);

	/* Schedule the next thread that is ready: */
	_thread_kern_sched(NULL);
	return;
}

static void
_thread_kern_select(int wait_reqd)
{
	char            bufr[128];
	fd_set          fd_set_except;
	fd_set          fd_set_read;
	fd_set          fd_set_write;
	int             count = 0;
	int             count_dec;
	int             found_one;
	int             i;
	int             nfds = -1;
	int             settimeout;
	pthread_t       pthread;
	ssize_t         num;
	struct timespec ts;
	struct timespec ts1;
	struct timeval *p_tv;
	struct timeval  tv;
	struct timeval  tv1;

	/* Zero the file descriptor sets: */
	FD_ZERO(&fd_set_read);
	FD_ZERO(&fd_set_write);
	FD_ZERO(&fd_set_except);

	/* Check if the caller wants to wait: */
	if (wait_reqd) {
		/*
		 * Add the pthread kernel pipe file descriptor to the read
		 * set: 
		 */
		FD_SET(_thread_kern_pipe[0], &fd_set_read);
		nfds = _thread_kern_pipe[0];

		/* Get the current time of day: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &ts);
	}
	/* Initialise the time value structure: */
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	/*
	 * Enter a loop to process threads waiting on either file descriptors
	 * or times: 
	 */
	for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
		/* Assume that this state does not time out: */
		settimeout = 0;

		/* Process according to thread state: */
		switch (pthread->state) {
		/*
		 * States which do not depend on file descriptor I/O
		 * operations or timeouts: 
		 */
		case PS_DEAD:
		case PS_FDLR_WAIT:
		case PS_FDLW_WAIT:
		case PS_FILE_WAIT:
		case PS_JOIN:
		case PS_MUTEX_WAIT:
		case PS_RUNNING:
		case PS_SIGTHREAD:
		case PS_SIGWAIT:
		case PS_STATE_MAX:
		case PS_WAIT_WAIT:
		case PS_SUSPENDED:
		case PS_SIGSUSPEND:
			/* Nothing to do here. */
			break;

		/* File descriptor read wait: */
		case PS_FDR_WAIT:
			/* Add the file descriptor to the read set: */
			FD_SET(pthread->data.fd.fd, &fd_set_read);

			/*
			 * Check if this file descriptor is greater than any
			 * of those seen so far: 
			 */
			if (pthread->data.fd.fd > nfds) {
				/* Remember this file descriptor: */
				nfds = pthread->data.fd.fd;
			}
			/* Increment the file descriptor count: */
			count++;

			/* This state can time out: */
			settimeout = 1;
			break;

		/* File descriptor write wait: */
		case PS_FDW_WAIT:
			/* Add the file descriptor to the write set: */
			FD_SET(pthread->data.fd.fd, &fd_set_write);

			/*
			 * Check if this file descriptor is greater than any
			 * of those seen so far: 
			 */
			if (pthread->data.fd.fd > nfds) {
				/* Remember this file descriptor: */
				nfds = pthread->data.fd.fd;
			}
			/* Increment the file descriptor count: */
			count++;

			/* This state can time out: */
			settimeout = 1;
			break;

		/* States that time out: */
		case PS_SLEEP_WAIT:
		case PS_COND_WAIT:
			/* Flag a timeout as required: */
			settimeout = 1;
			break;

		/* Select wait: */
		case PS_SELECT_WAIT:
			/*
			 * Enter a loop to process each file descriptor in
			 * the thread-specific file descriptor sets: 
			 */
			for (i = 0; i < pthread->data.select_data->nfds; i++) {
				/*
				 * Check if this file descriptor is set for
				 * exceptions: 
				 */
				if (FD_ISSET(i, &pthread->data.select_data->exceptfds)) {
					/*
					 * Add the file descriptor to the
					 * exception set: 
					 */
					FD_SET(i, &fd_set_except);

					/*
					 * Increment the file descriptor
					 * count: 
					 */
					count++;

					/*
					 * Check if this file descriptor is
					 * greater than any of those seen so
					 * far: 
					 */
					if (i > nfds) {
						/*
						 * Remember this file
						 * descriptor: 
						 */
						nfds = i;
					}
				}
				/*
				 * Check if this file descriptor is set for
				 * write: 
				 */
				if (FD_ISSET(i, &pthread->data.select_data->writefds)) {
					/*
					 * Add the file descriptor to the
					 * write set: 
					 */
					FD_SET(i, &fd_set_write);

					/*
					 * Increment the file descriptor
					 * count: 
					 */
					count++;

					/*
					 * Check if this file descriptor is
					 * greater than any of those seen so
					 * far: 
					 */
					if (i > nfds) {
						/*
						 * Remember this file
						 * descriptor: 
						 */
						nfds = i;
					}
				}
				/*
				 * Check if this file descriptor is set for
				 * read: 
				 */
				if (FD_ISSET(i, &pthread->data.select_data->readfds)) {
					/*
					 * Add the file descriptor to the
					 * read set: 
					 */
					FD_SET(i, &fd_set_read);

					/*
					 * Increment the file descriptor
					 * count: 
					 */
					count++;

					/*
					 * Check if this file descriptor is
					 * greater than any of those seen so
					 * far: 
					 */
					if (i > nfds) {
						/*
						 * Remember this file
						 * descriptor: 
						 */
						nfds = i;
					}
				}
			}

			/* This state can time out: */
			settimeout = 1;
			break;
		}

		/*
		 * Check if the caller wants to wait and if the thread state
		 * is one that times out: 
		 */
		if (wait_reqd && settimeout) {
			/* Check if this thread wants to wait forever: */
			if (pthread->wakeup_time.tv_sec == -1) {
			}
			/* Check if this thread doesn't want to wait at all: */
			else if (pthread->wakeup_time.tv_sec == 0 &&
				 pthread->wakeup_time.tv_nsec == 0) {
				/* Override the caller's request to wait: */
				wait_reqd = 0;
			} else {
				/*
				 * Calculate the time until this thread is
				 * ready, allowing for the clock resolution: 
				 */
				ts1.tv_sec = pthread->wakeup_time.tv_sec - ts.tv_sec;
				ts1.tv_nsec = pthread->wakeup_time.tv_nsec - ts.tv_nsec +
					CLOCK_RES_NSEC;

				/*
				 * Check for underflow of the nanosecond
				 * field: 
				 */
				if (ts1.tv_nsec < 0) {
					/*
					 * Allow for the underflow of the
					 * nanosecond field: 
					 */
					ts1.tv_sec--;
					ts1.tv_nsec += 1000000000;
				}
				/*
				 * Check for overflow of the nanosecond
				 * field: 
				 */
				if (ts1.tv_nsec >= 1000000000) {
					/*
					 * Allow for the overflow of the
					 * nanosecond field: 
					 */
					ts1.tv_sec++;
					ts1.tv_nsec -= 1000000000;
				}
				/*
				 * Convert the timespec structure to a
				 * timeval structure: 
				 */
				TIMESPEC_TO_TIMEVAL(&tv1, &ts1);

				/*
				 * Check if no time value has been found yet,
				 * or if the thread will be ready sooner that
				 * the earliest one found so far: 
				 */
				if ((tv.tv_sec == 0 && tv.tv_usec == 0) || timercmp(&tv1, &tv, <)) {
					/* Update the time value: */
					tv.tv_sec = tv1.tv_sec;
					tv.tv_usec = tv1.tv_usec;
				}
			}
		}
	}

	/* Check if the caller wants to wait: */
	if (wait_reqd) {
		/* Check if no threads were found with timeouts: */
		if (tv.tv_sec == 0 && tv.tv_usec == 0) {
			/* Wait forever: */
			p_tv = NULL;
		} else {
			/*
			 * Point to the time value structure which contains
			 * the earliest time that a thread will be ready: 
			 */
			p_tv = &tv;
		}

		/*
		 * Flag the pthread kernel as in a select. This is to avoid
		 * the window between the next statement that unblocks
		 * signals and the select statement which follows. 
		 */
		_thread_kern_in_select = 1;

		/*
		 * Wait for a file descriptor to be ready for read, write, or
		 * an exception, or a timeout to occur: 
		 */
		count = _thread_sys_select(nfds + 1, &fd_set_read, &fd_set_write, &fd_set_except, p_tv);

		/* Reset the kernel in select flag: */
		_thread_kern_in_select = 0;

		/*
		 * Check if it is possible that there are bytes in the kernel
		 * read pipe waiting to be read: 
		 */
		if (count < 0 || FD_ISSET(_thread_kern_pipe[0], &fd_set_read)) {
			/*
			 * Check if the kernel read pipe was included in the
			 * count: 
			 */
			if (count > 0) {
				/*
				 * Remove the kernel read pipe from the
				 * count: 
				 */
				FD_CLR(_thread_kern_pipe[0], &fd_set_read);

				/* Decrement the count of file descriptors: */
				count--;
			}
			/*
			 * Enter a loop to read (and trash) bytes from the
			 * pthread kernel pipe: 
			 */
			while ((num = _thread_sys_read(_thread_kern_pipe[0], bufr, sizeof(bufr))) > 0) {
				/*
				 * The buffer read contains one byte per
				 * signal and each byte is the signal number.
				 * This data is not used, but the fact that
				 * the signal handler wrote to the pipe *is*
				 * used to cause the _select call
				 * to complete if the signal occurred between
				 * the time when signals were unblocked and
				 * the _select select call being
				 * made. 
				 */
			}
		}
	}
	/* Check if there are file descriptors to poll: */
	else if (count > 0) {
		/*
		 * Point to the time value structure which has been zeroed so
		 * that the call to _select will not wait: 
		 */
		p_tv = &tv;

		/* Poll file descrptors without wait: */
		count = _thread_sys_select(nfds + 1, &fd_set_read, &fd_set_write, &fd_set_except, p_tv);
	}

	/*
	 * Check if any file descriptors are ready:
	 */
	if (count > 0) {
		/*
		 * Enter a loop to look for threads waiting on file
		 * descriptors that are flagged as available by the
		 * _select syscall: 
		 */
		for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
			/* Process according to thread state: */
			switch (pthread->state) {
			/*
			 * States which do not depend on file
			 * descriptor I/O operations: 
			 */
			case PS_RUNNING:
			case PS_COND_WAIT:
			case PS_DEAD:
			case PS_FDLR_WAIT:
			case PS_FDLW_WAIT:
			case PS_FILE_WAIT:
			case PS_JOIN:
			case PS_MUTEX_WAIT:
			case PS_SIGWAIT:
			case PS_SLEEP_WAIT:
			case PS_WAIT_WAIT:
			case PS_SIGTHREAD:
			case PS_STATE_MAX:
			case PS_SUSPENDED:
			case PS_SIGSUSPEND:
				/* Nothing to do here. */
				break;

			/* File descriptor read wait: */
			case PS_FDR_WAIT:
				/*
				 * Check if the file descriptor is available
				 * for read: 
				 */
				if (FD_ISSET(pthread->data.fd.fd, &fd_set_read)) {
					/*
					 * Change the thread state to allow
					 * it to read from the file when it
					 * is scheduled next: 
					 */
					pthread->state = PS_RUNNING;
				}
				break;

			/* File descriptor write wait: */
			case PS_FDW_WAIT:
				/*
				 * Check if the file descriptor is available
				 * for write: 
				 */
				if (FD_ISSET(pthread->data.fd.fd, &fd_set_write)) {
					/*
					 * Change the thread state to allow
					 * it to write to the file when it is
					 * scheduled next: 
					 */
					pthread->state = PS_RUNNING;
				}
				break;

			/* Select wait: */
			case PS_SELECT_WAIT:
				/*
				 * Reset the flag that indicates if a file
				 * descriptor is ready for some type of
				 * operation: 
				 */
				count_dec = 0;

				/*
				 * Enter a loop to search though the
				 * thread-specific select file descriptors
				 * for the first descriptor that is ready: 
				 */
				for (i = 0; i < pthread->data.select_data->nfds && count_dec == 0; i++) {
					/*
					 * Check if this file descriptor does
					 * not have an exception: 
					 */
					if (FD_ISSET(i, &pthread->data.select_data->exceptfds) && FD_ISSET(i, &fd_set_except)) {
						/*
						 * Flag this file descriptor
						 * as ready: 
						 */
						count_dec = 1;
					}
					/*
					 * Check if this file descriptor is
					 * not ready for write: 
					 */
					if (FD_ISSET(i, &pthread->data.select_data->writefds) && FD_ISSET(i, &fd_set_write)) {
						/*
						 * Flag this file descriptor
						 * as ready: 
						 */
						count_dec = 1;
					}
					/*
					 * Check if this file descriptor is
					 * not ready for read: 
					 */
					if (FD_ISSET(i, &pthread->data.select_data->readfds) && FD_ISSET(i, &fd_set_read)) {
						/*
						 * Flag this file descriptor
						 * as ready: 
						 */
						count_dec = 1;
					}
				}

				/*
				 * Check if any file descriptors are ready
				 * for the current thread: 
				 */
				if (count_dec) {
					/*
					 * Reset the count of file
					 * descriptors that are ready for
					 * this thread: 
					 */
					found_one = 0;

					/*
					 * Enter a loop to search though the
					 * thread-specific select file
					 * descriptors: 
					 */
					for (i = 0; i < pthread->data.select_data->nfds; i++) {
						/*
						 * Reset the count of
						 * operations for which the
						 * current file descriptor is
						 * ready: 
						 */
						count_dec = 0;

						/*
						 * Check if this file
						 * descriptor is selected for
						 * exceptions: 
						 */
						if (FD_ISSET(i, &pthread->data.select_data->exceptfds)) {
							/*
							 * Check if this file
							 * descriptor has an
							 * exception: 
							 */
							if (FD_ISSET(i, &fd_set_except)) {
								/*
								 * Increment
								 * the count
								 * for this
								 * file: 
								 */
								count_dec++;
							} else {
								/*
								 * Clear the
								 * file
								 * descriptor
								 * in the
								 * thread-spec
								 * ific file
								 * descriptor
								 * set: 
								 */
								FD_CLR(i, &pthread->data.select_data->exceptfds);
							}
						}
						/*
						 * Check if this file
						 * descriptor is selected for
						 * write: 
						 */
						if (FD_ISSET(i, &pthread->data.select_data->writefds)) {
							/*
							 * Check if this file
							 * descriptor is
							 * ready for write: 
							 */
							if (FD_ISSET(i, &fd_set_write)) {
								/*
								 * Increment
								 * the count
								 * for this
								 * file: 
								 */
								count_dec++;
							} else {
								/*
								 * Clear the
								 * file
								 * descriptor
								 * in the
								 * thread-spec
								 * ific file
								 * descriptor
								 * set: 
								 */
								FD_CLR(i, &pthread->data.select_data->writefds);
							}
						}
						/*
						 * Check if this file
						 * descriptor is selected for
						 * read: 
						 */
						if (FD_ISSET(i, &pthread->data.select_data->readfds)) {
							/*
							 * Check if this file
							 * descriptor is
							 * ready for read: 
							 */
							if (FD_ISSET(i, &fd_set_read)) {
								/*
								 * Increment
								 * the count
								 * for this
								 * file: 
								 */
								count_dec++;
							} else {
								/*
								 * Clear the
								 * file
								 * descriptor
								 * in the
								 * thread-spec
								 * ific file
								 * descriptor
								 * set: 
								 */
								FD_CLR(i, &pthread->data.select_data->readfds);
							}
						}
						/*
						 * Check if the current file
						 * descriptor is ready for
						 * any one of the operations: 
						 */
						if (count_dec > 0) {
							/*
							 * Increment the
							 * count of file
							 * descriptors that
							 * are ready for the
							 * current thread: 
							 */
							found_one++;
						}
					}

					/*
					 * Return the number of file
					 * descriptors that are ready: 
					 */
					pthread->data.select_data->nfds = found_one;

					/*
					 * Change the state of the current
					 * thread to run: 
					 */
					pthread->state = PS_RUNNING;
				}
				break;
			}
		}
	}

	/* Nothing to return. */
	return;
}

void
_thread_kern_set_timeout(struct timespec * timeout)
{
	struct timespec current_time;
	struct timeval  tv;

	/* Reset the timeout flag for the running thread: */
	_thread_run->timeout = 0;

	/* Check if the thread is to wait forever: */
	if (timeout == NULL) {
		/*
		 * Set the wakeup time to something that can be recognised as
		 * different to an actual time of day: 
		 */
		_thread_run->wakeup_time.tv_sec = -1;
		_thread_run->wakeup_time.tv_nsec = -1;
	}
	/* Check if no waiting is required: */
	else if (timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
		/* Set the wake up time to 'immediately': */
		_thread_run->wakeup_time.tv_sec = 0;
		_thread_run->wakeup_time.tv_nsec = 0;
	} else {
		/* Get the current time: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &current_time);

		/* Calculate the time for the current thread to wake up: */
		_thread_run->wakeup_time.tv_sec = current_time.tv_sec + timeout->tv_sec;
		_thread_run->wakeup_time.tv_nsec = current_time.tv_nsec + timeout->tv_nsec;

		/* Check if the nanosecond field needs to wrap: */
		if (_thread_run->wakeup_time.tv_nsec >= 1000000000) {
			/* Wrap the nanosecond field: */
			_thread_run->wakeup_time.tv_sec += 1;
			_thread_run->wakeup_time.tv_nsec -= 1000000000;
		}
	}
	return;
}
#endif
