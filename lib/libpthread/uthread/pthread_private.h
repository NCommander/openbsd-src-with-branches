/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>.
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
 * Private thread definitions for the uthread kernel.
 *
 */

#ifndef _PTHREAD_PRIVATE_H
#define _PTHREAD_PRIVATE_H

#include <paths.h>

/*
 * Include files.
 */
#include <setjmp.h>
#include <signal.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sched.h>
#include <spinlock.h>
#include "uthread_machdep.h"

/*
 * Kernel fatal error handler macro.
 */
#define PANIC(string)   _thread_exit(__FILE__,__LINE__,string)

/* Output debug messages like this: */
#define	stdout_debug(_x)	_write(1,_x,strlen(_x));
#define	stderr_debug(_x)	_write(2,_x,strlen(_x));

/*
 * State change macro:
 */
#define PTHREAD_NEW_STATE(thrd, newstate) {				\
	(thrd)->state = newstate;					\
	(thrd)->fname = __FILE__;					\
	(thrd)->lineno = __LINE__;					\
}

/*
 * Queue definitions.
 */
struct pthread_queue {
	struct pthread	*q_next;
	struct pthread	*q_last;
	void		*q_data;
};

/*
 * Static queue initialization values. 
 */
#define PTHREAD_QUEUE_INITIALIZER { NULL, NULL, NULL }

/* 
 * Mutex definitions.
 */
union pthread_mutex_data {
	void	*m_ptr;
	int	m_count;
};

struct pthread_mutex {
	enum pthread_mutextype		m_type;
	struct pthread_queue		m_queue;
	struct pthread			*m_owner;
	union pthread_mutex_data	m_data;
	long				m_flags;

	/*
	 * Lock for accesses to this structure.
	 */
	spinlock_t			lock;
};

/*
 * Flags for mutexes. 
 */
#define MUTEX_FLAGS_PRIVATE	0x01
#define MUTEX_FLAGS_INITED	0x02
#define MUTEX_FLAGS_BUSY	0x04

/*
 * Static mutex initialization values. 
 */
#define PTHREAD_MUTEX_STATIC_INITIALIZER   \
	{ MUTEX_TYPE_FAST, PTHREAD_QUEUE_INITIALIZER, \
	NULL, { NULL }, MUTEX_FLAGS_INITED }

struct pthread_mutex_attr {
	enum pthread_mutextype	m_type;
	long			m_flags;
};

/* 
 * Condition variable definitions.
 */
enum pthread_cond_type {
	COND_TYPE_FAST,
	COND_TYPE_MAX
};

struct pthread_cond {
	enum pthread_cond_type	c_type;
	struct pthread_queue	c_queue;
	void			*c_data;
	long			c_flags;

	/*
	 * Lock for accesses to this structure.
	 */
	spinlock_t		lock;
};

struct pthread_cond_attr {
	enum pthread_cond_type	c_type;
	long			c_flags;
};

/*
 * Flags for condition variables.
 */
#define COND_FLAGS_PRIVATE	0x01
#define COND_FLAGS_INITED	0x02
#define COND_FLAGS_BUSY		0x04

/*
 * Static cond initialization values. 
 */
#define PTHREAD_COND_STATIC_INITIALIZER    \
	{ COND_TYPE_FAST, PTHREAD_QUEUE_INITIALIZER, NULL, COND_FLAGS_INITED }

/*
 * Cleanup definitions.
 */
struct pthread_cleanup {
	struct pthread_cleanup	*next;
	void			(*routine) ();
	void			*routine_arg;
};

struct pthread_attr {
	int	schedparam_policy;
	int	prio;
	int	suspend;
	int	flags;
	void	*arg_attr;
	void	(*cleanup_attr) ();
	void	*stackaddr_attr;
	size_t	stacksize_attr;
};

/*
 * Thread creation state attributes.
 */
#define PTHREAD_CREATE_RUNNING			0
#define PTHREAD_CREATE_SUSPENDED		1

/*
 * Miscellaneous definitions.
 */
#define PTHREAD_STACK_DEFAULT			65536
#define PTHREAD_DEFAULT_PRIORITY		64
#define PTHREAD_MAX_PRIORITY			126
#define PTHREAD_MIN_PRIORITY			0
#define _POSIX_THREAD_ATTR_STACKSIZE

/*
 * Clock resolution in nanoseconds.
 */
#define CLOCK_RES_NSEC				10000000

/*
 * Number of microseconds between incremental priority updates for
 * threads that are ready to run, but denied being run.
 */
#define INC_PRIO_USEC				500000

/*
 * Time slice period in microseconds.
 */
#define TIMESLICE_USEC				100000

struct pthread_key {
	spinlock_t	lock;
	volatile int	allocated;
	volatile int	count;
	void            (*destructor) ();
};

/*
 * Thread states.
 */
enum pthread_state {
	PS_RUNNING,
	PS_SIGTHREAD,
	PS_MUTEX_WAIT,
	PS_COND_WAIT,
	PS_FDLR_WAIT,
	PS_FDLW_WAIT,
	PS_FDR_WAIT,
	PS_FDW_WAIT,
	PS_FILE_WAIT,
	PS_SELECT_WAIT,
	PS_SLEEP_WAIT,
	PS_WAIT_WAIT,
	PS_SIGWAIT,
	PS_JOIN,
	PS_SUSPENDED,
	PS_DEAD,
	PS_STATE_MAX
};


/*
 * File descriptor locking definitions.
 */
#define FD_READ             0x1
#define FD_WRITE            0x2
#define FD_RDWR             (FD_READ | FD_WRITE)

/*
 * File descriptor table structure.
 */
struct fd_table_entry {
	/*
	 * Lock for accesses to this file descriptor table
	 * entry. This is passed to _spinlock() to provide atomic
	 * access to this structure. It does *not* represent the
	 * state of the lock on the file descriptor.
	 */
	spinlock_t		lock;
	struct pthread_queue	r_queue;	/* Read queue.                        */
	struct pthread_queue	w_queue;	/* Write queue.                       */
	struct pthread		*r_owner;	/* Ptr to thread owning read lock.    */
	struct pthread		*w_owner;	/* Ptr to thread owning write lock.   */
	char			*r_fname;	/* Ptr to read lock source file name  */
	int			r_lineno;	/* Read lock source line number.      */
	char			*w_fname;	/* Ptr to write lock source file name */
	int			w_lineno;	/* Write lock source line number.     */
	int			r_lockcount;	/* Count for FILE read locks.         */
	int			w_lockcount;	/* Count for FILE write locks.        */
	int			flags;		/* Flags used in open.                */
};

struct pthread_select_data {
	int	nfds;
	fd_set	readfds;
	fd_set	writefds;
	fd_set	exceptfds;
};

union pthread_wait_data {
	pthread_mutex_t	*mutex;
	pthread_cond_t	*cond;
	const sigset_t	*sigwait;	/* Waiting on a signal in sigwait */
	struct {
		short	fd;		/* Used when thread waiting on fd */
		short	branch;		/* Line number, for debugging.    */
		char	*fname;		/* Source file name for debugging.*/
	} fd;
	struct pthread_select_data * select_data;
};

/*
 * Thread structure.
 */
struct pthread {
	/*
	 * Magic value to help recognize a valid thread structure
	 * from an invalid one:
	 */
#define	PTHREAD_MAGIC		((u_int32_t) 0xd09ba115)
	u_int32_t		magic;
	char			*name;

	/*
	 * Lock for accesses to this thread structure.
	 */
	spinlock_t		lock;

	/*
	 * Pointer to the next thread in the thread linked list.
	 */
	struct pthread	*nxt;

	/*
	 * Thread start routine, argument, stack pointer and thread
	 * attributes.
	 */
	void			*(*start_routine)(void *);
	void			*arg;
	void			*stack;
	struct pthread_attr	attr;

	struct _machdep_struct	_machdep;

	/*
	 * Saved signal context used in call to sigreturn by
	 * _thread_kern_sched if sig_saved is TRUE.
	 */
	struct  sigcontext saved_sigcontext;

	/* 
	 * Saved jump buffer used in call to longjmp by _thread_kern_sched
	 * if sig_saved is FALSE.
	 */
	jmp_buf	saved_jmp_buf;

	/*
	 * TRUE if the last state saved was a signal context. FALSE if the
	 * last state saved was a jump buffer.
	 */
	int	sig_saved;

	/*
	 * Current signal mask and pending signals.
	 */
	sigset_t	sigmask;
	sigset_t	sigpend;

	/* Thread state: */
	enum pthread_state	state;

	/* Time that this thread was last made active. */
	struct  timeval		last_active;

	/* Time that this thread was last made inactive. */
	struct  timeval		last_inactive;

	/*
	 * Number of microseconds accumulated by this thread when
	 * time slicing is active.
	 */
	long	slice_usec;

	/*
	 * Incremental priority accumulated by thread while it is ready to
	 * run but is denied being run.
	 */
	int	inc_prio;

	/*
	 * Time to wake up thread. This is used for sleeping threads and
	 * for any operation which may time out (such as select).
	 */
	struct timespec	wakeup_time;

	/* TRUE if operation has timed out. */
	int	timeout;

	/*
	 * Error variable used instead of errno. The function __error()
	 * returns a pointer to this. 
	 */
	int	error;

	/* Join queue for waiting threads: */
	struct pthread_queue	join_queue;

	/*
	 * The current thread can belong to only one queue at a time.
	 *
	 * Pointer to queue (if any) on which the current thread is waiting.
	 *
	 * XXX The queuing should be changed to use the TAILQ entry below.
	 * XXX For the time being, it's hybrid.
	 */
	struct pthread_queue	*queue;

	/* Pointer to next element in queue. */
	struct pthread	*qnxt;

	/* Queue entry for this thread: */
	TAILQ_ENTRY(pthread) qe;

	/* Wait data. */
	union pthread_wait_data data;

	/*
	 * Set to TRUE if a blocking operation was
	 * interrupted by a signal:
	 */
	int		interrupted;

	/* Signal number when in state PS_SIGWAIT: */
	int		signo;

	/* Miscellaneous data. */
	char		flags;
#define PTHREAD_EXITING		0x0100
	char		pthread_priority;
	void		*ret;
	const void	**specific_data;
	int		specific_data_count;

	/* Cleanup handlers Link List */
	struct pthread_cleanup *cleanup;
	char			*fname;	/* Ptr to source file name  */
	int			lineno;	/* Source line number.      */
};

/*
 * Global variables for the uthread kernel.
 */

/* Kernel thread structure used when there are no running threads: */
extern struct pthread   _thread_kern_thread;

/* Ptr to the thread structure for the running thread: */
extern struct pthread   * volatile _thread_run;

/*
 * Ptr to the thread running in single-threaded mode or NULL if
 * running multi-threaded (default POSIX behaviour).
 */
extern struct pthread   * volatile _thread_single;

/* Ptr to the first thread in the thread linked list: */
extern struct pthread   * volatile _thread_link_list;

/*
 * Array of kernel pipe file descriptors that are used to ensure that
 * no signals are missed in calls to _select.
 */
extern int              _thread_kern_pipe[2];
extern int              _thread_kern_in_select;
extern int              _thread_kern_in_sched;

/* Last time that an incremental priority update was performed: */
extern struct timeval   kern_inc_prio_time;

/* Dead threads: */
extern struct pthread * volatile _thread_dead;

/* Initial thread: */
extern struct pthread *_thread_initial;

/* Default thread attributes: */
extern struct pthread_attr pthread_attr_default;

/* Default mutex attributes: */
extern struct pthread_mutex_attr pthread_mutexattr_default;

/* Default condition variable attributes: */
extern struct pthread_cond_attr pthread_condattr_default;

/*
 * Standard I/O file descriptors need special flag treatment since
 * setting one to non-blocking does all on *BSD. Sigh. This array
 * is used to store the initial flag settings.
 */
extern int	_pthread_stdio_flags[3];

/* File table information: */
extern struct fd_table_entry **_thread_fd_table;
extern int    _thread_dtablesize;

/*
 * Array of signal actions for this process.
 */
extern struct  sigaction _thread_sigact[NSIG];

/*
 * Where SIGINFO writes thread states when /dev/tty cannot be opened
 */
#define INFO_DUMP_FILE  "/tmp/uthread.dump"

#ifdef	_LOCK_DEBUG
#define	_FD_LOCK(_fd,_type,_ts)		_thread_fd_lock_debug(_fd, _type, \
						_ts, __FILE__, __LINE__)
#define _FD_UNLOCK(_fd,_type)		_thread_fd_unlock_debug(_fd, _type, \
						__FILE__, __LINE__)
#else
#define	_FD_LOCK(_fd,_type,_ts)		_thread_fd_lock(_fd, _type, _ts)
#define _FD_UNLOCK(_fd,_type)		_thread_fd_unlock(_fd, _type)
#endif

/*
 * Function prototype definitions.
 */
__BEGIN_DECLS
int     _find_dead_thread(pthread_t);
int     _find_thread(pthread_t);
int     _thread_create(pthread_t *,const pthread_attr_t *,void *(*start_routine)(void *),void *,pthread_t);
int     _thread_fd_lock(int, int, struct timespec *);
int     _thread_fd_lock_debug(int, int, struct timespec *,char *fname,int lineno);
void    _dispatch_signals(void);
void    _thread_signal(pthread_t, int);
void    _lock_dead_thread_list(void);
void    _lock_thread(void);
void    _lock_thread_list(void);
void    _unlock_dead_thread_list(void);
void    _unlock_thread(void);
void    _unlock_thread_list(void);
void    _thread_exit(char *, int, char *);
void    _thread_fd_unlock(int, int);
void    _thread_fd_unlock_debug(int, int, char *, int);
void    *_thread_cleanup(pthread_t);
void    _thread_cleanupspecific(void);
void    _thread_dump_info(void);
void    _thread_init(void) __attribute__((constructor));
void    _thread_kern_sched(struct sigcontext *);
void    _thread_kern_sched_state(enum pthread_state,char *fname,int lineno);
void    _thread_kern_set_timeout(struct timespec *);
void    _thread_sig_handler(int, int, struct sigcontext *);
void    _thread_start(void);
void    _thread_start_sig_handler(void);
void	_thread_seterrno(pthread_t,int);
void    _thread_queue_init(struct pthread_queue *);
void    _thread_queue_enq(struct pthread_queue *, struct pthread *);
int     _thread_queue_remove(struct pthread_queue *, struct pthread *);
int     _thread_fd_table_init(int fd);
struct pthread *_thread_queue_get(struct pthread_queue *);
struct pthread *_thread_queue_deq(struct pthread_queue *);

/* #include <signal.h> */
#ifdef _USER_SIGNAL_H
int     _thread_sys_sigaction(int, const struct sigaction *, struct sigaction *);
int     _thread_sys_sigpending(sigset_t *);
int     _thread_sys_sigprocmask(int, const sigset_t *, sigset_t *);
int     _thread_sys_sigsuspend(const sigset_t *);
int     _thread_sys_siginterrupt(int, int);
int     _thread_sys_sigpause(int);
int     _thread_sys_sigreturn(struct sigcontext *);
int     _thread_sys_sigstack(const struct sigstack *, struct sigstack *);
int     _thread_sys_sigvec(int, struct sigvec *, struct sigvec *);
void    _thread_sys_psignal(unsigned int, const char *);
void    (*_thread_sys_signal(int, void (*)(int)))(int);
#endif

/* #include <sys/stat.h> */
#ifdef  _SYS_STAT_H_
int     _thread_sys_fchmod(int, mode_t);
int     _thread_sys_fstat(int, struct stat *);
int     _thread_sys_fchflags(int, u_long);
#endif

/* #include <sys/mount.h> */
#ifdef  _SYS_MOUNT_H_
int     _thread_sys_fstatfs(int, struct statfs *);
#endif
int     _thread_sys_pipe(int *);

/* #include <sys/socket.h> */
#ifdef  _SYS_SOCKET_H_
int     _thread_sys_accept(int, struct sockaddr *, int *);
int     _thread_sys_bind(int, const struct sockaddr *, int);
int     _thread_sys_connect(int, const struct sockaddr *, int);
int     _thread_sys_getpeername(int, struct sockaddr *, int *);
int     _thread_sys_getsockname(int, struct sockaddr *, int *);
int     _thread_sys_getsockopt(int, int, int, void *, int *);
int     _thread_sys_listen(int, int);
int     _thread_sys_setsockopt(int, int, int, const void *, int);
int     _thread_sys_shutdown(int, int);
int     _thread_sys_socket(int, int, int);
int     _thread_sys_socketpair(int, int, int, int *);
ssize_t _thread_sys_recv(int, void *, size_t, int);
ssize_t _thread_sys_recvfrom(int, void *, size_t, int, struct sockaddr *, int *);
ssize_t _thread_sys_recvmsg(int, struct msghdr *, int);
ssize_t _thread_sys_send(int, const void *, size_t, int);
ssize_t _thread_sys_sendmsg(int, const struct msghdr *, int);
ssize_t _thread_sys_sendto(int, const void *,size_t, int, const struct sockaddr *, int);
#endif

/* #include <stdio.h> */
#ifdef  _STDIO_H_
FILE    *_thread_sys_fdopen(int, const char *);
FILE    *_thread_sys_fopen(const char *, const char *);
FILE    *_thread_sys_freopen(const char *, const char *, FILE *);
FILE    *_thread_sys_popen(const char *, const char *);
FILE    *_thread_sys_tmpfile(void);
char    *_thread_sys_ctermid(char *);
char    *_thread_sys_cuserid(char *);
char    *_thread_sys_fgetln(FILE *, size_t *);
char    *_thread_sys_fgets(char *, int, FILE *);
char    *_thread_sys_gets(char *);
char    *_thread_sys_tempnam(const char *, const char *);
char    *_thread_sys_tmpnam(char *);
int     _thread_sys_fclose(FILE *);
int     _thread_sys_feof(FILE *);
int     _thread_sys_ferror(FILE *);
int     _thread_sys_fflush(FILE *);
int     _thread_sys_fgetc(FILE *);
int     _thread_sys_fgetpos(FILE *, fpos_t *);
int     _thread_sys_fileno(FILE *);
int     _thread_sys_fprintf(FILE *, const char *, ...);
int     _thread_sys_fpurge(FILE *);
int     _thread_sys_fputc(int, FILE *);
int     _thread_sys_fputs(const char *, FILE *);
int     _thread_sys_fscanf(FILE *, const char *, ...);
int     _thread_sys_fseek(FILE *, long, int);
int     _thread_sys_fsetpos(FILE *, const fpos_t *);
int     _thread_sys_getc(FILE *);
int     _thread_sys_getchar(void);
int     _thread_sys_getw(FILE *);
int     _thread_sys_pclose(FILE *);
int     _thread_sys_printf(const char *, ...);
int     _thread_sys_putc(int, FILE *);
int     _thread_sys_putchar(int);
int     _thread_sys_puts(const char *);
int     _thread_sys_putw(int, FILE *);
int     _thread_sys_remove(const char *);
int     _thread_sys_rename (const char *, const char *);
int     _thread_sys_scanf(const char *, ...);
int     _thread_sys_setlinebuf(FILE *);
int     _thread_sys_setvbuf(FILE *, char *, int, size_t);
int     _thread_sys_snprintf(char *, size_t, const char *, ...);
int     _thread_sys_sprintf(char *, const char *, ...);
int     _thread_sys_sscanf(const char *, const char *, ...);
int     _thread_sys_ungetc(int, FILE *);
int     _thread_sys_vfprintf(FILE *, const char *, _BSD_VA_LIST_);
int     _thread_sys_vprintf(const char *, _BSD_VA_LIST_);
int     _thread_sys_vscanf(const char *, _BSD_VA_LIST_);
int     _thread_sys_vsnprintf(char *, size_t, const char *, _BSD_VA_LIST_);
int     _thread_sys_vsprintf(char *, const char *, _BSD_VA_LIST_);
int     _thread_sys_vsscanf(const char *, const char *, _BSD_VA_LIST_);
long    _thread_sys_ftell(FILE *);
size_t  _thread_sys_fread(void *, size_t, size_t, FILE *);
size_t  _thread_sys_fwrite(const void *, size_t, size_t, FILE *);
void    _thread_sys_clearerr(FILE *);
void    _thread_sys_perror(const char *);
void    _thread_sys_rewind(FILE *);
void    _thread_sys_setbuf(FILE *, char *);
void    _thread_sys_setbuffer(FILE *, char *, int);
#endif

/* #include <unistd.h> */
#ifdef  _UNISTD_H_
char    *_thread_sys_ttyname(int);
int     _thread_sys_close(int);
int     _thread_sys_dup(int);
int     _thread_sys_dup2(int, int);
int     _thread_sys_exect(const char *, char * const *, char * const *);
int     _thread_sys_execve(const char *, char * const *, char * const *);
int     _thread_sys_fchdir(int);
int     _thread_sys_fchown(int, uid_t, gid_t);
int     _thread_sys_fsync(int);
int     _thread_sys_ftruncate(int, off_t);
int     _thread_sys_pause(void);
int     _thread_sys_pipe(int *);
int     _thread_sys_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
off_t   _thread_sys_lseek(int, off_t, int);
pid_t   _thread_sys_fork(void);
pid_t   _thread_sys_tcgetpgrp(int);
ssize_t _thread_sys_read(int, void *, size_t);
ssize_t _thread_sys_write(int, const void *, size_t);
void	_thread_sys__exit(int);
#endif

/* #include <fcntl.h> */
#ifdef  _SYS_FCNTL_H_
int     _thread_sys_creat(const char *, mode_t);
int     _thread_sys_fcntl(int, int, ...);
int     _thread_sys_flock(int, int);
int     _thread_sys_open(const char *, int, ...);
#endif

/* #include <sys/ioctl.h> */
#ifdef  _SYS_IOCTL_H_
int     _thread_sys_ioctl(int, unsigned long, ...);
#endif

/* #include <dirent.h> */
#ifdef  _DIRENT_H_
DIR     *___thread_sys_opendir2(const char *, int);
DIR     *_thread_sys_opendir(const char *);
int     _thread_sys_alphasort(const void *, const void *);
int     _thread_sys_scandir(const char *, struct dirent ***,
	int (*)(struct dirent *), int (*)(const void *, const void *));
int     _thread_sys_closedir(DIR *);
int     _thread_sys_getdirentries(int, char *, int, long *);
long    _thread_sys_telldir(const DIR *);
struct  dirent *_thread_sys_readdir(DIR *);
void    _thread_sys_rewinddir(DIR *);
void    _thread_sys_seekdir(DIR *, long);
#endif

/* #include <sys/uio.h> */
#ifdef  _SYS_UIO_H_
ssize_t _thread_sys_readv(int, const struct iovec *, int);
ssize_t _thread_sys_writev(int, const struct iovec *, int);
#endif

/* #include <sys/wait.h> */
#ifdef _SYS_WAIT_H_
pid_t   _thread_sys_wait(int *);
pid_t   _thread_sys_waitpid(pid_t, int *, int);
pid_t   _thread_sys_wait3(int *, int, struct rusage *);
pid_t   _thread_sys_wait4(pid_t, int *, int, struct rusage *);
#endif

__END_DECLS

#endif  /* !_PTHREAD_PRIVATE_H */
