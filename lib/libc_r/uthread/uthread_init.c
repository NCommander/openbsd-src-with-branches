/*	$OpenBSD$	*/
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: uthread_init.c,v 1.18 1999/08/28 00:03:36 peter Exp $
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <paths.h>
#include <poll.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/mman.h>
#ifdef _THREAD_SAFE
#include <machine/reg.h>
#include <pthread.h>
#include "pthread_private.h"

/* Global thread variables. */
struct pthread	_thread_kern_thread;
struct pthread *volatile _thread_run = &_thread_kern_thread;
struct pthread *volatile _last_user_thread = &_thread_kern_thread;
struct pthread *volatile _thread_single = NULL;
_thread_list_t	_thread_list = TAILQ_HEAD_INITIALIZER(_thread_list);
int		_thread_kern_pipe[2] = { -1, -1 };
int volatile	_queue_signals = 0;
int		_thread_kern_in_sched = 0;
struct timeval	kern_inc_prio_time = { 0, 0 };
_thread_list_t	_dead_list = TAILQ_HEAD_INITIALIZER(_dead_list);
struct pthread *_thread_initial = NULL;
struct pthread_attr pthread_attr_default = { 
                SCHED_RR, 0, TIMESLICE_USEC, PTHREAD_DEFAULT_PRIORITY,
                PTHREAD_CREATE_RUNNING, PTHREAD_CREATE_JOINABLE,
                NULL, NULL, NULL, PTHREAD_STACK_DEFAULT };
struct pthread_mutex_attr pthread_mutexattr_default = { 
		PTHREAD_MUTEX_DEFAULT, PTHREAD_PRIO_NONE, 0, 0 };
struct pthread_cond_attr pthread_condattr_default = { COND_TYPE_FAST, 0 };
int		_pthread_stdio_flags[3];
struct fd_table_entry **_thread_fd_table = NULL;
struct pollfd *_thread_pfd_table = NULL;
const int	dtablecount = 4096/sizeof(struct fd_table_entry);
int		_thread_dtablesize = 0;
int		_clock_res_nsec = CLOCK_RES_NSEC;
pthread_mutex_t	_gc_mutex = NULL;
pthread_cond_t	_gc_cond = NULL;
struct sigaction _thread_sigact[NSIG];
pq_queue_t	_readyq;
_thread_list_t	_waitingq;
_thread_list_t	_workq;
volatile int	_spinblock_count = 0;
volatile int	_sigq_check_reqd = 0;
pthread_switch_routine_t _sched_switch_hook = NULL;
_stack_list_t	_stackq;
int		_thread_kern_new_state = 0;

extern int _thread_autoinit_dummy_decl;

#ifdef GCC_2_8_MADE_THREAD_AWARE
typedef void *** (*dynamic_handler_allocator)();
extern void __set_dynamic_handler_allocator(dynamic_handler_allocator);

static pthread_key_t except_head_key;

typedef struct {
	void **__dynamic_handler_chain;
	void *top_elt[2];
} except_struct;

static void ***dynamic_allocator_handler_fn()
{
	except_struct *dh = (except_struct *)pthread_getspecific(except_head_key);

	if(dh == NULL) {
		dh = (except_struct *)malloc( sizeof(except_struct) );
		memset(dh, '\0', sizeof(except_struct));
		dh->__dynamic_handler_chain= dh->top_elt;
		pthread_setspecific(except_head_key, (void *)dh);
	}
	return &dh->__dynamic_handler_chain;
}
#endif /* GCC_2_8_MADE_THREAD_AWARE */

/*
 * Threaded process initialization
 */
void
_thread_init(void)
{
	int		fd;
	int             flags;
	int             i;
	size_t		len;
	int		mib[2];
	struct clockinfo clockinfo;
	struct sigaction act;

	/* Check if this function has already been called: */
	if (_thread_initial)
		/* Only initialise the threaded application once. */
		return;

	/*
	 * Check for the special case of this process running as
	 * or in place of init as pid = 1:
	 */
	if (getpid() == 1) {
		/*
		 * Setup a new session for this process which is
		 * assumed to be running as root.
		 */
		if (setsid() == -1)
			PANIC("Can't set session ID");
		if (revoke(_PATH_CONSOLE) != 0)
			PANIC("Can't revoke console");
		if ((fd = _thread_sys_open(_PATH_CONSOLE, O_RDWR)) < 0)
			PANIC("Can't open console");
		if (setlogin("root") == -1)
			PANIC("Can't set login to root");
		if (_thread_sys_ioctl(fd,TIOCSCTTY, (char *) NULL) == -1)
			PANIC("Can't set controlling terminal");
		if (_thread_sys_dup2(fd,0) == -1 ||
		    _thread_sys_dup2(fd,1) == -1 ||
		    _thread_sys_dup2(fd,2) == -1)
			PANIC("Can't dup2");
	}

	/* Get the standard I/O flags before messing with them : */
	for (i = 0; i < 3; i++)
		if (((_pthread_stdio_flags[i] =
		    _thread_sys_fcntl(i,F_GETFL, NULL)) == -1) &&
		    (errno != EBADF))
			PANIC("Cannot get stdio flags");

	/*
	 * Create a pipe that is written to by the signal handler to prevent
	 * signals being missed in calls to _select: 
	 */
	if (_thread_sys_pipe(_thread_kern_pipe) != 0) {
		/* Cannot create pipe, so abort: */
		PANIC("Cannot create kernel pipe");
	}
	/* Get the flags for the read pipe: */
	else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[0], F_GETFL, NULL)) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel read pipe flags");
	}
	/* Make the read pipe non-blocking: */
	else if (_thread_sys_fcntl(_thread_kern_pipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
		/* Abort this application: */
		PANIC("Cannot make kernel read pipe non-blocking");
	}
	/* Get the flags for the write pipe: */
	else if ((flags = _thread_sys_fcntl(_thread_kern_pipe[1], F_GETFL, NULL)) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel write pipe flags");
	}
	/* Make the write pipe non-blocking: */
	else if (_thread_sys_fcntl(_thread_kern_pipe[1], F_SETFL, flags | O_NONBLOCK) == -1) {
		/* Abort this application: */
		PANIC("Cannot get kernel write pipe flags");
	}
	/* Allocate and initialize the ready queue: */
	else if (_pq_alloc(&_readyq, PTHREAD_MIN_PRIORITY, PTHREAD_MAX_PRIORITY) != 0) {
		/* Abort this application: */
		PANIC("Cannot allocate priority ready queue.");
	}
	/* Allocate memory for the thread structure of the initial thread: */
	else if ((_thread_initial = (pthread_t) malloc(sizeof(struct pthread))) == NULL) {
		/*
		 * Insufficient memory to initialise this application, so
		 * abort: 
		 */
		PANIC("Cannot allocate memory for initial thread");
	} else {
		/* Zero the global kernel thread structure: */
		memset(&_thread_kern_thread, 0, sizeof(struct pthread));
		_thread_kern_thread.flags = PTHREAD_FLAGS_PRIVATE;
		memset(_thread_initial, 0, sizeof(struct pthread));

		/* Initialize the waiting and work queues: */
		TAILQ_INIT(&_waitingq);
		TAILQ_INIT(&_workq);

		/* Initialize the scheduling switch hook routine: */
		_sched_switch_hook = NULL;

		/* Initialize the thread stack cache: */
		SLIST_INIT(&_stackq);

		/*
		 * Write a magic value to the thread structure
		 * to help identify valid ones:
		 */
		_thread_initial->magic = PTHREAD_MAGIC;

		/* Default the priority of the initial thread: */
		_thread_initial->base_priority = PTHREAD_DEFAULT_PRIORITY;
		_thread_initial->active_priority = PTHREAD_DEFAULT_PRIORITY;
		_thread_initial->inherited_priority = 0;

		/* Initialise the state of the initial thread: */
		_thread_initial->state = PS_RUNNING;

		/* Initialise the queue: */
		TAILQ_INIT(&(_thread_initial->join_queue));

		/* Initialize the owned mutex queue and count: */
		TAILQ_INIT(&(_thread_initial->mutexq));
		_thread_initial->priority_mutex_count = 0;

		/* Give it a useful name */
		pthread_set_name_np(_thread_initial, "main");

		/* Initialise the rest of the fields: */
		_thread_initial->poll_data.nfds = 0;
		_thread_initial->poll_data.fds = NULL;
		_thread_initial->sig_defer_count = 0;
		_thread_initial->yield_on_sig_undefer = 0;
		_thread_initial->specific_data = NULL;
		_thread_initial->cleanup = NULL;
		_thread_initial->flags = 0;
		_thread_initial->error = 0;
		_thread_initial->cancelstate = PTHREAD_CANCEL_ENABLE;
		_thread_initial->canceltype = PTHREAD_CANCEL_DEFERRED;
		_SPINLOCK_INIT(&_thread_initial->lock);
		TAILQ_INIT(&_thread_list);
		TAILQ_INSERT_HEAD(&_thread_list, _thread_initial, tle);
		_thread_run = _thread_initial;

		/* Initialise the global signal action structure: */
		sigfillset(&act.sa_mask);
		act.sa_handler = (void (*) ()) _thread_sig_handler;
		act.sa_flags = 0;

		/* Initialize signal handling: */
		_thread_sig_init();

		/* Enter a loop to get the existing signal status: */
		for (i = 1; i < NSIG; i++) {
			/* Check for signals which cannot be trapped: */
			if (i == SIGKILL || i == SIGSTOP) {
			}

			/* Get the signal handler details: */
			else if (_thread_sys_sigaction(i, NULL,
						       &_thread_sigact[i - 1]) != 0) {
				/*
				 * Abort this process if signal
				 * initialisation fails: 
				 */
				PANIC("Cannot read signal handler info");
			}
		}

		/*
		 * Install the signal handler for the most important
		 * signals that the user-thread kernel needs. Actually
		 * SIGINFO isn't really needed, but it is nice to have.
		 */
		if (_thread_sys_sigaction(_SCHED_SIGNAL, &act, NULL) != 0 ||
		    _thread_sys_sigaction(SIGINFO,       &act, NULL) != 0 ||
		    _thread_sys_sigaction(SIGCHLD,       &act, NULL) != 0) {
			/*
			 * Abort this process if signal initialisation fails: 
			 */
			PANIC("Cannot initialise signal handler");
		}

		/* Get the kernel clockrate: */
		mib[0] = CTL_KERN;
		mib[1] = KERN_CLOCKRATE;
		len = sizeof (struct clockinfo);
		if (sysctl(mib, 2, &clockinfo, &len, NULL, 0) == 0)
			_clock_res_nsec = clockinfo.tick * 1000;

		/* Get the table size: */
		if ((_thread_dtablesize = getdtablesize()) < 0) {
			/*
			 * Cannot get the system defined table size, so abort
			 * this process. 
			 */
			PANIC("Cannot get dtablesize");
		}
		/* Allocate memory for the file descriptor table: */
		if ((_thread_fd_table = (struct fd_table_entry **) malloc(sizeof(struct fd_table_entry *) * _thread_dtablesize)) == NULL) {
			/* Avoid accesses to file descriptor table on exit: */
			_thread_dtablesize = 0;

			/*
			 * Cannot allocate memory for the file descriptor
			 * table, so abort this process. 
			 */
			PANIC("Cannot allocate memory for file descriptor table");
		}
		/* Allocate memory for the pollfd table: */
		if ((_thread_pfd_table = (struct pollfd *) malloc(sizeof(struct pollfd) * _thread_dtablesize)) == NULL) {
			/*
			 * Cannot allocate memory for the file descriptor
			 * table, so abort this process. 
			 */
			PANIC("Cannot allocate memory for pollfd table");
		} else {
			/*
			 * Enter a loop to initialise the file descriptor
			 * table: 
			 */
			for (i = 0; i < _thread_dtablesize; i++) {
				/* Initialise the file descriptor table: */
				_thread_fd_table[i] = NULL;
			}

			/* Initialize stdio file descriptor table entries: */
			for (i = 0; i < 3; i++) {
				if ((_thread_fd_table_init(i) != 0) &&
				    (errno != EBADF))
					PANIC("Cannot initialize stdio file "
					    "descriptor table entry");
			}
		}
	}

#ifdef GCC_2_8_MADE_THREAD_AWARE
	/* Create the thread-specific data for the exception linked list. */
	if(pthread_key_create(&except_head_key, NULL) != 0)
		PANIC("Failed to create thread specific execption head");

	/* Setup the gcc exception handler per thread. */
	__set_dynamic_handler_allocator( dynamic_allocator_handler_fn );
#endif /* GCC_2_8_MADE_THREAD_AWARE */

	/* Initialise the garbage collector mutex and condition variable. */
	if (pthread_mutex_init(&_gc_mutex,NULL) != 0 ||
	    pthread_cond_init(&_gc_cond,NULL) != 0)
		PANIC("Failed to initialise garbage collector mutex or condvar");

	gettimeofday(&kern_inc_prio_time, NULL);

	_thread_autoinit_dummy_decl = 0;

	return;
}
#endif
