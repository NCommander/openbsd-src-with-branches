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
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <paths.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ttycom.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <signal.h>
#ifdef _THREAD_SAFE
#include <machine/reg.h>
#include <pthread.h>
#include "pthread_private.h"

/* Allocate space for global thread variables here: */

struct pthread		  _thread_kern_thread;
struct pthread * volatile _thread_run = &_thread_kern_thread;
struct pthread * volatile _thread_single = NULL;
struct pthread * volatile _thread_link_list = NULL;
int             	  _thread_kern_pipe[2] = { -1, -1 };
int             	  _thread_kern_in_select = 0;
int             	  _thread_kern_in_sched = 0;
struct timeval  	  kern_inc_prio_time = { 0, 0 };
struct pthread * volatile _thread_dead = NULL;
struct pthread *	  _thread_initial = NULL;
struct pthread_attr 	  pthread_attr_default = {
	SCHED_RR,			/* schedparam_policy */
	PTHREAD_DEFAULT_PRIORITY,	/* prio */
	PTHREAD_CREATE_RUNNING,		/* suspend */
	PTHREAD_CREATE_JOINABLE,	/* flags */
	NULL,				/* arg_attr */
	NULL,				/* cleanup_attr */
	NULL,				/* stackaddr_attr */
	PTHREAD_STACK_DEFAULT		/* stacksize_attr */
};
struct pthread_mutex_attr pthread_mutexattr_default = {
	MUTEX_TYPE_FAST,		/* m_type */
	0				/* m_flags */
};
struct pthread_cond_attr pthread_condattr_default = {
	COND_TYPE_FAST,			/* c_type */
	0				/* c_flags */
};
int			  _pthread_stdio_flags[3];
struct fd_table_entry **  _thread_fd_table = NULL;
int    			  _thread_dtablesize = NOFILE_MAX;
pthread_mutex_t		  _gc_mutex = NULL;
pthread_cond_t		  _gc_cond = NULL;
struct  sigaction 	  _thread_sigact[NSIG];

/* Automatic init module. */
extern int _thread_autoinit_hook;

#ifdef GCC_2_8_MADE_THREAD_AWARE
/* see src/gnu/usr.bin/gcc/libgcc2.c */
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
		if ((_pthread_stdio_flags[i] =
		    _thread_sys_fcntl(i,F_GETFL, NULL)) == -1)
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
		_thread_kern_thread.magic = PTHREAD_MAGIC;
		pthread_set_name_np(&_thread_kern_thread, "kern");

		/* Zero the initial thread: */
		memset(_thread_initial, 0, sizeof(struct pthread));

		/* Default the priority of the initial thread: */
		_thread_initial->pthread_priority = PTHREAD_DEFAULT_PRIORITY;

		/* Initialise the state of the initial thread: */
		_thread_initial->state = PS_RUNNING;

		/* Initialise the queue: */
		_thread_queue_init(&(_thread_initial->join_queue));

		/* Initialise the rest of the fields: */
		_thread_initial->specific_data = NULL;
		_thread_initial->cleanup = NULL;
		_thread_initial->queue = NULL;
		_thread_initial->qnxt = NULL;
		_thread_initial->nxt = NULL;
		_thread_initial->flags = 0;
		_thread_initial->error = 0;
		_thread_initial->magic = PTHREAD_MAGIC;
		pthread_set_name_np(_thread_initial, "init");
		_thread_link_list = _thread_initial;
		_thread_run = _thread_initial;

		/* Initialise the global signal action structure: */
		sigfillset(&act.sa_mask);
		act.sa_handler = (void (*) ()) _thread_sig_handler;
		act.sa_flags = 0;

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
		if (_thread_sys_sigaction(SIGVTALRM, &act, NULL) != 0 ||
		    _thread_sys_sigaction(SIGINFO  , &act, NULL) != 0 ||
		    _thread_sys_sigaction(SIGCHLD  , &act, NULL) != 0) {
			/*
			 * Abort this process if signal initialisation fails: 
			 */
			PANIC("Cannot initialise signal handler");
		}

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
			/*
			 * Cannot allocate memory for the file descriptor
			 * table, so abort this process. 
			 */
			PANIC("Cannot allocate memory for file descriptor table");
		} else {
			/*
			 * Enter a loop to initialise the file descriptor
			 * table: 
			 */
			for (i = 0; i < _thread_dtablesize; i++) {
				/* Initialise the file descriptor table: */
				_thread_fd_table[i] = NULL;
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

	/* Pull in automatic thread unit. */
	_thread_autoinit_hook = 1;

	return;
}

#endif _THREAD_SAFE
