/* ====================================================================
 * Copyright (c) 1995-1998 The Apache Group.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * 4. The names "Apache Server" and "Apache Group" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    apache@apache.org.
 *
 * 5. Products derived from this software may not be called "Apache"
 *    nor may "Apache" appear in their names without prior written
 *    permission of the Apache Group.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the Apache Group
 *    for use in the Apache HTTP server project (http://www.apache.org/)."
 *
 * THIS SOFTWARE IS PROVIDED BY THE APACHE GROUP ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE APACHE GROUP OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This software consists of voluntary contributions made by many
 * individuals on behalf of the Apache Group and was originally based
 * on public domain software written at the National Center for
 * Supercomputing Applications, University of Illinois, Urbana-Champaign.
 * For more information on the Apache Group and the Apache HTTP server
 * project, please see <http://www.apache.org/>.
 *
 */

/*
 * httpd.c: simple http daemon for answering WWW file requests
 *
 * 
 * 03-21-93  Rob McCool wrote original code (up to NCSA HTTPd 1.3)
 * 
 * 03-06-95  blong
 *  changed server number for child-alone processes to 0 and changed name
 *   of processes
 *
 * 03-10-95  blong
 *      Added numerous speed hacks proposed by Robert S. Thau (rst@ai.mit.edu) 
 *      including set group before fork, and call gettime before to fork
 *      to set up libraries.
 *
 * 04-14-95  rst / rh
 *      Brandon's code snarfed from NCSA 1.4, but tinkered to work with the
 *      Apache server, and also to have child processes do accept() directly.
 *
 * April-July '95 rst
 *      Extensive rework for Apache.
 */

#ifndef SHARED_CORE_BOOTSTRAP
#ifndef SHARED_CORE_TIESTATIC

#ifdef SHARED_CORE
#define REALMAIN ap_main
int ap_main(int argc, char *argv[]);
#else
#define REALMAIN main
#endif

#define CORE_PRIVATE

#include "httpd.h"
#include "http_main.h"
#include "http_log.h"
#include "http_config.h"	/* for read_config */
#include "http_protocol.h"	/* for read_request */
#include "http_request.h"	/* for process_request */
#include "http_conf_globals.h"
#include "http_core.h"		/* for get_remote_host */
#include "http_vhost.h"
#include "util_script.h"	/* to force util_script.c linking */
#include "util_uri.h"
#include "scoreboard.h"
#include "multithread.h"
#include <sys/stat.h>
#ifdef USE_SHMGET_SCOREBOARD
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
#ifdef SecureWare
#include <sys/security.h>
#include <sys/audit.h>
#include <prot.h>
#endif
#ifdef WIN32
#include "../os/win32/getopt.h"
#elif !defined(BEOS)
#include <netinet/tcp.h>
#endif

#ifdef HAVE_BSTRING_H
#include <bstring.h>		/* for IRIX, FD_SET calls bzero() */
#endif

#ifdef MULTITHREAD
/* special debug stuff -- PCS */

/* Set this non-zero if you are prepared to put up with more than one log entry per second */
#define SEVERELY_VERBOSE	    0

  /* APD1() to APD5() are macros to help us debug. They can either
   * log to the screen or the error_log file. In release builds, these
   * macros do nothing. In debug builds, they send messages at priority
   * "debug" to the error log file, or if DEBUG_TO_CONSOLE is defined,
   * to the console.
   */

# ifdef _DEBUG
#  ifndef DEBUG_TO_CONSOLE
#   define APD1(a) ap_log_error(APLOG_MARK,APLOG_DEBUG|APLOG_NOERRNO,server_conf,a)
#   define APD2(a,b) ap_log_error(APLOG_MARK,APLOG_DEBUG|APLOG_NOERRNO,server_conf,a,b)
#   define APD3(a,b,c) ap_log_error(APLOG_MARK,APLOG_DEBUG|APLOG_NOERRNO,server_conf,a,b,c)
#   define APD4(a,b,c,d) ap_log_error(APLOG_MARK,APLOG_DEBUG|APLOG_NOERRNO,server_conf,a,b,c,d)
#   define APD5(a,b,c,d,e) ap_log_error(APLOG_MARK,APLOG_DEBUG|APLOG_NOERRNO,server_conf,a,b,c,d,e)
#  else
#   define APD1(a) printf("%s\n",a)
#   define APD2(a,b) do { printf(a,b);putchar('\n'); } while(0);
#   define APD3(a,b,c) do { printf(a,b,c);putchar('\n'); } while(0);
#   define APD4(a,b,c,d) do { printf(a,b,c,d);putchar('\n'); } while(0);
#   define APD5(a,b,c,d,e) do { printf(a,b,c,d,e);putchar('\n'); } while(0);
#  endif
# else /* !_DEBUG */
#  define APD1(a) 
#  define APD2(a,b) 
#  define APD3(a,b,c) 
#  define APD4(a,b,c,d) 
#  define APD5(a,b,c,d,e) 
# endif /* _DEBUG */
#endif /* MULTITHREAD */

/* This next function is never used. It is here to ensure that if we
 * make all the modules into shared libraries that core httpd still
 * includes the full Apache API. Without this function the objects in
 * main/util_script.c would not be linked into a minimal httpd.
 * And the extra prototype is to make gcc -Wmissing-prototypes quiet.
 */
extern void ap_force_library_loading(void);
void ap_force_library_loading(void) {
    ap_add_cgi_vars(NULL);
}

#include "explain.h"

#if !defined(max)
#define max(a,b)        (a > b ? a : b)
#endif

#ifdef WIN32
#include "../os/win32/service.h"
#include "../os/win32/registry.h"
#endif


#ifdef MINT
long _stksize = 32768;
#endif

#ifdef USE_OS2_SCOREBOARD
    /* Add MMAP style functionality to OS/2 */
#define INCL_DOSMEMMGR
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#include <os2.h>
#include <umalloc.h>
#include <stdio.h>
caddr_t create_shared_heap(const char *, size_t);
caddr_t get_shared_heap(const char *);
#endif

DEF_Explain

/* Defining GPROF when compiling uses the moncontrol() function to
 * disable gprof profiling in the parent, and enable it only for
 * request processing in children (or in one_process mode).  It's
 * absolutely required to get useful gprof results under linux
 * because the profile itimers and such are disabled across a
 * fork().  It's probably useful elsewhere as well.
 */
#ifdef GPROF
extern void moncontrol(int);
#define MONCONTROL(x) moncontrol(x)
#else
#define MONCONTROL(x)
#endif

#ifndef MULTITHREAD
/* this just need to be anything non-NULL */
void *ap_dummy_mutex = &ap_dummy_mutex;
#endif

/*
 * Actual definitions of config globals... here because this is
 * for the most part the only code that acts on 'em.  (Hmmm... mod_main.c?)
 */

int ap_standalone;
uid_t ap_user_id;
char *ap_user_name;
gid_t ap_group_id;
#ifdef MULTIPLE_GROUPS
gid_t group_id_list[NGROUPS_MAX];
#endif
int ap_max_requests_per_child;
int ap_threads_per_child;
int ap_excess_requests_per_child;
char *ap_pid_fname;
char *ap_scoreboard_fname;
char *ap_lock_fname;
char *ap_server_argv0;
struct in_addr ap_bind_address;
int ap_daemons_to_start;
int ap_daemons_min_free;
int ap_daemons_max_free;
int ap_daemons_limit;
time_t ap_restart_time;
int ap_suexec_enabled = 0;
int ap_listenbacklog;
int ap_dump_settings;
API_VAR_EXPORT int ap_extended_status = 0;

/*
 * The max child slot ever assigned, preserved across restarts.  Necessary
 * to deal with MaxClients changes across SIGUSR1 restarts.  We use this
 * value to optimize routines that have to scan the entire scoreboard.
 */
static int max_daemons_limit = -1;

/*
 * During config time, listeners is treated as a NULL-terminated list.
 * child_main previously would start at the beginning of the list each time
 * through the loop, so a socket early on in the list could easily starve out
 * sockets later on in the list.  The solution is to start at the listener
 * after the last one processed.  But to do that fast/easily in child_main it's
 * way more convenient for listeners to be a ring that loops back on itself.
 * The routine setup_listeners() is called after config time to both open up
 * the sockets and to turn the NULL-terminated list into a ring that loops back
 * on itself.
 *
 * head_listener is used by each child to keep track of what they consider
 * to be the "start" of the ring.  It is also set by make_child to ensure
 * that new children also don't starve any sockets.
 *
 * Note that listeners != NULL is ensured by read_config().
 */
listen_rec *ap_listeners;
static listen_rec *head_listener;

API_VAR_EXPORT char ap_server_root[MAX_STRING_LEN];
char ap_server_confname[MAX_STRING_LEN];
char ap_coredump_dir[MAX_STRING_LEN];

array_header *ap_server_pre_read_config;
array_header *ap_server_post_read_config;
array_header *ap_server_config_defines;

/* *Non*-shared http_main globals... */

static server_rec *server_conf;
static JMP_BUF APACHE_TLS jmpbuffer;
static int sd;
static fd_set listenfds;
static int listenmaxfd;
static pid_t pgrp;

/* one_process --- debugging mode variable; can be set from the command line
 * with the -X flag.  If set, this gets you the child_main loop running
 * in the process which originally started up (no detach, no make_child),
 * which is a pretty nice debugging environment.  (You'll get a SIGHUP
 * early in standalone_main; just continue through.  This is the server
 * trying to kill off any child processes which it might have lying
 * around --- Apache doesn't keep track of their pids, it just sends
 * SIGHUP to the process group, ignoring it in the root process.
 * Continue through and you'll be fine.).
 */

static int one_process = 0;

/* set if timeouts are to be handled by the children and not by the parent.
 * i.e. child_timeouts = !standalone || one_process.
 */
static int child_timeouts;

#ifdef DEBUG_SIGSTOP
int raise_sigstop_flags;
#endif

#ifndef NO_OTHER_CHILD
/* used to maintain list of children which aren't part of the scoreboard */
typedef struct other_child_rec other_child_rec;
struct other_child_rec {
    other_child_rec *next;
    int pid;
    void (*maintenance) (int, void *, ap_wait_t);
    void *data;
    int write_fd;
};
static other_child_rec *other_children;
#endif

static pool *pconf;		/* Pool for config stuff */
static pool *ptrans;		/* Pool for per-transaction stuff */
static pool *pchild;		/* Pool for httpd child stuff */
static pool *pcommands;	/* Pool for -C and -c switches */

static int APACHE_TLS my_pid;	/* it seems silly to call getpid all the time */
#ifndef MULTITHREAD
static int my_child_num;
#endif

scoreboard *ap_scoreboard_image = NULL;

/*
 * Pieces for managing the contents of the Server response header
 * field.
 */
static char *server_version = NULL;
static int version_locked = 0;

/* Global, alas, so http_core can talk to us */
enum server_token_type ap_server_tokens = SrvTk_FULL;

/*
 * This routine is called when the pconf pool is vacuumed.  It resets the
 * server version string to a known value and [re]enables modifications
 * (which are disabled by configuration completion). 
 */
static void reset_version(void *dummy)
{
    version_locked = 0;
    ap_server_tokens = SrvTk_FULL;
    server_version = NULL;
}

API_EXPORT(const char *) ap_get_server_version(void)
{
    return (server_version ? server_version : SERVER_BASEVERSION);
}

API_EXPORT(void) ap_add_version_component(const char *component)
{
    if (! version_locked) {
        /*
         * If the version string is null, register our cleanup to reset the
         * pointer on pool destruction. We also know that, if NULL,
	 * we are adding the original SERVER_BASEVERSION string.
         */
        if (server_version == NULL) {
	    ap_register_cleanup(pconf, NULL, (void (*)(void *))reset_version, 
				ap_null_cleanup);
	    server_version = ap_pstrdup(pconf, component);
	}
	else {
	    /*
	     * Tack the given component identifier to the end of
	     * the existing string.
	     */
	    server_version = ap_pstrcat(pconf, server_version, " ",
					component, NULL);
	}
    }
}

/*
 * This routine adds the real server base identity to the version string,
 * and then locks out changes until the next reconfig.
 */
static void ap_set_version(void)
{
    if (ap_server_tokens == SrvTk_MIN) {
	ap_add_version_component(SERVER_BASEVERSION);
    }
    else {
	ap_add_version_component(SERVER_BASEVERSION " (" PLATFORM ")");
    }
    /*
     * Lock the server_version string if we're not displaying
     * the full set of tokens
     */
    if (ap_server_tokens != SrvTk_FULL) {
	version_locked++;
    }
}

static APACHE_TLS int volatile exit_after_unblock = 0;

#ifdef GPROF
/* 
 * change directory for gprof to plop the gmon.out file
 * configure in httpd.conf:
 * GprofDir logs/   -> $ServerRoot/logs/gmon.out
 * GprofDir logs/%  -> $ServerRoot/logs/gprof.$pid/gmon.out
 */
static void chdir_for_gprof(void)
{
    core_server_config *sconf = 
	ap_get_module_config(server_conf->module_config, &core_module);    
    char *dir = sconf->gprof_dir;

    if(dir) {
	char buf[512];
	int len = strlen(sconf->gprof_dir) - 1;
	if(*(dir + len) == '%') {
	    dir[len] = '\0';
	    ap_snprintf(buf, sizeof(buf), "%sgprof.%d", dir, (int)getpid());
	} 
	dir = ap_server_root_relative(pconf, buf[0] ? buf : dir);
	if(mkdir(dir, 0755) < 0 && errno != EEXIST) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
			 "gprof: error creating directory %s", dir);
	}
    }
    else {
	dir = ap_server_root_relative(pconf, "logs");
    }

    chdir(dir);
}
#else
#define chdir_for_gprof()
#endif

/* a clean exit from a child with proper cleanup */
static void clean_child_exit(int code) __attribute__ ((noreturn));
static void clean_child_exit(int code)
{
    if (pchild) {
	ap_child_exit_modules(pchild, server_conf);
	ap_destroy_pool(pchild);
    }
    chdir_for_gprof();
    exit(code);
}

#if defined(USE_FCNTL_SERIALIZED_ACCEPT) || defined(USE_FLOCK_SERIALIZED_ACCEPT)
static void expand_lock_fname(pool *p)
{
    /* XXXX possibly bogus cast */
    ap_lock_fname = ap_psprintf(p, "%s.%lu",
	ap_server_root_relative(p, ap_lock_fname), (unsigned long)getpid());
}
#endif

#if defined (USE_USLOCK_SERIALIZED_ACCEPT)

#include <ulocks.h>

static ulock_t uslock = NULL;

#define accept_mutex_child_init(x)

static void accept_mutex_init(pool *p)
{
    ptrdiff_t old;
    usptr_t *us;


    /* default is 8, allocate enough for all the children plus the parent */
    if ((old = usconfig(CONF_INITUSERS, HARD_SERVER_LIMIT + 1)) == -1) {
	perror("usconfig(CONF_INITUSERS)");
	exit(-1);
    }

    if ((old = usconfig(CONF_LOCKTYPE, US_NODEBUG)) == -1) {
	perror("usconfig(CONF_LOCKTYPE)");
	exit(-1);
    }
    if ((old = usconfig(CONF_ARENATYPE, US_SHAREDONLY)) == -1) {
	perror("usconfig(CONF_ARENATYPE)");
	exit(-1);
    }
    if ((us = usinit("/dev/zero")) == NULL) {
	perror("usinit");
	exit(-1);
    }

    if ((uslock = usnewlock(us)) == NULL) {
	perror("usnewlock");
	exit(-1);
    }
}

static void accept_mutex_on(void)
{
    switch (ussetlock(uslock)) {
    case 1:
	/* got lock */
	break;
    case 0:
	fprintf(stderr, "didn't get lock\n");
	clean_child_exit(APEXIT_CHILDFATAL);
    case -1:
	perror("ussetlock");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

static void accept_mutex_off(void)
{
    if (usunsetlock(uslock) == -1) {
	perror("usunsetlock");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

#elif defined (USE_PTHREAD_SERIALIZED_ACCEPT)

/* This code probably only works on Solaris ... but it works really fast
 * on Solaris.  Note that pthread mutexes are *NOT* released when a task
 * dies ... the task has to free it itself.  So we block signals and
 * try to be nice about releasing the mutex.
 */

#include <pthread.h>

static pthread_mutex_t *accept_mutex = (void *)(caddr_t) -1;
static int have_accept_mutex;
static sigset_t accept_block_mask;
static sigset_t accept_previous_mask;

static void accept_mutex_child_cleanup(void *foo)
{
    if (accept_mutex != (void *)(caddr_t)-1
	&& have_accept_mutex) {
	pthread_mutex_unlock(accept_mutex);
    }
}

static void accept_mutex_child_init(pool *p)
{
    ap_register_cleanup(p, NULL, accept_mutex_child_cleanup, ap_null_cleanup);
}

static void accept_mutex_cleanup(void *foo)
{
    if (accept_mutex != (void *)(caddr_t)-1
	&& munmap((caddr_t) accept_mutex, sizeof(*accept_mutex))) {
	perror("munmap");
    }
    accept_mutex = (void *)(caddr_t)-1;
}

static void accept_mutex_init(pool *p)
{
    pthread_mutexattr_t mattr;
    int fd;

    fd = open("/dev/zero", O_RDWR);
    if (fd == -1) {
	perror("open(/dev/zero)");
	exit(APEXIT_INIT);
    }
    accept_mutex = (pthread_mutex_t *) mmap((caddr_t) 0, sizeof(*accept_mutex),
				 PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (accept_mutex == (void *) (caddr_t) - 1) {
	perror("mmap");
	exit(APEXIT_INIT);
    }
    close(fd);
    if ((errno = pthread_mutexattr_init(&mattr))) {
	perror("pthread_mutexattr_init");
	exit(APEXIT_INIT);
    }
    if ((errno = pthread_mutexattr_setpshared(&mattr,
						PTHREAD_PROCESS_SHARED))) {
	perror("pthread_mutexattr_setpshared");
	exit(APEXIT_INIT);
    }
    if ((errno = pthread_mutex_init(accept_mutex, &mattr))) {
	perror("pthread_mutex_init");
	exit(APEXIT_INIT);
    }
    sigfillset(&accept_block_mask);
    sigdelset(&accept_block_mask, SIGHUP);
    sigdelset(&accept_block_mask, SIGTERM);
    sigdelset(&accept_block_mask, SIGUSR1);
    ap_register_cleanup(p, NULL, accept_mutex_cleanup, ap_null_cleanup);
}

static void accept_mutex_on(void)
{
    int err;

    if (sigprocmask(SIG_BLOCK, &accept_block_mask, &accept_previous_mask)) {
	perror("sigprocmask(SIG_BLOCK)");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
    if ((err = pthread_mutex_lock(accept_mutex))) {
	errno = err;
	perror("pthread_mutex_lock");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
    have_accept_mutex = 1;
}

static void accept_mutex_off(void)
{
    int err;

    if ((err = pthread_mutex_unlock(accept_mutex))) {
	errno = err;
	perror("pthread_mutex_unlock");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
    /* There is a slight race condition right here... if we were to die right
     * now, we'd do another pthread_mutex_unlock.  Now, doing that would let
     * another process into the mutex.  pthread mutexes are designed to be
     * fast, as such they don't have protection for things like testing if the
     * thread owning a mutex is actually unlocking it (or even any way of
     * testing who owns the mutex).
     *
     * If we were to unset have_accept_mutex prior to releasing the mutex
     * then the race could result in the server unable to serve hits.  Doing
     * it this way means that the server can continue, but an additional
     * child might be in the critical section ... at least it's still serving
     * hits.
     */
    have_accept_mutex = 0;
    if (sigprocmask(SIG_SETMASK, &accept_previous_mask, NULL)) {
	perror("sigprocmask(SIG_SETMASK)");
	clean_child_exit(1);
    }
}

#elif defined (USE_SYSVSEM_SERIALIZED_ACCEPT)

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#ifdef NEED_UNION_SEMUN
/* it makes no sense, but this isn't defined on solaris */
union semun {
    long val;
    struct semid_ds *buf;
    ushort *array;
};

#endif

static int sem_id = -1;
static struct sembuf op_on;
static struct sembuf op_off;

/* We get a random semaphore ... the lame sysv semaphore interface
 * means we have to be sure to clean this up or else we'll leak
 * semaphores.
 */
static void accept_mutex_cleanup(void *foo)
{
    union semun ick;

    if (sem_id < 0)
	return;
    /* this is ignored anyhow */
    ick.val = 0;
    semctl(sem_id, 0, IPC_RMID, ick);
}

#define accept_mutex_child_init(x)

static void accept_mutex_init(pool *p)
{
    union semun ick;
    struct semid_ds buf;

    /* acquire the semaphore */
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (sem_id < 0) {
	perror("semget");
	exit(APEXIT_INIT);
    }
    ick.val = 1;
    if (semctl(sem_id, 0, SETVAL, ick) < 0) {
	perror("semctl(SETVAL)");
	exit(APEXIT_INIT);
    }
    if (!getuid()) {
	/* restrict it to use only by the appropriate user_id ... not that this
	 * stops CGIs from acquiring it and dinking around with it.
	 */
	buf.sem_perm.uid = ap_user_id;
	buf.sem_perm.gid = ap_group_id;
	buf.sem_perm.mode = 0600;
	ick.buf = &buf;
	if (semctl(sem_id, 0, IPC_SET, ick) < 0) {
	    perror("semctl(IPC_SET)");
	    exit(APEXIT_INIT);
	}
    }
    ap_register_cleanup(p, NULL, accept_mutex_cleanup, ap_null_cleanup);

    /* pre-initialize these */
    op_on.sem_num = 0;
    op_on.sem_op = -1;
    op_on.sem_flg = SEM_UNDO;
    op_off.sem_num = 0;
    op_off.sem_op = 1;
    op_off.sem_flg = SEM_UNDO;
}

static void accept_mutex_on(void)
{
    if (semop(sem_id, &op_on, 1) < 0) {
	perror("accept_mutex_on");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

static void accept_mutex_off(void)
{
    if (semop(sem_id, &op_off, 1) < 0) {
	perror("accept_mutex_off");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

#elif defined(USE_FCNTL_SERIALIZED_ACCEPT)
static struct flock lock_it;
static struct flock unlock_it;

static int lock_fd = -1;

#define accept_mutex_child_init(x)

/*
 * Initialize mutex lock.
 * Must be safe to call this on a restart.
 */
static void accept_mutex_init(pool *p)
{

    lock_it.l_whence = SEEK_SET;	/* from current point */
    lock_it.l_start = 0;		/* -"- */
    lock_it.l_len = 0;			/* until end of file */
    lock_it.l_type = F_WRLCK;		/* set exclusive/write lock */
    lock_it.l_pid = 0;			/* pid not actually interesting */
    unlock_it.l_whence = SEEK_SET;	/* from current point */
    unlock_it.l_start = 0;		/* -"- */
    unlock_it.l_len = 0;		/* until end of file */
    unlock_it.l_type = F_UNLCK;		/* set exclusive/write lock */
    unlock_it.l_pid = 0;		/* pid not actually interesting */

    expand_lock_fname(p);
    lock_fd = ap_popenf(p, ap_lock_fname, O_CREAT | O_WRONLY | O_EXCL, 0644);
    if (lock_fd == -1) {
	perror("open");
	fprintf(stderr, "Cannot open lock file: %s\n", ap_lock_fname);
	exit(APEXIT_INIT);
    }
    unlink(ap_lock_fname);
}

static void accept_mutex_on(void)
{
    int ret;

    while ((ret = fcntl(lock_fd, F_SETLKW, &lock_it)) < 0 && errno == EINTR) {
	/* nop */
    }

    if (ret < 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "fcntl: F_SETLKW: Error getting accept lock, exiting!  "
		    "Perhaps you need to use the LockFile directive to place "
		    "your lock file on a local disk!");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

static void accept_mutex_off(void)
{
    int ret;

    while ((ret = fcntl(lock_fd, F_SETLKW, &unlock_it)) < 0 && errno == EINTR) {
	/* nop */
    }
    if (ret < 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "fcntl: F_SETLKW: Error freeing accept lock, exiting!  "
		    "Perhaps you need to use the LockFile directive to place "
		    "your lock file on a local disk!");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

#elif defined(USE_FLOCK_SERIALIZED_ACCEPT)

static int lock_fd = -1;

static void accept_mutex_cleanup(void *foo)
{
    unlink(ap_lock_fname);
}

/*
 * Initialize mutex lock.
 * Done by each child at it's birth
 */
static void accept_mutex_child_init(pool *p)
{

    lock_fd = ap_popenf(p, ap_lock_fname, O_WRONLY, 0600);
    if (lock_fd == -1) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "Child cannot open lock file: %s", ap_lock_fname);
	clean_child_exit(APEXIT_CHILDINIT);
    }
}

/*
 * Initialize mutex lock.
 * Must be safe to call this on a restart.
 */
static void accept_mutex_init(pool *p)
{
    expand_lock_fname(p);
    unlink(ap_lock_fname);
    lock_fd = ap_popenf(p, ap_lock_fname, O_CREAT | O_WRONLY | O_EXCL, 0600);
    if (lock_fd == -1) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "Parent cannot open lock file: %s", ap_lock_fname);
	exit(APEXIT_INIT);
    }
    ap_register_cleanup(p, NULL, accept_mutex_cleanup, ap_null_cleanup);
}

static void accept_mutex_on(void)
{
    int ret;

    while ((ret = flock(lock_fd, LOCK_EX)) < 0 && errno == EINTR)
	continue;

    if (ret < 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "flock: LOCK_EX: Error getting accept lock. Exiting!");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

static void accept_mutex_off(void)
{
    if (flock(lock_fd, LOCK_UN) < 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "flock: LOCK_UN: Error freeing accept lock. Exiting!");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

#elif defined(USE_OS2SEM_SERIALIZED_ACCEPT)

static HMTX lock_sem = -1;

static void accept_mutex_cleanup(void *foo)
{
    DosReleaseMutexSem(lock_sem);
    DosCloseMutexSem(lock_sem);
}

/*
 * Initialize mutex lock.
 * Done by each child at it's birth
 */
static void accept_mutex_child_init(pool *p)
{
    int rc = DosOpenMutexSem(NULL, &lock_sem);

    if (rc != 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "Child cannot open lock semaphore");
	clean_child_exit(APEXIT_CHILDINIT);
    }
}

/*
 * Initialize mutex lock.
 * Must be safe to call this on a restart.
 */
static void accept_mutex_init(pool *p)
{
    int rc = DosCreateMutexSem(NULL, &lock_sem, DC_SEM_SHARED, FALSE);

    if (rc != 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "Parent cannot create lock semaphore");
	exit(APEXIT_INIT);
    }

    ap_register_cleanup(p, NULL, accept_mutex_cleanup, ap_null_cleanup);
}

static void accept_mutex_on(void)
{
    int rc = DosRequestMutexSem(lock_sem, SEM_INDEFINITE_WAIT);

    if (rc != 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "OS2SEM: Error %d getting accept lock. Exiting!", rc);
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

static void accept_mutex_off(void)
{
    int rc = DosReleaseMutexSem(lock_sem);
    
    if (rc != 0) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "OS2SEM: Error %d freeing accept lock. Exiting!", rc);
	clean_child_exit(APEXIT_CHILDFATAL);
    }
}

#else
/* Default --- no serialization.  Other methods *could* go here,
 * as #elifs...
 */
#if !defined(MULTITHREAD)
/* Multithreaded systems don't complete between processes for
 * the sockets. */
#define NO_SERIALIZED_ACCEPT
#define accept_mutex_child_init(x)
#define accept_mutex_init(x)
#define accept_mutex_on()
#define accept_mutex_off()
#endif
#endif

/* On some architectures it's safe to do unserialized accept()s in the single
 * Listen case.  But it's never safe to do it in the case where there's
 * multiple Listen statements.  Define SINGLE_LISTEN_UNSERIALIZED_ACCEPT
 * when it's safe in the single Listen case.
 */
#ifdef SINGLE_LISTEN_UNSERIALIZED_ACCEPT
#define SAFE_ACCEPT(stmt) do {if(ap_listeners->next != ap_listeners) {stmt;}} while(0)
#else
#define SAFE_ACCEPT(stmt) do {stmt;} while(0)
#endif

static void usage(char *bin)
{
    char pad[MAX_STRING_LEN];
    unsigned i;

    for (i = 0; i < strlen(bin); i++)
	pad[i] = ' ';
    pad[i] = '\0';
#ifdef SHARED_CORE
    fprintf(stderr, "Usage: %s [-L directory] [-d directory] [-f file]\n", bin);
#else
    fprintf(stderr, "Usage: %s [-d directory] [-f file]\n", bin);
#endif
    fprintf(stderr, "       %s [-C \"directive\"] [-c \"directive\"]\n", pad);
    fprintf(stderr, "       %s [-v] [-V] [-h] [-l] [-S] [-t]\n", pad);
    fprintf(stderr, "Options:\n");
#ifdef SHARED_CORE
    fprintf(stderr, "  -L directory     : specify an alternate location for shared object files\n");
#endif
    fprintf(stderr, "  -D name          : define a name for use in <IfDefine name> directives\n");
    fprintf(stderr, "  -d directory     : specify an alternate initial ServerRoot\n");
    fprintf(stderr, "  -f file          : specify an alternate ServerConfigFile\n");
    fprintf(stderr, "  -C \"directive\"   : process directive before reading config files\n");
    fprintf(stderr, "  -c \"directive\"   : process directive after  reading config files\n");
    fprintf(stderr, "  -v               : show version number\n");
    fprintf(stderr, "  -V               : show compile settings\n");
    fprintf(stderr, "  -h               : list available configuration directives\n");
    fprintf(stderr, "  -l               : list compiled-in modules\n");
    fprintf(stderr, "  -S               : show parsed settings (currently only vhost settings)\n");
    fprintf(stderr, "  -t               : run syntax test for configuration files only\n");
    exit(1);
}

/*****************************************************************
 *
 * Timeout handling.  DISTINCTLY not thread-safe, but all this stuff
 * has to change for threads anyway.  Note that this code allows only
 * one timeout in progress at a time...
 */

static APACHE_TLS conn_rec *volatile current_conn;
static APACHE_TLS request_rec *volatile timeout_req;
static APACHE_TLS const char *volatile timeout_name = NULL;
static APACHE_TLS int volatile alarms_blocked = 0;
static APACHE_TLS int volatile alarm_pending = 0;

static void timeout(int sig)
{				/* Also called on SIGPIPE */
    void *dirconf;

    signal(SIGPIPE, SIG_IGN);	/* Block SIGPIPE */
    if (alarms_blocked) {
	alarm_pending = 1;
	return;
    }
    if (exit_after_unblock) {
	clean_child_exit(0);
    }

    if (!current_conn) {
	ap_longjmp(jmpbuffer, 1);
    }

    if (timeout_req != NULL)
	dirconf = timeout_req->per_dir_config;
    else
	dirconf = current_conn->server->lookup_defaults;
    if (!current_conn->keptalive) {
	if (sig == SIGPIPE) {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO,
			current_conn->server,
			"(client %s) stopped connection before %s completed",
			current_conn->remote_ip,
			timeout_name ? timeout_name : "request");
	}
	else {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO,
			current_conn->server,
			"(client %s) %s timed out",
			current_conn->remote_ip,
			timeout_name ? timeout_name : "request");
	}
    }

    if (timeout_req) {
	/* Someone has asked for this transaction to just be aborted
	 * if it times out...
	 */

	request_rec *log_req = timeout_req;
	request_rec *save_req = timeout_req;

	/* avoid looping... if ap_log_transaction started another
	 * timer (say via rfc1413.c) we could loop...
	 */
	timeout_req = NULL;

	while (log_req->main || log_req->prev) {
	    /* Get back to original request... */
	    if (log_req->main)
		log_req = log_req->main;
	    else
		log_req = log_req->prev;
	}

	if (!current_conn->keptalive)
	    ap_log_transaction(log_req);

	ap_bsetflag(save_req->connection->client, B_EOUT, 1);
	ap_bclose(save_req->connection->client);

	if (!ap_standalone)
	    exit(0);

	ap_longjmp(jmpbuffer, 1);
    }
    else {			/* abort the connection */
	ap_bsetflag(current_conn->client, B_EOUT, 1);
	ap_bclose(current_conn->client);
	current_conn->aborted = 1;
    }
}

/*
 * These two called from alloc.c to protect its critical sections...
 * Note that they can nest (as when destroying the sub_pools of a pool
 * which is itself being cleared); we have to support that here.
 */

API_EXPORT(void) ap_block_alarms(void)
{
    ++alarms_blocked;
}

API_EXPORT(void) ap_unblock_alarms(void)
{
    --alarms_blocked;
    if (alarms_blocked == 0) {
	if (exit_after_unblock) {
	    /* We have a couple race conditions to deal with here, we can't
	     * allow a timeout that comes in this small interval to allow
	     * the child to jump back to the main loop.  Instead we block
	     * alarms again, and then note that exit_after_unblock is
	     * being dealt with.  We choose this way to solve this so that
	     * the common path through unblock_alarms() is really short.
	     */
	    ++alarms_blocked;
	    exit_after_unblock = 0;
	    clean_child_exit(0);
	}
	if (alarm_pending) {
	    alarm_pending = 0;
	    timeout(0);
	}
    }
}


static APACHE_TLS void (*volatile alarm_fn) (int) = NULL;
#ifdef WIN32
static APACHE_TLS unsigned int alarm_expiry_time = 0;
#endif /* WIN32 */

#ifndef WIN32
static void alrm_handler(int sig)
{
    if (alarm_fn) {
	(*alarm_fn) (sig);
    }
}
#endif

unsigned int ap_set_callback_and_alarm(void (*fn) (int), int x)
{
    unsigned int old;

#ifdef WIN32
    old = alarm_expiry_time;
    if (old)
	old -= time(0);
    if (x == 0) {
	alarm_fn = NULL;
	alarm_expiry_time = 0;
    }
    else {
	alarm_fn = fn;
	alarm_expiry_time = time(NULL) + x;
    }
#else
    if (alarm_fn && x && fn != alarm_fn) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_DEBUG, NULL,
	    "ap_set_callback_and_alarm: possible nested timer!");
    }
    alarm_fn = fn;
#ifndef OPTIMIZE_TIMEOUTS
    old = alarm(x);
#else
    if (child_timeouts) {
	old = alarm(x);
    }
    else {
	/* Just note the timeout in our scoreboard, no need to call the system.
	 * We also note that the virtual time has gone forward.
	 */
	old = ap_scoreboard_image->servers[my_child_num].timeout_len;
	ap_scoreboard_image->servers[my_child_num].timeout_len = x;
	++ap_scoreboard_image->servers[my_child_num].cur_vtime;
    }
#endif
#endif
    return (old);
}


#ifdef WIN32
API_EXPORT(int) ap_check_alarm(void)
{
    if (alarm_expiry_time) {
	unsigned int t;

	t = time(NULL);
	if (t >= alarm_expiry_time) {
	    alarm_expiry_time = 0;
	    (*alarm_fn) (0);
	    return (-1);
	}
	else {
	    return (alarm_expiry_time - t);
	}
    }
    else
	return (0);
}
#endif /* WIN32 */



/* reset_timeout (request_rec *) resets the timeout in effect,
 * as long as it hasn't expired already.
 */

API_EXPORT(void) ap_reset_timeout(request_rec *r)
{
    int i;

    if (timeout_name) {		/* timeout has been set */
	i = ap_set_callback_and_alarm(alarm_fn, r->server->timeout);
	if (i == 0)		/* timeout already expired, so set it back to 0 */
	    ap_set_callback_and_alarm(alarm_fn, 0);
    }
}




void ap_keepalive_timeout(char *name, request_rec *r)
{
    unsigned int to;

    timeout_req = r;
    timeout_name = name;

    if (r->connection->keptalive)
	to = r->server->keep_alive_timeout;
    else
	to = r->server->timeout;
    ap_set_callback_and_alarm(timeout, to);

}

API_EXPORT(void) ap_hard_timeout(char *name, request_rec *r)
{
    timeout_req = r;
    timeout_name = name;

    ap_set_callback_and_alarm(timeout, r->server->timeout);

}

API_EXPORT(void) ap_soft_timeout(char *name, request_rec *r)
{
    timeout_name = name;

    ap_set_callback_and_alarm(timeout, r->server->timeout);

}

API_EXPORT(void) ap_kill_timeout(request_rec *dummy)
{
    ap_set_callback_and_alarm(NULL, 0);
    timeout_req = NULL;
    timeout_name = NULL;
}


/*
 * More machine-dependent networking gooo... on some systems,
 * you've got to be *really* sure that all the packets are acknowledged
 * before closing the connection, since the client will not be able
 * to see the last response if their TCP buffer is flushed by a RST
 * packet from us, which is what the server's TCP stack will send
 * if it receives any request data after closing the connection.
 *
 * In an ideal world, this function would be accomplished by simply
 * setting the socket option SO_LINGER and handling it within the
 * server's TCP stack while the process continues on to the next request.
 * Unfortunately, it seems that most (if not all) operating systems
 * block the server process on close() when SO_LINGER is used.
 * For those that don't, see USE_SO_LINGER below.  For the rest,
 * we have created a home-brew lingering_close.
 *
 * Many operating systems tend to block, puke, or otherwise mishandle
 * calls to shutdown only half of the connection.  You should define
 * NO_LINGCLOSE in ap_config.h if such is the case for your system.
 */
#ifndef MAX_SECS_TO_LINGER
#define MAX_SECS_TO_LINGER 30
#endif

#ifdef USE_SO_LINGER
#define NO_LINGCLOSE		/* The two lingering options are exclusive */

static void sock_enable_linger(int s)
{
    struct linger li;

    li.l_onoff = 1;
    li.l_linger = MAX_SECS_TO_LINGER;

    if (setsockopt(s, SOL_SOCKET, SO_LINGER,
		   (char *) &li, sizeof(struct linger)) < 0) {
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf,
	            "setsockopt: (SO_LINGER)");
	/* not a fatal error */
    }
}

#else
#define sock_enable_linger(s)	/* NOOP */
#endif /* USE_SO_LINGER */

#ifndef NO_LINGCLOSE

/* Special version of timeout for lingering_close */

static void lingerout(int sig)
{
    if (alarms_blocked) {
	alarm_pending = 1;
	return;
    }

    if (!current_conn) {
	ap_longjmp(jmpbuffer, 1);
    }
    ap_bsetflag(current_conn->client, B_EOUT, 1);
    current_conn->aborted = 1;
}

static void linger_timeout(void)
{
    timeout_name = "lingering close";

    ap_set_callback_and_alarm(lingerout, MAX_SECS_TO_LINGER);
}

/* Since many clients will abort a connection instead of closing it,
 * attempting to log an error message from this routine will only
 * confuse the webmaster.  There doesn't seem to be any portable way to
 * distinguish between a dropped connection and something that might be
 * worth logging.
 */
static void lingering_close(request_rec *r)
{
    char dummybuf[512];
    struct timeval tv;
    fd_set lfds;
    int select_rv;
    int lsd;

    /* Prevent a slow-drip client from holding us here indefinitely */

    linger_timeout();

    /* Send any leftover data to the client, but never try to again */

    if (ap_bflush(r->connection->client) == -1) {
	ap_kill_timeout(r);
	ap_bclose(r->connection->client);
	return;
    }
    ap_bsetflag(r->connection->client, B_EOUT, 1);

    /* Close our half of the connection --- send the client a FIN */

    lsd = r->connection->client->fd;

    if ((shutdown(lsd, 1) != 0) || r->connection->aborted) {
	ap_kill_timeout(r);
	ap_bclose(r->connection->client);
	return;
    }

    /* Set up to wait for readable data on socket... */

    FD_ZERO(&lfds);

    /* Wait for readable data or error condition on socket;
     * slurp up any data that arrives...  We exit when we go for an
     * interval of tv length without getting any more data, get an error
     * from select(), get an error or EOF on a read, or the timer expires.
     */

    do {
	/* We use a 2 second timeout because current (Feb 97) browsers
	 * fail to close a connection after the server closes it.  Thus,
	 * to avoid keeping the child busy, we are only lingering long enough
	 * for a client that is actively sending data on a connection.
	 * This should be sufficient unless the connection is massively
	 * losing packets, in which case we might have missed the RST anyway.
	 * These parameters are reset on each pass, since they might be
	 * changed by select.
	 */
	FD_SET(lsd, &lfds);
	tv.tv_sec = 2;
	tv.tv_usec = 0;

	select_rv = ap_select(lsd + 1, &lfds, NULL, NULL, &tv);

    } while ((select_rv > 0) &&
             (read(lsd, dummybuf, sizeof dummybuf) > 0));

    /* Should now have seen final ack.  Safe to finally kill socket */

    ap_bclose(r->connection->client);

    ap_kill_timeout(r);
}
#endif /* ndef NO_LINGCLOSE */

/*****************************************************************
 * dealing with other children
 */

#ifndef NO_OTHER_CHILD
API_EXPORT(void) ap_register_other_child(int pid,
		       void (*maintenance) (int reason, void *, ap_wait_t status),
			  void *data, int write_fd)
{
    other_child_rec *ocr;

    ocr = ap_palloc(pconf, sizeof(*ocr));
    ocr->pid = pid;
    ocr->maintenance = maintenance;
    ocr->data = data;
    ocr->write_fd = write_fd;
    ocr->next = other_children;
    other_children = ocr;
}

/* note that since this can be called by a maintenance function while we're
 * scanning the other_children list, all scanners should protect themself
 * by loading ocr->next before calling any maintenance function.
 */
API_EXPORT(void) ap_unregister_other_child(void *data)
{
    other_child_rec **pocr, *nocr;

    for (pocr = &other_children; *pocr; pocr = &(*pocr)->next) {
	if ((*pocr)->data == data) {
	    nocr = (*pocr)->next;
	    (*(*pocr)->maintenance) (OC_REASON_UNREGISTER, (*pocr)->data, -1);
	    *pocr = nocr;
	    /* XXX: um, well we've just wasted some space in pconf ? */
	    return;
	}
    }
}

/* test to ensure that the write_fds are all still writable, otherwise
 * invoke the maintenance functions as appropriate */
static void probe_writable_fds(void)
{
    fd_set writable_fds;
    int fd_max;
    other_child_rec *ocr, *nocr;
    struct timeval tv;
    int rc;

    if (other_children == NULL)
	return;

    fd_max = 0;
    FD_ZERO(&writable_fds);
    do {
	for (ocr = other_children; ocr; ocr = ocr->next) {
	    if (ocr->write_fd == -1)
		continue;
	    FD_SET(ocr->write_fd, &writable_fds);
	    if (ocr->write_fd > fd_max) {
		fd_max = ocr->write_fd;
	    }
	}
	if (fd_max == 0)
	    return;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	rc = ap_select(fd_max + 1, NULL, &writable_fds, NULL, &tv);
    } while (rc == -1 && errno == EINTR);

    if (rc == -1) {
	/* XXX: uhh this could be really bad, we could have a bad file
	 * descriptor due to a bug in one of the maintenance routines */
	ap_log_unixerr("probe_writable_fds", "select",
		    "could not probe writable fds", server_conf);
	return;
    }
    if (rc == 0)
	return;

    for (ocr = other_children; ocr; ocr = nocr) {
	nocr = ocr->next;
	if (ocr->write_fd == -1)
	    continue;
	if (FD_ISSET(ocr->write_fd, &writable_fds))
	    continue;
	(*ocr->maintenance) (OC_REASON_UNWRITABLE, ocr->data, -1);
    }
}

/* possibly reap an other_child, return 0 if yes, -1 if not */
static int reap_other_child(int pid, ap_wait_t status)
{
    other_child_rec *ocr, *nocr;

    for (ocr = other_children; ocr; ocr = nocr) {
	nocr = ocr->next;
	if (ocr->pid != pid)
	    continue;
	ocr->pid = -1;
	(*ocr->maintenance) (OC_REASON_DEATH, ocr->data, status);
	return 0;
    }
    return -1;
}
#endif

/*****************************************************************
 *
 * Dealing with the scoreboard... a lot of these variables are global
 * only to avoid getting clobbered by the longjmp() that happens when
 * a hard timeout expires...
 *
 * We begin with routines which deal with the file itself... 
 */

#ifdef MULTITHREAD
/*
 * In the multithreaded mode, have multiple threads - not multiple
 * processes that need to talk to each other. Just use a simple
 * malloc. But let the routines that follow, think that you have
 * shared memory (so they use memcpy etc.)
 */

static void reinit_scoreboard(pool *p)
{
    ap_assert(!ap_scoreboard_image);
    ap_scoreboard_image = (scoreboard *) malloc(SCOREBOARD_SIZE);
    memset(ap_scoreboard_image, 0, SCOREBOARD_SIZE);
}

void cleanup_scoreboard(void)
{
    ap_assert(ap_scoreboard_image);
    free(ap_scoreboard_image);
    ap_scoreboard_image = NULL;
}

API_EXPORT(void) ap_sync_scoreboard_image(void)
{
}


#else /* MULTITHREAD */
#if defined(USE_OS2_SCOREBOARD)

/* The next two routines are used to access shared memory under OS/2.  */
/* This requires EMX v09c to be installed.                           */

caddr_t create_shared_heap(const char *name, size_t size)
{
    ULONG rc;
    void *mem;
    Heap_t h;

    rc = DosAllocSharedMem(&mem, name, size,
			   PAG_COMMIT | PAG_READ | PAG_WRITE);
    if (rc != 0)
	return NULL;
    h = _ucreate(mem, size, !_BLOCK_CLEAN, _HEAP_REGULAR | _HEAP_SHARED,
		 NULL, NULL);
    if (h == NULL)
	DosFreeMem(mem);
    return (caddr_t) h;
}

caddr_t get_shared_heap(const char *Name)
{

    PVOID BaseAddress;		/* Pointer to the base address of
				   the shared memory object */
    ULONG AttributeFlags;	/* Flags describing characteristics
				   of the shared memory object */
    APIRET rc;			/* Return code */

    /* Request read and write access to */
    /*   the shared memory object       */
    AttributeFlags = PAG_WRITE | PAG_READ;

    rc = DosGetNamedSharedMem(&BaseAddress, Name, AttributeFlags);

    if (rc != 0) {
	printf("DosGetNamedSharedMem error: return code = %ld", rc);
	return 0;
    }

    return BaseAddress;
}

static void setup_shared_mem(pool *p)
{
    caddr_t m;

    int rc;

    m = (caddr_t) create_shared_heap("\\SHAREMEM\\SCOREBOARD", SCOREBOARD_SIZE);
    if (m == 0) {
	fprintf(stderr, "httpd: Could not create OS/2 Shared memory pool.\n");
	exit(APEXIT_INIT);
    }

    rc = _uopen((Heap_t) m);
    if (rc != 0) {
	fprintf(stderr, "httpd: Could not uopen() newly created OS/2 Shared memory pool.\n");
    }
    ap_scoreboard_image = (scoreboard *) m;
    ap_scoreboard_image->global.exit_generation = 0;
}

static void reopen_scoreboard(pool *p)
{
    caddr_t m;
    int rc;

    m = (caddr_t) get_shared_heap("\\SHAREMEM\\SCOREBOARD");
    if (m == 0) {
	fprintf(stderr, "httpd: Could not find existing OS/2 Shared memory pool.\n");
	exit(APEXIT_INIT);
    }

    rc = _uopen((Heap_t) m);
    ap_scoreboard_image = (scoreboard *) m;
}

#elif defined(USE_POSIX_SCOREBOARD)
#include <sys/mman.h>
/* 
 * POSIX 1003.4 style
 *
 * Note 1: 
 * As of version 4.23A, shared memory in QNX must reside under /dev/shmem,
 * where no subdirectories allowed.
 *
 * POSIX shm_open() and shm_unlink() will take care about this issue,
 * but to avoid confusion, I suggest to redefine scoreboard file name
 * in httpd.conf to cut "logs/" from it. With default setup actual name
 * will be "/dev/shmem/logs.apache_status". 
 * 
 * If something went wrong and Apache did not unlinked this object upon
 * exit, you can remove it manually, using "rm -f" command.
 * 
 * Note 2:
 * <sys/mman.h> in QNX defines MAP_ANON, but current implementation 
 * does NOT support BSD style anonymous mapping. So, the order of 
 * conditional compilation is important: 
 * this #ifdef section must be ABOVE the next one (BSD style).
 *
 * I tested this stuff and it works fine for me, but if it provides 
 * trouble for you, just comment out USE_MMAP_SCOREBOARD in QNX section
 * of ap_config.h
 *
 * June 5, 1997, 
 * Igor N. Kovalenko -- infoh@mail.wplus.net
 */

static void cleanup_shared_mem(void *d)
{
    shm_unlink(ap_scoreboard_fname);
}

static void setup_shared_mem(pool *p)
{
    caddr_t m;
    int fd;

    fd = shm_open(ap_scoreboard_fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
	perror("httpd: could not open(create) scoreboard");
	exit(APEXIT_INIT);
    }
    if (ltrunc(fd, (off_t) SCOREBOARD_SIZE, SEEK_SET) == -1) {
	perror("httpd: could not ltrunc scoreboard");
	shm_unlink(ap_scoreboard_fname);
	exit(APEXIT_INIT);
    }
    if ((m = (caddr_t) mmap((caddr_t) 0,
			    (size_t) SCOREBOARD_SIZE, PROT_READ | PROT_WRITE,
			    MAP_SHARED, fd, (off_t) 0)) == (caddr_t) - 1) {
	perror("httpd: cannot mmap scoreboard");
	shm_unlink(ap_scoreboard_fname);
	exit(APEXIT_INIT);
    }
    close(fd);
    ap_register_cleanup(p, NULL, cleanup_shared_mem, ap_null_cleanup);
    ap_scoreboard_image = (scoreboard *) m;
    ap_scoreboard_image->global.exit_generation = 0;
}

static void reopen_scoreboard(pool *p)
{
}

#elif defined(USE_MMAP_SCOREBOARD)

static void setup_shared_mem(pool *p)
{
    caddr_t m;

#if defined(MAP_ANON)
/* BSD style */
#ifdef CONVEXOS11
    /*
     * 9-Aug-97 - Jeff Venters (venters@convex.hp.com)
     * ConvexOS maps address space as follows:
     *   0x00000000 - 0x7fffffff : Kernel
     *   0x80000000 - 0xffffffff : User
     * Start mmapped area 1GB above start of text.
     *
     * Also, the length requires a pointer as the actual length is
     * returned (rounded up to a page boundary).
     */
    {
	unsigned len = SCOREBOARD_SIZE;

	m = mmap((caddr_t) 0xC0000000, &len,
		 PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, NOFD, 0);
    }
#elif defined(MAP_TMPFILE)
    {
	char mfile[] = "/tmp/apache_shmem_XXXX";
	int fd = mkstemp(mfile);
	if (fd == -1) {
	    perror("open");
	    fprintf(stderr, "httpd: Could not open %s\n", mfile);
	    exit(APEXIT_INIT);
	}
	m = mmap((caddr_t) 0, SCOREBOARD_SIZE,
		PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (m == (caddr_t) - 1) {
	    perror("mmap");
	    fprintf(stderr, "httpd: Could not mmap %s\n", mfile);
	    exit(APEXIT_INIT);
	}
	close(fd);
	unlink(mfile);
    }
#else
    m = mmap((caddr_t) 0, SCOREBOARD_SIZE,
	     PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);
#endif
    if (m == (caddr_t) - 1) {
	perror("mmap");
	fprintf(stderr, "httpd: Could not mmap memory\n");
	exit(APEXIT_INIT);
    }
#else
/* Sun style */
    int fd;

    fd = open("/dev/zero", O_RDWR);
    if (fd == -1) {
	perror("open");
	fprintf(stderr, "httpd: Could not open /dev/zero\n");
	exit(APEXIT_INIT);
    }
    m = mmap((caddr_t) 0, SCOREBOARD_SIZE,
	     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (m == (caddr_t) - 1) {
	perror("mmap");
	fprintf(stderr, "httpd: Could not mmap /dev/zero\n");
	exit(APEXIT_INIT);
    }
    close(fd);
#endif
    ap_scoreboard_image = (scoreboard *) m;
    ap_scoreboard_image->global.exit_generation = 0;
}

static void reopen_scoreboard(pool *p)
{
}

#elif defined(USE_SHMGET_SCOREBOARD)
static key_t shmkey = IPC_PRIVATE;
static int shmid = -1;

static void setup_shared_mem(pool *p)
{
    struct shmid_ds shmbuf;
#ifdef MOVEBREAK
    char *obrk;
#endif

    if ((shmid = shmget(shmkey, SCOREBOARD_SIZE, IPC_CREAT | SHM_R | SHM_W)) == -1) {
#ifdef LINUX
	if (errno == ENOSYS) {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_EMERG, server_conf,
		    "httpd: Your kernel was built without CONFIG_SYSVIPC\n"
		    "httpd: please consult the Apache FAQ for details");
	}
#endif
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf,
		    "could not call shmget");
	exit(APEXIT_INIT);
    }

    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, server_conf,
		"created shared memory segment #%d", shmid);

#ifdef MOVEBREAK
    /*
     * Some SysV systems place the shared segment WAY too close
     * to the dynamic memory break point (sbrk(0)). This severely
     * limits the use of malloc/sbrk in the program since sbrk will
     * refuse to move past that point.
     *
     * To get around this, we move the break point "way up there",
     * attach the segment and then move break back down. Ugly
     */
    if ((obrk = sbrk(MOVEBREAK)) == (char *) -1) {
	ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
	    "sbrk() could not move break");
    }
#endif

#define BADSHMAT	((scoreboard *)(-1))
    if ((ap_scoreboard_image = (scoreboard *) shmat(shmid, 0, 0)) == BADSHMAT) {
	ap_log_error(APLOG_MARK, APLOG_EMERG, server_conf, "shmat error");
	/*
	 * We exit below, after we try to remove the segment
	 */
    }
    else {			/* only worry about permissions if we attached the segment */
	if (shmctl(shmid, IPC_STAT, &shmbuf) != 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
		"shmctl() could not stat segment #%d", shmid);
	}
	else {
	    shmbuf.shm_perm.uid = ap_user_id;
	    shmbuf.shm_perm.gid = ap_group_id;
	    if (shmctl(shmid, IPC_SET, &shmbuf) != 0) {
		ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
		    "shmctl() could not set segment #%d", shmid);
	    }
	}
    }
    /*
     * We must avoid leaving segments in the kernel's
     * (small) tables.
     */
    if (shmctl(shmid, IPC_RMID, NULL) != 0) {
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf,
		"shmctl: IPC_RMID: could not remove shared memory segment #%d",
		shmid);
    }
    if (ap_scoreboard_image == BADSHMAT)	/* now bailout */
	exit(APEXIT_INIT);

#ifdef MOVEBREAK
    if (obrk == (char *) -1)
	return;			/* nothing else to do */
    if (sbrk(-(MOVEBREAK)) == (char *) -1) {
	ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
	    "sbrk() could not move break back");
    }
#endif
    ap_scoreboard_image->global.exit_generation = 0;
}

static void reopen_scoreboard(pool *p)
{
}

#else
#define SCOREBOARD_FILE
static scoreboard _scoreboard_image;
static int scoreboard_fd = -1;

/* XXX: things are seriously screwed if we ever have to do a partial
 * read or write ... we could get a corrupted scoreboard
 */
static int force_write(int fd, void *buffer, int bufsz)
{
    int rv, orig_sz = bufsz;

    do {
	rv = write(fd, buffer, bufsz);
	if (rv > 0) {
	    buffer = (char *) buffer + rv;
	    bufsz -= rv;
	}
    } while ((rv > 0 && bufsz > 0) || (rv == -1 && errno == EINTR));

    return rv < 0 ? rv : orig_sz - bufsz;
}

static int force_read(int fd, void *buffer, int bufsz)
{
    int rv, orig_sz = bufsz;

    do {
	rv = read(fd, buffer, bufsz);
	if (rv > 0) {
	    buffer = (char *) buffer + rv;
	    bufsz -= rv;
	}
    } while ((rv > 0 && bufsz > 0) || (rv == -1 && errno == EINTR));

    return rv < 0 ? rv : orig_sz - bufsz;
}

static void cleanup_scoreboard_file(void *foo)
{
    unlink(ap_scoreboard_fname);
}

void reopen_scoreboard(pool *p)
{
    if (scoreboard_fd != -1)
	ap_pclosef(p, scoreboard_fd);

    scoreboard_fd = ap_popenf(p, ap_scoreboard_fname, O_CREAT | O_BINARY | O_RDWR, 0666);
    if (scoreboard_fd == -1) {
	perror(ap_scoreboard_fname);
	fprintf(stderr, "Cannot open scoreboard file:\n");
	clean_child_exit(1);
    }
}
#endif

/* Called by parent process */
static void reinit_scoreboard(pool *p)
{
    int exit_gen = 0;
    if (ap_scoreboard_image)
	exit_gen = ap_scoreboard_image->global.exit_generation;

#ifndef SCOREBOARD_FILE
    if (ap_scoreboard_image == NULL) {
	setup_shared_mem(p);
    }
    memset(ap_scoreboard_image, 0, SCOREBOARD_SIZE);
    ap_scoreboard_image->global.exit_generation = exit_gen;
#else
    ap_scoreboard_image = &_scoreboard_image;
    ap_scoreboard_fname = ap_server_root_relative(p, ap_scoreboard_fname);

    scoreboard_fd = ap_popenf(p, ap_scoreboard_fname, O_CREAT | O_BINARY | O_RDWR, 0644);
    if (scoreboard_fd == -1) {
	perror(ap_scoreboard_fname);
	fprintf(stderr, "Cannot open scoreboard file:\n");
	exit(APEXIT_INIT);
    }
    ap_register_cleanup(p, NULL, cleanup_scoreboard_file, ap_null_cleanup);

    memset((char *) ap_scoreboard_image, 0, sizeof(*ap_scoreboard_image));
    ap_scoreboard_image->global.exit_generation = exit_gen;
    force_write(scoreboard_fd, ap_scoreboard_image, sizeof(*ap_scoreboard_image));
#endif
}

/* Routines called to deal with the scoreboard image
 * --- note that we do *not* need write locks, since update_child_status
 * only updates a *single* record in place, and only one process writes to
 * a given scoreboard slot at a time (either the child process owning that
 * slot, or the parent, noting that the child has died).
 *
 * As a final note --- setting the score entry to getpid() is always safe,
 * since when the parent is writing an entry, it's only noting SERVER_DEAD
 * anyway.
 */

ap_inline void ap_sync_scoreboard_image(void)
{
#ifdef SCOREBOARD_FILE
    lseek(scoreboard_fd, 0L, 0);
    force_read(scoreboard_fd, ap_scoreboard_image, sizeof(*ap_scoreboard_image));
#endif
}

#endif /* MULTITHREAD */

API_EXPORT(int) ap_exists_scoreboard_image(void)
{
    return (ap_scoreboard_image ? 1 : 0);
}

static ap_inline void put_scoreboard_info(int child_num,
				       short_score *new_score_rec)
{
#ifdef SCOREBOARD_FILE
    lseek(scoreboard_fd, (long) child_num * sizeof(short_score), 0);
    force_write(scoreboard_fd, new_score_rec, sizeof(short_score));
#endif
}

/* a clean exit from the parent with proper cleanup */
static void clean_parent_exit(int code) __attribute__((noreturn));
static void clean_parent_exit(int code)
{
    /* Clear the pool - including any registered cleanups */
    ap_destroy_pool(pconf);
    exit(code);
}

int ap_update_child_status(int child_num, int status, request_rec *r)
{
    int old_status;
    short_score *ss;

    if (child_num < 0)
	return -1;

    ap_sync_scoreboard_image();
    ss = &ap_scoreboard_image->servers[child_num];
    old_status = ss->status;
    ss->status = status;
#ifdef OPTIMIZE_TIMEOUTS
    ++ss->cur_vtime;
#endif

    if (ap_extended_status) {
#ifndef OPTIMIZE_TIMEOUTS
	ss->last_used = time(NULL);
#endif
	if (status == SERVER_READY || status == SERVER_DEAD) {
	    /*
	     * Reset individual counters
	     */
	    if (status == SERVER_DEAD) {
		ss->my_access_count = 0L;
		ss->my_bytes_served = 0L;
	    }
	    ss->conn_count = (unsigned short) 0;
	    ss->conn_bytes = (unsigned long) 0;
	}
	if (r) {
	    conn_rec *c = r->connection;
	    ap_cpystrn(ss->client, ap_get_remote_host(c, r->per_dir_config,
				  REMOTE_NOLOOKUP), sizeof(ss->client));
	    if (r->the_request == NULL) {
		    ap_cpystrn(ss->request, "NULL", sizeof(ss->request));
	    } else if (r->parsed_uri.password == NULL) {
		    ap_cpystrn(ss->request, r->the_request, sizeof(ss->request));
	    } else {
		/* Don't reveal the password in the server-status view */
		    ap_cpystrn(ss->request, ap_pstrcat(r->pool, r->method, " ",
					       ap_unparse_uri_components(r->pool, &r->parsed_uri, UNP_OMITPASSWORD),
					       r->assbackwards ? NULL : " ", r->protocol, NULL),
				       sizeof(ss->request));
	    }
	    ap_cpystrn(ss->vhost, r->server->server_hostname, sizeof(ss->vhost));
	}
    }
    put_scoreboard_info(child_num, ss);

    return old_status;
}

static void update_scoreboard_global(void)
{
#ifdef SCOREBOARD_FILE
    lseek(scoreboard_fd,
	  (char *) &ap_scoreboard_image->global -(char *) ap_scoreboard_image, 0);
    force_write(scoreboard_fd, &ap_scoreboard_image->global,
		sizeof ap_scoreboard_image->global);
#endif
}

void ap_time_process_request(int child_num, int status)
{
    short_score *ss;
#if defined(NO_GETTIMEOFDAY) && !defined(NO_TIMES)
    struct tms tms_blk;
#endif

    if (child_num < 0)
	return;

    ap_sync_scoreboard_image();
    ss = &ap_scoreboard_image->servers[child_num];

    if (status == START_PREQUEST) {
#if defined(NO_GETTIMEOFDAY)
#ifndef NO_TIMES
	if ((ss->start_time = times(&tms_blk)) == -1)
#endif /* NO_TIMES */
	    ss->start_time = (clock_t) 0;
#else
	if (gettimeofday(&ss->start_time, (struct timezone *) 0) < 0)
	    ss->start_time.tv_sec =
		ss->start_time.tv_usec = 0L;
#endif
    }
    else if (status == STOP_PREQUEST) {
#if defined(NO_GETTIMEOFDAY)
#ifndef NO_TIMES
	if ((ss->stop_time = times(&tms_blk)) == -1)
#endif
	    ss->stop_time = ss->start_time = (clock_t) 0;
#else
	if (gettimeofday(&ss->stop_time, (struct timezone *) 0) < 0)
	    ss->stop_time.tv_sec =
		ss->stop_time.tv_usec =
		ss->start_time.tv_sec =
		ss->start_time.tv_usec = 0L;
#endif

    }

    put_scoreboard_info(child_num, ss);
}

static void increment_counts(int child_num, request_rec *r)
{
    long int bs = 0;
    short_score *ss;

    ap_sync_scoreboard_image();
    ss = &ap_scoreboard_image->servers[child_num];

    if (r->sent_bodyct)
	ap_bgetopt(r->connection->client, BO_BYTECT, &bs);

#ifndef NO_TIMES
    times(&ss->times);
#endif
    ss->access_count++;
    ss->my_access_count++;
    ss->conn_count++;
    ss->bytes_served += (unsigned long) bs;
    ss->my_bytes_served += (unsigned long) bs;
    ss->conn_bytes += (unsigned long) bs;

    put_scoreboard_info(child_num, ss);
}

static int find_child_by_pid(int pid)
{
    int i;

    for (i = 0; i < max_daemons_limit; ++i)
	if (ap_scoreboard_image->parent[i].pid == pid)
	    return i;

    return -1;
}

static void reclaim_child_processes(int terminate)
{
#ifndef MULTITHREAD
    int i, status;
    long int waittime = 1024 * 16;	/* in usecs */
    struct timeval tv;
    int waitret, tries;
    int not_dead_yet;
#ifndef NO_OTHER_CHILD
    other_child_rec *ocr, *nocr;
#endif

    ap_sync_scoreboard_image();

    for (tries = terminate ? 4 : 1; tries <= 9; ++tries) {
	/* don't want to hold up progress any more than 
	 * necessary, but we need to allow children a few moments to exit.
	 * Set delay with an exponential backoff.
	 */
	tv.tv_sec = waittime / 1000000;
	tv.tv_usec = waittime % 1000000;
	waittime = waittime * 4;
	ap_select(0, NULL, NULL, NULL, &tv);

	/* now see who is done */
	not_dead_yet = 0;
	for (i = 0; i < max_daemons_limit; ++i) {
	    int pid = ap_scoreboard_image->parent[i].pid;

	    if (pid == my_pid || pid == 0)
		continue;

	    waitret = waitpid(pid, &status, WNOHANG);
	    if (waitret == pid || waitret == -1) {
		ap_scoreboard_image->parent[i].pid = 0;
		continue;
	    }
	    ++not_dead_yet;
	    switch (tries) {
	    case 1:     /*  16ms */
	    case 2:     /*  82ms */
		break;
	    case 3:     /* 344ms */
		/* perhaps it missed the SIGHUP, lets try again */
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING,
			    server_conf,
		    "child process %d did not exit, sending another SIGHUP",
			    pid);
		kill(pid, SIGHUP);
		waittime = 1024 * 16;
		break;
	    case 4:     /*  16ms */
	    case 5:     /*  82ms */
	    case 6:     /* 344ms */
		break;
	    case 7:     /* 1.4sec */
		/* ok, now it's being annoying */
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING,
			    server_conf,
		   "child process %d still did not exit, sending a SIGTERM",
			    pid);
		kill(pid, SIGTERM);
		break;
	    case 8:     /*  6 sec */
		/* die child scum */
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, server_conf,
		   "child process %d still did not exit, sending a SIGKILL",
			    pid);
		kill(pid, SIGKILL);
		break;
	    case 9:     /* 14 sec */
		/* gave it our best shot, but alas...  If this really 
		 * is a child we are trying to kill and it really hasn't
		 * exited, we will likely fail to bind to the port
		 * after the restart.
		 */
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, server_conf,
			    "could not make child process %d exit, "
			    "attempting to continue anyway", pid);
		break;
	    }
	}
#ifndef NO_OTHER_CHILD
	for (ocr = other_children; ocr; ocr = nocr) {
	    nocr = ocr->next;
	    if (ocr->pid == -1)
		continue;

	    waitret = waitpid(ocr->pid, &status, WNOHANG);
	    if (waitret == ocr->pid) {
		ocr->pid = -1;
		(*ocr->maintenance) (OC_REASON_DEATH, ocr->data, status);
	    }
	    else if (waitret == 0) {
		(*ocr->maintenance) (OC_REASON_RESTART, ocr->data, -1);
		++not_dead_yet;
	    }
	    else if (waitret == -1) {
		/* uh what the heck? they didn't call unregister? */
		ocr->pid = -1;
		(*ocr->maintenance) (OC_REASON_LOST, ocr->data, -1);
	    }
	}
#endif
	if (!not_dead_yet) {
	    /* nothing left to wait for */
	    break;
	}
    }
#endif /* ndef MULTITHREAD */
}


#if defined(NEED_WAITPID)
/*
   Systems without a real waitpid sometimes lose a child's exit while waiting
   for another.  Search through the scoreboard for missing children.
 */
int reap_children(ap_wait_t *status)
{
    int n, pid;

    for (n = 0; n < max_daemons_limit; ++n) {
        ap_sync_scoreboard_image();
	if (ap_scoreboard_image->servers[n].status != SERVER_DEAD &&
		kill((pid = ap_scoreboard_image->parent[n].pid), 0) == -1) {
	    ap_update_child_status(n, SERVER_DEAD, NULL);
	    /* just mark it as having a successful exit status */
	    bzero((char *) status, sizeof(ap_wait_t));
	    return(pid);
	}
    }
    return 0;
}
#endif

/* Finally, this routine is used by the caretaker process to wait for
 * a while...
 */

/* number of calls to wait_or_timeout between writable probes */
#ifndef INTERVAL_OF_WRITABLE_PROBES
#define INTERVAL_OF_WRITABLE_PROBES 10
#endif
static int wait_or_timeout_counter;

static int wait_or_timeout(ap_wait_t *status)
{
#ifdef WIN32
#define MAXWAITOBJ MAXIMUM_WAIT_OBJECTS
    HANDLE h[MAXWAITOBJ];
    int e[MAXWAITOBJ];
    int round, pi, hi, rv, err;
    for (round = 0; round <= (HARD_SERVER_LIMIT - 1) / MAXWAITOBJ + 1; round++) {
	hi = 0;
	for (pi = round * MAXWAITOBJ;
	     (pi < (round + 1) * MAXWAITOBJ) && (pi < HARD_SERVER_LIMIT);
	     pi++) {
	    if (ap_scoreboard_image->servers[pi].status != SERVER_DEAD) {
		e[hi] = pi;
		h[hi++] = (HANDLE) ap_scoreboard_image->parent[pi].pid;
	    }

	}
	if (hi > 0) {
	    rv = WaitForMultipleObjects(hi, h, FALSE, 10000);
	    if (rv == -1)
		err = GetLastError();
	    if ((WAIT_OBJECT_0 <= (unsigned int) rv) && ((unsigned int) rv < (WAIT_OBJECT_0 + hi)))
		return (ap_scoreboard_image->parent[e[rv - WAIT_OBJECT_0]].pid);
	    else if ((WAIT_ABANDONED_0 <= (unsigned int) rv) && ((unsigned int) rv < (WAIT_ABANDONED_0 + hi)))
		return (ap_scoreboard_image->parent[e[rv - WAIT_ABANDONED_0]].pid);

	}
    }
    return (-1);

#else /* WIN32 */
    struct timeval tv;
    int ret;

    ++wait_or_timeout_counter;
    if (wait_or_timeout_counter == INTERVAL_OF_WRITABLE_PROBES) {
	wait_or_timeout_counter = 0;
#ifndef NO_OTHER_CHILD
	probe_writable_fds();
#endif
    }
    ret = waitpid(-1, status, WNOHANG);
    if (ret == -1 && errno == EINTR) {
	return -1;
    }
    if (ret > 0) {
	return ret;
    }
#ifdef NEED_WAITPID
    if ((ret = reap_children(status)) > 0) {
	return ret;
    }
#endif
    tv.tv_sec = SCOREBOARD_MAINTENANCE_INTERVAL / 1000000;
    tv.tv_usec = SCOREBOARD_MAINTENANCE_INTERVAL % 1000000;
    ap_select(0, NULL, NULL, NULL, &tv);
    return -1;
#endif /* WIN32 */
}


#if defined(NSIG)
#define NumSIG NSIG
#elif defined(_NSIG)
#define NumSIG _NSIG
#elif defined(__NSIG)
#define NumSIG __NSIG
#else
#define NumSIG 32   /* for 1998's unixes, this is still a good assumption */
#endif

#ifdef SYS_SIGLIST /* platform has sys_siglist[] */
#define INIT_SIGLIST()  /*nothing*/
#else /* platform has no sys_siglist[], define our own */
#define SYS_SIGLIST ap_sys_siglist
#define INIT_SIGLIST() siglist_init();

const char *ap_sys_siglist[NumSIG];

static void siglist_init(void)
{
    int sig;

    ap_sys_siglist[0] = "Signal 0";
#ifdef SIGHUP
    ap_sys_siglist[SIGHUP] = "Hangup";
#endif
#ifdef SIGINT
    ap_sys_siglist[SIGINT] = "Interrupt";
#endif
#ifdef SIGQUIT
    ap_sys_siglist[SIGQUIT] = "Quit";
#endif
#ifdef SIGILL
    ap_sys_siglist[SIGILL] = "Illegal instruction";
#endif
#ifdef SIGTRAP
    ap_sys_siglist[SIGTRAP] = "Trace/BPT trap";
#endif
#ifdef SIGIOT
    ap_sys_siglist[SIGIOT] = "IOT instruction";
#endif
#ifdef SIGABRT
    ap_sys_siglist[SIGABRT] = "Abort";
#endif
#ifdef SIGEMT
    ap_sys_siglist[SIGEMT] = "Emulator trap";
#endif
#ifdef SIGFPE
    ap_sys_siglist[SIGFPE] = "Arithmetic exception";
#endif
#ifdef SIGKILL
    ap_sys_siglist[SIGKILL] = "Killed";
#endif
#ifdef SIGBUS
    ap_sys_siglist[SIGBUS] = "Bus error";
#endif
#ifdef SIGSEGV
    ap_sys_siglist[SIGSEGV] = "Segmentation fault";
#endif
#ifdef SIGSYS
    ap_sys_siglist[SIGSYS] = "Bad system call";
#endif
#ifdef SIGPIPE
    ap_sys_siglist[SIGPIPE] = "Broken pipe";
#endif
#ifdef SIGALRM
    ap_sys_siglist[SIGALRM] = "Alarm clock";
#endif
#ifdef SIGTERM
    ap_sys_siglist[SIGTERM] = "Terminated";
#endif
#ifdef SIGUSR1
    ap_sys_siglist[SIGUSR1] = "User defined signal 1";
#endif
#ifdef SIGUSR2
    ap_sys_siglist[SIGUSR2] = "User defined signal 2";
#endif
#ifdef SIGCLD
    ap_sys_siglist[SIGCLD] = "Child status change";
#endif
#ifdef SIGCHLD
    ap_sys_siglist[SIGCHLD] = "Child status change";
#endif
#ifdef SIGPWR
    ap_sys_siglist[SIGPWR] = "Power-fail restart";
#endif
#ifdef SIGWINCH
    ap_sys_siglist[SIGWINCH] = "Window changed";
#endif
#ifdef SIGURG
    ap_sys_siglist[SIGURG] = "urgent socket condition";
#endif
#ifdef SIGPOLL
    ap_sys_siglist[SIGPOLL] = "Pollable event occurred";
#endif
#ifdef SIGIO
    ap_sys_siglist[SIGIO] = "socket I/O possible";
#endif
#ifdef SIGSTOP
    ap_sys_siglist[SIGSTOP] = "Stopped (signal)";
#endif
#ifdef SIGTSTP
    ap_sys_siglist[SIGTSTP] = "Stopped";
#endif
#ifdef SIGCONT
    ap_sys_siglist[SIGCONT] = "Continued";
#endif
#ifdef SIGTTIN
    ap_sys_siglist[SIGTTIN] = "Stopped (tty input)";
#endif
#ifdef SIGTTOU
    ap_sys_siglist[SIGTTOU] = "Stopped (tty output)";
#endif
#ifdef SIGVTALRM
    ap_sys_siglist[SIGVTALRM] = "virtual timer expired";
#endif
#ifdef SIGPROF
    ap_sys_siglist[SIGPROF] = "profiling timer expired";
#endif
#ifdef SIGXCPU
    ap_sys_siglist[SIGXCPU] = "exceeded cpu limit";
#endif
#ifdef SIGXFSZ
    ap_sys_siglist[SIGXFSZ] = "exceeded file size limit";
#endif
    for (sig=0; sig < sizeof(ap_sys_siglist)/sizeof(ap_sys_siglist[0]); ++sig)
        if (ap_sys_siglist[sig] == NULL)
            ap_sys_siglist[sig] = "";
}
#endif /* platform has sys_siglist[] */


/* handle all varieties of core dumping signals */
static void sig_coredump(int sig)
{
    chdir(ap_coredump_dir);
    signal(sig, SIG_DFL);
#ifndef WIN32
    kill(getpid(), sig);
#else
    raise(sig);
#endif
    /* At this point we've got sig blocked, because we're still inside
     * the signal handler.  When we leave the signal handler it will
     * be unblocked, and we'll take the signal... and coredump or whatever
     * is appropriate for this particular Unix.  In addition the parent
     * will see the real signal we received -- whereas if we called
     * abort() here, the parent would only see SIGABRT.
     */
}

/*****************************************************************
 * Connection structures and accounting...
 */

static void just_die(int sig)
{				/* SIGHUP to child process??? */
    /* if alarms are blocked we have to wait to die otherwise we might
     * end up with corruption in alloc.c's internal structures */
    if (alarms_blocked) {
	exit_after_unblock = 1;
    }
    else {
	clean_child_exit(0);
    }
}

static int volatile usr1_just_die = 1;
static int volatile deferred_die;

static void usr1_handler(int sig)
{
    if (usr1_just_die) {
	just_die(sig);
    }
    deferred_die = 1;
}

/* volatile just in case */
static int volatile shutdown_pending;
static int volatile restart_pending;
static int volatile is_graceful;
static int volatile generation;

#ifdef WIN32
/*
 * signal_parent() tells the parent process to wake up and do something.
 * Once woken it will look at shutdown_pending and restart_pending to decide
 * what to do. If neither variable is set, it will do a shutdown. This function
 * if called by start_shutdown() or start_restart() in the parent's process
 * space, so that the variables get set. However it can also be called 
 * by child processes to force the parent to exit in an emergency.
 */

static void signal_parent(void)
{
    HANDLE e;
    /* after updating the shutdown_pending or restart flags, we need
     * to wake up the parent process so it can see the changes. The
     * parent will normally be waiting for either a child process
     * to die, or for a signal on the "spache-signal" event. So set the
     * "apache-signal" event here.
     */

    if (one_process) {
	return;
    }

    APD1("*** SIGNAL_PARENT SET ***");

    e = OpenEvent(EVENT_ALL_ACCESS, FALSE, "apache-signal");
    if (!e) {
	/* Um, problem, can't signal the main loop, which means we can't
	 * signal ourselves to die. Ignore for now...
	 */
	ap_log_error(APLOG_MARK, APLOG_EMERG|APLOG_WIN32ERROR, server_conf,
	    "OpenEvent on apache-signal event");
	return;
    }
    if (SetEvent(e) == 0) {
	/* Same problem as above */
	ap_log_error(APLOG_MARK, APLOG_EMERG|APLOG_WIN32ERROR, server_conf,
	    "SetEvent on apache-signal event");
	return;
    }
    CloseHandle(e);
}
#endif

/*
 * start_shutdown() and start_restart(), below, are a first stab at
 * functions to initiate shutdown or restart without relying on signals. 
 * Previously this was initiated in sig_term() and restart() signal handlers, 
 * but we want to be able to start a shutdown/restart from other sources --
 * e.g. on Win32, from the service manager. Now the service manager can
 * call start_shutdown() or start_restart() as appropiate. 
 *
 * These should only be called from the parent process itself, since the
 * parent process will use the shutdown_pending and restart_pending variables
 * to determine whether to shutdown or restart. The child process should
 * call signal_parent() directly to tell the parent to die -- this will
 * cause neither of those variable to be set, which the parent will
 * assume means something serious is wrong (which it will be, for the
 * child to force an exit) and so do an exit anyway.
 */

void ap_start_shutdown(void)
{
    if (shutdown_pending == 1) {
	/* Um, is this _probably_ not an error, if the user has
	 * tried to do a shutdown twice quickly, so we won't
	 * worry about reporting it.
	 */
	return;
    }
    shutdown_pending = 1;

#ifdef WIN32
    signal_parent();	    /* get the parent process to wake up */
#endif
}

/* do a graceful restart if graceful == 1 */
void ap_start_restart(int graceful)
{
    if (restart_pending == 1) {
	/* Probably not an error - don't bother reporting it */
	return;
    }
    restart_pending = 1;
    is_graceful = graceful;

#ifdef WIN32
    signal_parent();	    /* get the parent process to wake up */
#endif /* WIN32 */
}

static void sig_term(int sig)
{
    ap_start_shutdown();
}

static void restart(int sig)
{
#ifndef WIN32
    ap_start_restart(sig == SIGUSR1);
#else
    ap_start_restart(1);
#endif
}

static void set_signals(void)
{
#ifndef NO_USE_SIGACTION
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (!one_process) {
	sa.sa_handler = sig_coredump;
#if defined(SA_ONESHOT)
	sa.sa_flags = SA_ONESHOT;
#elif defined(SA_RESETHAND)
	sa.sa_flags = SA_RESETHAND;
#endif
	if (sigaction(SIGSEGV, &sa, NULL) < 0)
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGSEGV)");
#ifdef SIGBUS
	if (sigaction(SIGBUS, &sa, NULL) < 0)
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGBUS)");
#endif
#ifdef SIGABORT
	if (sigaction(SIGABORT, &sa, NULL) < 0)
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGABORT)");
#endif
#ifdef SIGABRT
	if (sigaction(SIGABRT, &sa, NULL) < 0)
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGABRT)");
#endif
#ifdef SIGILL
	if (sigaction(SIGILL, &sa, NULL) < 0)
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGILL)");
#endif
	sa.sa_flags = 0;
    }
    sa.sa_handler = sig_term;
    if (sigaction(SIGTERM, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGTERM)");
#ifdef SIGINT
    if (sigaction(SIGINT, &sa, NULL) < 0)
        ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGINT)");
#endif
#ifdef SIGXCPU
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGXCPU, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGXCPU)");
#endif
#ifdef SIGXFSZ
    sa.sa_handler = SIG_DFL;
    if (sigaction(SIGXFSZ, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGXFSZ)");
#endif

    /* we want to ignore HUPs and USR1 while we're busy processing one */
    sigaddset(&sa.sa_mask, SIGHUP);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sa.sa_handler = restart;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGHUP)");
    if (sigaction(SIGUSR1, &sa, NULL) < 0)
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "sigaction(SIGUSR1)");
#else
    if (!one_process) {
	signal(SIGSEGV, sig_coredump);
#ifdef SIGBUS
	signal(SIGBUS, sig_coredump);
#endif /* SIGBUS */
#ifdef SIGABORT
	signal(SIGABORT, sig_coredump);
#endif /* SIGABORT */
#ifdef SIGABRT
	signal(SIGABRT, sig_coredump);
#endif /* SIGABRT */
#ifdef SIGILL
	signal(SIGILL, sig_coredump);
#endif /* SIGILL */
#ifdef SIGXCPU
	signal(SIGXCPU, SIG_DFL);
#endif /* SIGXCPU */
#ifdef SIGXFSZ
	signal(SIGXFSZ, SIG_DFL);
#endif /* SIGXFSZ */
    }

    signal(SIGTERM, sig_term);
#ifdef SIGHUP
    signal(SIGHUP, restart);
#endif /* SIGHUP */
#ifdef SIGUSR1
    signal(SIGUSR1, restart);
#endif /* SIGUSR1 */
#endif
}


/*****************************************************************
 * Here follows a long bunch of generic server bookkeeping stuff...
 */

static void detach(void)
{
#if !defined(WIN32)
    int x;

    chdir("/");
#if !defined(MPE) && !defined(OS2)
/* Don't detach for MPE because child processes can't survive the death of
   the parent. */
    if ((x = fork()) > 0)
	exit(0);
    else if (x == -1) {
	perror("fork");
	fprintf(stderr, "httpd: unable to fork new process\n");
	exit(1);
    }
    RAISE_SIGSTOP(DETACH);
#endif
#ifndef NO_SETSID
    if ((pgrp = setsid()) == -1) {
	perror("setsid");
	fprintf(stderr, "httpd: setsid failed\n");
	exit(1);
    }
#elif defined(NEXT) || defined(NEWSOS)
    if (setpgrp(0, getpid()) == -1 || (pgrp = getpgrp(0)) == -1) {
	perror("setpgrp");
	fprintf(stderr, "httpd: setpgrp or getpgrp failed\n");
	exit(1);
    }
#elif defined(OS2)
    /* OS/2 don't support process group IDs */
    pgrp = getpid();
#elif defined(MPE)
    /* MPE uses negative pid for process group */
    pgrp = -getpid();
#else
    if ((pgrp = setpgrp(getpid(), 0)) == -1) {
	perror("setpgrp");
	fprintf(stderr, "httpd: setpgrp failed\n");
	exit(1);
    }
#endif

    /* close out the standard file descriptors */
    if (freopen("/dev/null", "r", stdin) == NULL) {
	fprintf(stderr, "httpd: unable to replace stdin with /dev/null: %s\n",
		strerror(errno));
	/* continue anyhow -- note we can't close out descriptor 0 because we
	 * have nothing to replace it with, and if we didn't have a descriptor
	 * 0 the next file would be created with that value ... leading to
	 * havoc.
	 */
    }
    if (freopen("/dev/null", "w", stdout) == NULL) {
	fprintf(stderr, "httpd: unable to replace stdout with /dev/null: %s\n",
		strerror(errno));
    }
    /* stderr is a tricky one, we really want it to be the error_log,
     * but we haven't opened that yet.  So leave it alone for now and it'll
     * be reopened moments later.
     */
#endif /* ndef WIN32 */
}

/* Set group privileges.
 *
 * Note that we use the username as set in the config files, rather than
 * the lookup of to uid --- the same uid may have multiple passwd entries,
 * with different sets of groups for each.
 */

static void set_group_privs(void)
{
#ifndef WIN32
    if (!geteuid()) {
	char *name;

	/* Get username if passed as a uid */

	if (ap_user_name[0] == '#') {
	    struct passwd *ent;
	    uid_t uid = atoi(&ap_user_name[1]);

	    if ((ent = getpwuid(uid)) == NULL) {
		ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			 "getpwuid: couldn't determine user name from uid %u, "
			 "you probably need to modify the User directive",
			 (unsigned)uid);
		clean_child_exit(APEXIT_CHILDFATAL);
	    }

	    name = ent->pw_name;
	}
	else
	    name = ap_user_name;

#ifndef OS2
	/* OS/2 dosen't support groups. */

	/* Reset `groups' attributes. */

	if (initgroups(name, ap_group_id) == -1) {
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			"initgroups: unable to set groups for User %s "
			"and Group %u", name, (unsigned)ap_group_id);
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
#ifdef MULTIPLE_GROUPS
	if (getgroups(NGROUPS_MAX, group_id_list) == -1) {
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			"getgroups: unable to get group list");
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
#endif
	if (setgid(ap_group_id) == -1) {
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			"setgid: unable to set group id to Group %u",
			(unsigned)ap_group_id);
	    clean_child_exit(APEXIT_CHILDFATAL);
	}
#endif
    }
#endif /* ndef WIN32 */
}

/* check to see if we have the 'suexec' setuid wrapper installed */
static int init_suexec(void)
{
#ifndef WIN32
    struct stat wrapper;

    if ((stat(SUEXEC_BIN, &wrapper)) != 0)
	return (ap_suexec_enabled);

    if ((wrapper.st_mode & S_ISUID) && wrapper.st_uid == 0) {
	ap_suexec_enabled = 1;
    }
#endif /* ndef WIN32 */
    return (ap_suexec_enabled);
}

/*****************************************************************
 * Connection structures and accounting...
 */


static conn_rec *new_connection(pool *p, server_rec *server, BUFF *inout,
			     const struct sockaddr_in *remaddr,
			     const struct sockaddr_in *saddr,
			     int child_num)
{
    conn_rec *conn = (conn_rec *) ap_pcalloc(p, sizeof(conn_rec));

    /* Got a connection structure, so initialize what fields we can
     * (the rest are zeroed out by pcalloc).
     */

    conn->child_num = child_num;

    conn->pool = p;
    conn->local_addr = *saddr;
    conn->server = server; /* just a guess for now */
    ap_update_vhost_given_ip(conn);
    conn->base_server = conn->server;
    conn->client = inout;

    conn->remote_addr = *remaddr;
    conn->remote_ip = ap_pstrdup(conn->pool,
			      inet_ntoa(conn->remote_addr.sin_addr));

    return conn;
}

#if defined(TCP_NODELAY) && !defined(MPE)
static void sock_disable_nagle(int s)
{
    /* The Nagle algorithm says that we should delay sending partial
     * packets in hopes of getting more data.  We don't want to do
     * this; we are not telnet.  There are bad interactions between
     * persistent connections and Nagle's algorithm that have very severe
     * performance penalties.  (Failing to disable Nagle is not much of a
     * problem with simple HTTP.)
     *
     * In spite of these problems, failure here is not a shooting offense.
     */
    int just_say_no = 1;

    if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *) &just_say_no,
		   sizeof(int)) < 0) {
	ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf,
		    "setsockopt: (TCP_NODELAY)");
    }
}

#else
#define sock_disable_nagle(s)	/* NOOP */
#endif


static int make_sock(pool *p, const struct sockaddr_in *server)
{
    int s;
    int one = 1;
    char addr[512];

    if (server->sin_addr.s_addr != htonl(INADDR_ANY))
	ap_snprintf(addr, sizeof(addr), "address %s port %d",
		inet_ntoa(server->sin_addr), ntohs(server->sin_port));
    else
	ap_snprintf(addr, sizeof(addr), "port %d", ntohs(server->sin_port));

    /* note that because we're about to slack we don't use psocket */
    ap_block_alarms();
    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
	ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
		    "make_sock: failed to get a socket for %s", addr);
	ap_unblock_alarms();
	exit(1);
    }

    /* Solaris (probably versions 2.4, 2.5, and 2.5.1 with various levels
     * of tcp patches) has some really weird bugs where if you dup the
     * socket now it breaks things across SIGHUP restarts.  It'll either
     * be unable to bind, or it won't respond.
     */
#if defined (SOLARIS2) && SOLARIS2 < 260
#define WORKAROUND_SOLARIS_BUG
#endif

    /* PR#1282 Unixware 1.x appears to have the same problem as solaris */
#if defined (UW) && UW < 200
#define WORKAROUND_SOLARIS_BUG
#endif

    /* PR#1973 NCR SVR4 systems appear to have the same problem */
#if defined (MPRAS)
#define WORKAROUND_SOLARIS_BUG
#endif

#ifndef WORKAROUND_SOLARIS_BUG
    s = ap_slack(s, AP_SLACK_HIGH);

    ap_note_cleanups_for_socket(p, s);	/* arrange to close on exec or restart */
#endif

#ifndef MPE
/* MPE does not support SO_REUSEADDR and SO_KEEPALIVE */
#ifndef _OSD_POSIX
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(int)) < 0) {
	ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
		    "make_sock: for %s, setsockopt: (SO_REUSEADDR)", addr);
	close(s);
	ap_unblock_alarms();
	return -1;
    }
#endif /*_OSD_POSIX*/
    one = 1;
#ifndef BEOS
/* BeOS does not support SO_KEEPALIVE */
    if (setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, (char *) &one, sizeof(int)) < 0) {
	ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
		    "make_sock: for %s, setsockopt: (SO_KEEPALIVE)", addr);
	close(s);
	ap_unblock_alarms();
	return -1;
    }
#endif
#endif

    sock_disable_nagle(s);
    sock_enable_linger(s);

    /*
     * To send data over high bandwidth-delay connections at full
     * speed we must force the TCP window to open wide enough to keep the
     * pipe full.  The default window size on many systems
     * is only 4kB.  Cross-country WAN connections of 100ms
     * at 1Mb/s are not impossible for well connected sites.
     * If we assume 100ms cross-country latency,
     * a 4kB buffer limits throughput to 40kB/s.
     *
     * To avoid this problem I've added the SendBufferSize directive
     * to allow the web master to configure send buffer size.
     *
     * The trade-off of larger buffers is that more kernel memory
     * is consumed.  YMMV, know your customers and your network!
     *
     * -John Heidemann <johnh@isi.edu> 25-Oct-96
     *
     * If no size is specified, use the kernel default.
     */
#ifndef BEOS			/* BeOS does not support SO_SNDBUF */
    if (server_conf->send_buffer_size) {
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF,
		(char *) &server_conf->send_buffer_size, sizeof(int)) < 0) {
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf,
			"make_sock: failed to set SendBufferSize for %s, "
			"using default", addr);
	    /* not a fatal error */
	}
    }
#endif

#ifdef MPE
/* MPE requires CAP=PM and GETPRIVMODE to bind to ports less than 1024 */
    if (ntohs(server->sin_port) < 1024)
	GETPRIVMODE();
#endif
    if (bind(s, (struct sockaddr *) server, sizeof(struct sockaddr_in)) == -1) {
	ap_log_error(APLOG_MARK, APLOG_CRIT, server_conf,
	    "make_sock: could not bind to %s", addr);
#ifdef MPE
	if (ntohs(server->sin_port) < 1024)
	    GETUSERMODE();
#endif
	close(s);
	ap_unblock_alarms();
	exit(1);
    }
#ifdef MPE
    if (ntohs(server->sin_port) < 1024)
	GETUSERMODE();
#endif

    if (listen(s, ap_listenbacklog) == -1) {
	ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
	    "make_sock: unable to listen for connections on %s", addr);
	close(s);
	ap_unblock_alarms();
	exit(1);
    }

#ifdef WORKAROUND_SOLARIS_BUG
    s = ap_slack(s, AP_SLACK_HIGH);

    ap_note_cleanups_for_socket(p, s);	/* arrange to close on exec or restart */
#endif
    ap_unblock_alarms();

#ifndef WIN32
    /* protect various fd_sets */
    if (s >= FD_SETSIZE) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, NULL,
	    "make_sock: problem listening on %s, filedescriptor (%u) "
	    "larger than FD_SETSIZE (%u) "
	    "found, you probably need to rebuild Apache with a "
	    "larger FD_SETSIZE", addr, s, FD_SETSIZE);
	close(s);
	return -1;
    }
#endif

    return s;
}


/*
 * During a restart we keep track of the old listeners here, so that we
 * can re-use the sockets.  We have to do this because we won't be able
 * to re-open the sockets ("Address already in use").
 *
 * Unlike the listeners ring, old_listeners is a NULL terminated list.
 *
 * copy_listeners() makes the copy, find_listener() finds an old listener
 * and close_unused_listener() cleans up whatever wasn't used.
 */
static listen_rec *old_listeners;

/* unfortunately copy_listeners may be called before listeners is a ring */
static void copy_listeners(pool *p)
{
    listen_rec *lr;

    ap_assert(old_listeners == NULL);
    if (ap_listeners == NULL) {
	return;
    }
    lr = ap_listeners;
    do {
	listen_rec *nr = malloc(sizeof *nr);
	if (nr == NULL) {
	    fprintf(stderr, "Ouch!  malloc failed in copy_listeners()\n");
	    exit(1);
	}
	*nr = *lr;
	ap_kill_cleanups_for_socket(p, nr->fd);
	nr->next = old_listeners;
	ap_assert(!nr->used);
	old_listeners = nr;
	lr = lr->next;
    } while (lr && lr != ap_listeners);
}


static int find_listener(listen_rec *lr)
{
    listen_rec *or;

    for (or = old_listeners; or; or = or->next) {
	if (!memcmp(&or->local_addr, &lr->local_addr, sizeof(or->local_addr))) {
	    or->used = 1;
	    return or->fd;
	}
    }
    return -1;
}


static void close_unused_listeners(void)
{
    listen_rec *or, *next;

    for (or = old_listeners; or; or = next) {
	next = or->next;
	if (!or->used)
	    closesocket(or->fd);
	free(or);
    }
    old_listeners = NULL;
}


/* open sockets, and turn the listeners list into a singly linked ring */
static void setup_listeners(pool *p)
{
    listen_rec *lr;
    int fd;

    listenmaxfd = -1;
    FD_ZERO(&listenfds);
    lr = ap_listeners;
    for (;;) {
	fd = find_listener(lr);
	if (fd < 0) {
	    fd = make_sock(p, &lr->local_addr);
	}
	else {
	    ap_note_cleanups_for_socket(p, fd);
	}
	if (fd >= 0) {
	    FD_SET(fd, &listenfds);
	    if (fd > listenmaxfd)
		listenmaxfd = fd;
	}
	lr->fd = fd;
	if (lr->next == NULL)
	    break;
	lr = lr->next;
    }
    /* turn the list into a ring */
    lr->next = ap_listeners;
    head_listener = ap_listeners;
    close_unused_listeners();

#ifdef NO_SERIALIZED_ACCEPT
    /* warn them about the starvation problem if they're using multiple
     * sockets
     */
    if (ap_listeners->next != ap_listeners) {
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_CRIT, NULL,
		    "You cannot use multiple Listens safely on your system, "
		    "proceeding anyway.  See src/PORTING, search for "
		    "SERIALIZED_ACCEPT.");
    }
#endif
}


/*
 * Find a listener which is ready for accept().  This advances the
 * head_listener global.
 */
static ap_inline listen_rec *find_ready_listener(fd_set * main_fds)
{
    listen_rec *lr;

    lr = head_listener;
    do {
	if (FD_ISSET(lr->fd, main_fds)) {
	    head_listener = lr->next;
	    return (lr);
	}
	lr = lr->next;
    } while (lr != head_listener);
    return NULL;
}


#ifdef WIN32
static int s_iInitCount = 0;

static int AMCSocketInitialize(void)
{
    int iVersionRequested;
    WSADATA wsaData;
    int err;

    if (s_iInitCount > 0) {
	s_iInitCount++;
	return (0);
    }
    else if (s_iInitCount < 0)
	return (s_iInitCount);

    /* s_iInitCount == 0. Do the initailization */
    iVersionRequested = MAKEWORD(1, 1);
    err = WSAStartup((WORD) iVersionRequested, &wsaData);
    if (err) {
	s_iInitCount = -1;
	return (s_iInitCount);
    }
    if (LOBYTE(wsaData.wVersion) != 1 ||
	HIBYTE(wsaData.wVersion) != 1) {
	s_iInitCount = -2;
	WSACleanup();
	return (s_iInitCount);
    }

    s_iInitCount++;
    return (s_iInitCount);

}


static void AMCSocketCleanup(void)
{
    if (--s_iInitCount == 0)
	WSACleanup();
    return;
}
#endif

static void show_compile_settings(void)
{
    printf("Server version: %s\n", ap_get_server_version());
    printf("Server built:   %s\n", ap_get_server_built());
    printf("Server's Module Magic Number: %u:%u\n",
	   MODULE_MAGIC_NUMBER_MAJOR, MODULE_MAGIC_NUMBER_MINOR);
    printf("Server compiled with....\n");
#ifdef BIG_SECURITY_HOLE
    printf(" -D BIG_SECURITY_HOLE\n");
#endif
#ifdef SECURITY_HOLE_PASS_AUTHORIZATION
    printf(" -D SECURITY_HOLE_PASS_AUTHORIZATION\n");
#endif
#ifdef HAVE_MMAP
    printf(" -D HAVE_MMAP\n");
#endif
#ifdef HAVE_SHMGET
    printf(" -D HAVE_SHMGET\n");
#endif
#ifdef USE_MMAP_SCOREBOARD
    printf(" -D USE_MMAP_SCOREBOARD\n");
#endif
#ifdef USE_SHMGET_SCOREBOARD
    printf(" -D USE_SHMGET_SCOREBOARD\n");
#endif
#ifdef USE_OS2_SCOREBOARD
    printf(" -D USE_OS2_SCOREBOARD\n");
#endif
#ifdef USE_POSIX_SCOREBOARD
    printf(" -D USE_POSIX_SCOREBOARD\n");
#endif
#ifdef USE_MMAP_FILES
    printf(" -D USE_MMAP_FILES\n");
#ifdef MMAP_SEGMENT_SIZE
	printf(" -D MMAP_SEGMENT_SIZE=%ld\n",(long)MMAP_SEGMENT_SIZE);
#endif
#endif /*USE_MMAP_FILES*/
#ifdef NO_WRITEV
    printf(" -D NO_WRITEV\n");
#endif
#ifdef NO_LINGCLOSE
    printf(" -D NO_LINGCLOSE\n");
#endif
#ifdef USE_FCNTL_SERIALIZED_ACCEPT
    printf(" -D USE_FCNTL_SERIALIZED_ACCEPT\n");
#endif
#ifdef USE_FLOCK_SERIALIZED_ACCEPT
    printf(" -D USE_FLOCK_SERIALIZED_ACCEPT\n");
#endif
#ifdef USE_USLOCK_SERIALIZED_ACCEPT
    printf(" -D USE_USLOCK_SERIALIZED_ACCEPT\n");
#endif
#ifdef USE_SYSVSEM_SERIALIZED_ACCEPT
    printf(" -D USE_SYSVSEM_SERIALIZED_ACCEPT\n");
#endif
#ifdef USE_PTHREAD_SERIALIZED_ACCEPT
    printf(" -D USE_PTHREAD_SERIALIZED_ACCEPT\n");
#endif
#ifdef SINGLE_LISTEN_UNSERIALIZED_ACCEPT
    printf(" -D SINGLE_LISTEN_UNSERIALIZED_ACCEPT\n");
#endif
#ifdef NO_OTHER_CHILD
    printf(" -D NO_OTHER_CHILD\n");
#endif
#ifdef NO_RELIABLE_PIPED_LOGS
    printf(" -D NO_RELIABLE_PIPED_LOGS\n");
#endif
#ifdef BUFFERED_LOGS
    printf(" -D BUFFERED_LOGS\n");
#ifdef PIPE_BUF
	printf(" -D PIPE_BUF=%ld\n",(long)PIPE_BUF);
#endif
#endif
#ifdef MULTITHREAD
    printf(" -D MULTITHREAD\n");
#endif
#ifdef CHARSET_EBCDIC
    printf(" -D CHARSET_EBCDIC\n");
#endif
#ifdef NEED_HASHBANG_EMUL
    printf(" -D NEED_HASHBANG_EMUL\n");
#endif
#ifdef SHARED_CORE
    printf(" -D SHARED_CORE\n");
#endif

/* This list displays the compiled-in default paths: */
#ifdef HTTPD_ROOT
    printf(" -D HTTPD_ROOT=\"" HTTPD_ROOT "\"\n");
#endif
#ifdef SUEXEC_BIN
    printf(" -D SUEXEC_BIN=\"" SUEXEC_BIN "\"\n");
#endif
#ifdef SHARED_CORE_DIR
    printf(" -D SHARED_CORE_DIR=\"" SHARED_CORE_DIR "\"\n");
#endif
#ifdef DEFAULT_PIDLOG
    printf(" -D DEFAULT_PIDLOG=\"" DEFAULT_PIDLOG "\"\n");
#endif
#ifdef DEFAULT_SCOREBOARD
    printf(" -D DEFAULT_SCOREBOARD=\"" DEFAULT_SCOREBOARD "\"\n");
#endif
#ifdef DEFAULT_LOCKFILE
    printf(" -D DEFAULT_LOCKFILE=\"" DEFAULT_LOCKFILE "\"\n");
#endif
#ifdef DEFAULT_XFERLOG
    printf(" -D DEFAULT_XFERLOG=\"" DEFAULT_XFERLOG "\"\n");
#endif
#ifdef DEFAULT_ERRORLOG
    printf(" -D DEFAULT_ERRORLOG=\"" DEFAULT_ERRORLOG "\"\n");
#endif
#ifdef TYPES_CONFIG_FILE
    printf(" -D TYPES_CONFIG_FILE=\"" TYPES_CONFIG_FILE "\"\n");
#endif
#ifdef SERVER_CONFIG_FILE
    printf(" -D SERVER_CONFIG_FILE=\"" SERVER_CONFIG_FILE "\"\n");
#endif
#ifdef ACCESS_CONFIG_FILE
    printf(" -D ACCESS_CONFIG_FILE=\"" ACCESS_CONFIG_FILE "\"\n");
#endif
#ifdef RESOURCE_CONFIG_FILE
    printf(" -D RESOURCE_CONFIG_FILE=\"" RESOURCE_CONFIG_FILE "\"\n");
#endif
}


/* Some init code that's common between win32 and unix... well actually
 * some of it is #ifdef'd but was duplicated before anyhow.  This stuff
 * is still a mess.
 */
static void common_init(void)
{
    INIT_SIGLIST()
#ifdef AUX3
    (void) set42sig();
#endif

#ifdef WIN32
    /* Initialize the stupid sockets */
    AMCSocketInitialize();
#endif /* WIN32 */

    pconf = ap_init_alloc();
    ptrans = ap_make_sub_pool(pconf);

    ap_util_init();
    ap_util_uri_init();

    pcommands = ap_make_sub_pool(NULL);
    ap_server_pre_read_config  = ap_make_array(pcommands, 1, sizeof(char *));
    ap_server_post_read_config = ap_make_array(pcommands, 1, sizeof(char *));
    ap_server_config_defines   = ap_make_array(pcommands, 1, sizeof(char *));
}

#ifndef MULTITHREAD
/*****************************************************************
 * Child process main loop.
 * The following vars are static to avoid getting clobbered by longjmp();
 * they are really private to child_main.
 */

static int srv;
static int csd;
static int dupped_csd;
static int requests_this_child;
static fd_set main_fds;

API_EXPORT(void) ap_child_terminate(request_rec *r)
{
    r->connection->keepalive = 0;
    requests_this_child = ap_max_requests_per_child = 1;
}

static void child_main(int child_num_arg)
{
    NET_SIZE_T clen;
    struct sockaddr sa_server;
    struct sockaddr sa_client;
    listen_rec *lr;

    /* All of initialization is a critical section, we don't care if we're
     * told to HUP or USR1 before we're done initializing.  For example,
     * we could be half way through child_init_modules() when a restart
     * signal arrives, and we'd have no real way to recover gracefully
     * and exit properly.
     *
     * I suppose a module could take forever to initialize, but that would
     * be either a broken module, or a broken configuration (i.e. network
     * problems, file locking problems, whatever). -djg
     */
    ap_block_alarms();

    my_pid = getpid();
    csd = -1;
    dupped_csd = -1;
    my_child_num = child_num_arg;
    requests_this_child = 0;

    /* Get a sub pool for global allocations in this child, so that
     * we can have cleanups occur when the child exits.
     */
    pchild = ap_make_sub_pool(pconf);

    /* needs to be done before we switch UIDs so we have permissions */
    reopen_scoreboard(pchild);
    SAFE_ACCEPT(accept_mutex_child_init(pchild));

    set_group_privs();
#ifdef MPE
    /* Only try to switch if we're running as MANAGER.SYS */
    if (geteuid() == 1 && ap_user_id > 1) {
	GETPRIVMODE();
	if (setuid(ap_user_id) == -1) {
	    GETUSERMODE();
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			"setuid: unable to change uid");
	    exit(1);
	}
	GETUSERMODE();
    }
#else
    /* Only try to switch if we're running as root */
    if (!geteuid() && (
#ifdef _OSD_POSIX
	os_init_job_environment(server_conf, ap_user_name) != 0 || 
#endif
	setuid(ap_user_id) == -1)) {
	ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
		    "setuid: unable to change uid");
	clean_child_exit(APEXIT_CHILDFATAL);
    }
#endif

    ap_child_init_modules(pchild, server_conf);

    /* done with the initialization critical section */
    ap_unblock_alarms();

    (void) ap_update_child_status(my_child_num, SERVER_READY, (request_rec *) NULL);

    /*
     * Setup the jump buffers so that we can return here after
     * a signal or a timeout (yeah, I know, same thing).
     */
    ap_setjmp(jmpbuffer);
#ifndef OS2
#ifdef SIGURG
    signal(SIGURG, timeout);
#endif
#endif
    signal(SIGPIPE, timeout);
    signal(SIGALRM, alrm_handler);

#ifdef OS2
/* Stop Ctrl-C/Ctrl-Break signals going to child processes */
    {
        unsigned long ulTimes;
        DosSetSignalExceptionFocus(0, &ulTimes);
    }
#endif

    while (1) {
	BUFF *conn_io;
	request_rec *r;

	/* Prepare to receive a SIGUSR1 due to graceful restart so that
	 * we can exit cleanly.  Since we're between connections right
	 * now it's the right time to exit, but we might be blocked in a
	 * system call when the graceful restart request is made. */
	usr1_just_die = 1;
	signal(SIGUSR1, usr1_handler);

	/*
	 * (Re)initialize this child to a pre-connection state.
	 */

	ap_kill_timeout(0);	/* Cancel any outstanding alarms. */
	current_conn = NULL;

	ap_clear_pool(ptrans);

	ap_sync_scoreboard_image();
	if (ap_scoreboard_image->global.exit_generation >= generation) {
	    clean_child_exit(0);
	}

#ifndef WIN32
	if ((ap_max_requests_per_child > 0
	     && ++requests_this_child >= ap_max_requests_per_child)) {
	    clean_child_exit(0);
	}
#else
	++requests_this_child;
#endif

	(void) ap_update_child_status(my_child_num, SERVER_READY, (request_rec *) NULL);

	/*
	 * Wait for an acceptable connection to arrive.
	 */

	/* Lock around "accept", if necessary */
	SAFE_ACCEPT(accept_mutex_on());

	for (;;) {
	    if (ap_listeners->next != ap_listeners) {
		/* more than one socket */
		memcpy(&main_fds, &listenfds, sizeof(fd_set));
		srv = ap_select(listenmaxfd + 1, &main_fds, NULL, NULL, NULL);

		if (srv < 0 && errno != EINTR) {
		    /* Single Unix documents select as returning errnos
		     * EBADF, EINTR, and EINVAL... and in none of those
		     * cases does it make sense to continue.  In fact
		     * on Linux 2.0.x we seem to end up with EFAULT
		     * occasionally, and we'd loop forever due to it.
		     */
		    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf, "select: (listen)");
		    clean_child_exit(1);
		}

		if (srv <= 0)
		    continue;

		lr = find_ready_listener(&main_fds);
		if (lr == NULL)
		    continue;
		sd = lr->fd;
	    }
	    else {
		/* only one socket, just pretend we did the other stuff */
		sd = ap_listeners->fd;
	    }

	    /* if we accept() something we don't want to die, so we have to
	     * defer the exit
	     */
	    deferred_die = 0;
	    usr1_just_die = 0;
	    for (;;) {
		clen = sizeof(sa_client);
		csd = accept(sd, &sa_client, &clen);
		if (csd >= 0 || errno != EINTR)
		    break;
		if (deferred_die) {
		    /* we didn't get a socket, and we were told to die */
		    clean_child_exit(0);
		}
	    }

	    if (csd >= 0)
		break;		/* We have a socket ready for reading */
	    else {

		/* Our old behaviour here was to continue after accept()
		 * errors.  But this leads us into lots of troubles
		 * because most of the errors are quite fatal.  For
		 * example, EMFILE can be caused by slow descriptor
		 * leaks (say in a 3rd party module, or libc).  It's
		 * foolish for us to continue after an EMFILE.  We also
		 * seem to tickle kernel bugs on some platforms which
		 * lead to never-ending loops here.  So it seems best
		 * to just exit in most cases.
		 */
                switch (errno) {
#ifdef EPROTO
		    /* EPROTO on certain older kernels really means
		     * ECONNABORTED, so we need to ignore it for them.
		     * See discussion in new-httpd archives nh.9701
		     * search for EPROTO.
		     *
		     * Also see nh.9603, search for EPROTO:
		     * There is potentially a bug in Solaris 2.x x<6,
		     * and other boxes that implement tcp sockets in
		     * userland (i.e. on top of STREAMS).  On these
		     * systems, EPROTO can actually result in a fatal
		     * loop.  See PR#981 for example.  It's hard to
		     * handle both uses of EPROTO.
		     */
                case EPROTO:
#endif
#ifdef ECONNABORTED
                case ECONNABORTED:
#endif
		    /* Linux generates the rest of these, other tcp
		     * stacks (i.e. bsd) tend to hide them behind
		     * getsockopt() interfaces.  They occur when
		     * the net goes sour or the client disconnects
		     * after the three-way handshake has been done
		     * in the kernel but before userland has picked
		     * up the socket.
		     */
#ifdef ECONNRESET
                case ECONNRESET:
#endif
#ifdef ETIMEDOUT
                case ETIMEDOUT:
#endif
#ifdef EHOSTUNREACH
		case EHOSTUNREACH:
#endif
#ifdef ENETUNREACH
		case ENETUNREACH:
#endif
                    break;

		default:
		    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
				"accept: (client socket)");
		    clean_child_exit(1);
		}
	    }

	    /* go around again, safe to die */
	    usr1_just_die = 1;
	    if (deferred_die) {
		/* ok maybe not, see ya later */
		clean_child_exit(0);
	    }
	    /* or maybe we missed a signal, you never know on systems
	     * without reliable signals
	     */
	    ap_sync_scoreboard_image();
	    if (ap_scoreboard_image->global.exit_generation >= generation) {
		clean_child_exit(0);
	    }
	}

	SAFE_ACCEPT(accept_mutex_off());	/* unlock after "accept" */

	/* We've got a socket, let's at least process one request off the
	 * socket before we accept a graceful restart request.
	 */
	signal(SIGUSR1, SIG_IGN);

	ap_note_cleanups_for_fd(ptrans, csd);

	/* protect various fd_sets */
	if (csd >= FD_SETSIZE) {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, NULL,
		"[csd] filedescriptor (%u) larger than FD_SETSIZE (%u) "
		"found, you probably need to rebuild Apache with a "
		"larger FD_SETSIZE", csd, FD_SETSIZE);
	    continue;
	}

	/*
	 * We now have a connection, so set it up with the appropriate
	 * socket options, file descriptors, and read/write buffers.
	 */

	clen = sizeof(sa_server);
	if (getsockname(csd, &sa_server, &clen) < 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf, "getsockname");
	    continue;
	}

	sock_disable_nagle(csd);

	(void) ap_update_child_status(my_child_num, SERVER_BUSY_READ,
				   (request_rec *) NULL);

	conn_io = ap_bcreate(ptrans, B_RDWR | B_SOCKET);

#ifdef B_SFIO
	(void) sfdisc(conn_io->sf_in, SF_POPDISC);
	sfdisc(conn_io->sf_in, bsfio_new(conn_io->pool, conn_io));
	sfsetbuf(conn_io->sf_in, NULL, 0);

	(void) sfdisc(conn_io->sf_out, SF_POPDISC);
	sfdisc(conn_io->sf_out, bsfio_new(conn_io->pool, conn_io));
	sfsetbuf(conn_io->sf_out, NULL, 0);
#endif

	dupped_csd = csd;
#if defined(NEED_DUPPED_CSD)
	if ((dupped_csd = dup(csd)) < 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
			"dup: couldn't duplicate csd");
	    dupped_csd = csd;	/* Oh well... */
	}
	ap_note_cleanups_for_fd(ptrans, dupped_csd);

	/* protect various fd_sets */
	if (dupped_csd >= FD_SETSIZE) {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, NULL,
		"[dupped_csd] filedescriptor (%u) larger than FD_SETSIZE (%u) "
		"found, you probably need to rebuild Apache with a "
		"larger FD_SETSIZE", dupped_csd, FD_SETSIZE);
	    continue;
	}
#endif
	ap_bpushfd(conn_io, csd, dupped_csd);

	current_conn = new_connection(ptrans, server_conf, conn_io,
				          (struct sockaddr_in *) &sa_client,
				          (struct sockaddr_in *) &sa_server,
				          my_child_num);

	/*
	 * Read and process each request found on our connection
	 * until no requests are left or we decide to close.
	 */

	while ((r = ap_read_request(current_conn)) != NULL) {

	    /* read_request_line has already done a
	     * signal (SIGUSR1, SIG_IGN);
	     */

	    (void) ap_update_child_status(my_child_num, SERVER_BUSY_WRITE, r);

	    /* process the request if it was read without error */

	    if (r->status == HTTP_OK)
		ap_process_request(r);

	    if(ap_extended_status)
		increment_counts(my_child_num, r);

	    if (!current_conn->keepalive || current_conn->aborted)
		break;

	    ap_destroy_pool(r->pool);
	    (void) ap_update_child_status(my_child_num, SERVER_BUSY_KEEPALIVE,
				       (request_rec *) NULL);

	    ap_sync_scoreboard_image();
	    if (ap_scoreboard_image->global.exit_generation >= generation) {
		ap_bclose(conn_io);
		clean_child_exit(0);
	    }

	    /* In case we get a graceful restart while we're blocked
	     * waiting for the request.
	     *
	     * XXX: This isn't perfect, we might actually read the
	     * request and then just die without saying anything to
	     * the client.  This can be fixed by using deferred_die
	     * but you have to teach buff.c about it so that it can handle
	     * the EINTR properly.
	     *
	     * In practice though browsers (have to) expect keepalive
	     * connections to close before receiving a response because
	     * of network latencies and server timeouts.
	     */
	    usr1_just_die = 1;
	    signal(SIGUSR1, usr1_handler);
	}

	/*
	 * Close the connection, being careful to send out whatever is still
	 * in our buffers.  If possible, try to avoid a hard close until the
	 * client has ACKed our FIN and/or has stopped sending us data.
	 */

#ifdef NO_LINGCLOSE
	ap_bclose(conn_io);	/* just close it */
#else
	if (r && r->connection
	    && !r->connection->aborted
	    && r->connection->client
	    && (r->connection->client->fd >= 0)) {

	    lingering_close(r);
	}
	else {
	    ap_bsetflag(conn_io, B_EOUT, 1);
	    ap_bclose(conn_io);
	}
#endif
    }
}

static int make_child(server_rec *s, int slot, time_t now)
{
    int pid;

    if (slot + 1 > max_daemons_limit) {
	max_daemons_limit = slot + 1;
    }

    if (one_process) {
	signal(SIGHUP, just_die);
	signal(SIGINT, just_die);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGTERM, just_die);
	child_main(slot);
    }

    /* avoid starvation */
    head_listener = head_listener->next;

    Explain1("Starting new child in slot %d", slot);
    (void) ap_update_child_status(slot, SERVER_STARTING, (request_rec *) NULL);

    if ((pid = fork()) == -1) {
	ap_log_error(APLOG_MARK, APLOG_ERR, s, "fork: Unable to fork new process");

	/* fork didn't succeed. Fix the scoreboard or else
	 * it will say SERVER_STARTING forever and ever
	 */
	(void) ap_update_child_status(slot, SERVER_DEAD, (request_rec *) NULL);

	/* In case system resources are maxxed out, we don't want
	   Apache running away with the CPU trying to fork over and
	   over and over again. */
	sleep(10);

	return -1;
    }

    if (!pid) {
#ifdef AIX_BIND_PROCESSOR
/* by default AIX binds to a single processor
 * this bit unbinds children which will then bind to another cpu
 */
#include <sys/processor.h>
	int status = bindprocessor(BINDPROCESS, (int)getpid(), 
				   PROCESSOR_CLASS_ANY);
	if (status != OK) {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, server_conf,
			"processor unbind failed %d", status);
	}
#endif
	RAISE_SIGSTOP(MAKE_CHILD);
	MONCONTROL(1);
	/* Disable the restart signal handlers and enable the just_die stuff.
	 * Note that since restart() just notes that a restart has been
	 * requested there's no race condition here.
	 */
	signal(SIGHUP, just_die);
	signal(SIGUSR1, just_die);
	signal(SIGTERM, just_die);
	child_main(slot);
    }

#ifdef OPTIMIZE_TIMEOUTS
    ap_scoreboard_image->parent[slot].last_rtime = now;
#endif
    ap_scoreboard_image->parent[slot].pid = pid;
#ifdef SCOREBOARD_FILE
    lseek(scoreboard_fd, XtOffsetOf(scoreboard, parent[slot]), 0);
    force_write(scoreboard_fd, &ap_scoreboard_image->parent[slot],
		sizeof(parent_score));
#endif

    return 0;
}


/* start up a bunch of children */
static void startup_children(int number_to_start)
{
    int i;
    time_t now = time(0);

    for (i = 0; number_to_start && i < ap_daemons_limit; ++i) {
	if (ap_scoreboard_image->servers[i].status != SERVER_DEAD) {
	    continue;
	}
	if (make_child(server_conf, i, now) < 0) {
	    break;
	}
	--number_to_start;
    }
}


/*
 * idle_spawn_rate is the number of children that will be spawned on the
 * next maintenance cycle if there aren't enough idle servers.  It is
 * doubled up to MAX_SPAWN_RATE, and reset only when a cycle goes by
 * without the need to spawn.
 */
static int idle_spawn_rate = 1;
#ifndef MAX_SPAWN_RATE
#define MAX_SPAWN_RATE	(32)
#endif
static int hold_off_on_exponential_spawning;

static void perform_idle_server_maintenance(void)
{
    int i;
    int to_kill;
    int idle_count;
    short_score *ss;
    time_t now = time(0);
    int free_length;
    int free_slots[MAX_SPAWN_RATE];
    int last_non_dead;
    int total_non_dead;

    /* initialize the free_list */
    free_length = 0;

    to_kill = -1;
    idle_count = 0;
    last_non_dead = -1;
    total_non_dead = 0;

    ap_sync_scoreboard_image();
    for (i = 0; i < ap_daemons_limit; ++i) {
	int status;

	if (i >= max_daemons_limit && free_length == idle_spawn_rate)
	    break;
	ss = &ap_scoreboard_image->servers[i];
	status = ss->status;
	if (status == SERVER_DEAD) {
	    /* try to keep children numbers as low as possible */
	    if (free_length < idle_spawn_rate) {
		free_slots[free_length] = i;
		++free_length;
	    }
	}
	else {
	    /* We consider a starting server as idle because we started it
	     * at least a cycle ago, and if it still hasn't finished starting
	     * then we're just going to swamp things worse by forking more.
	     * So we hopefully won't need to fork more if we count it.
	     * This depends on the ordering of SERVER_READY and SERVER_STARTING.
	     */
	    if (status <= SERVER_READY) {
		++ idle_count;
		/* always kill the highest numbered child if we have to...
		 * no really well thought out reason ... other than observing
		 * the server behaviour under linux where lower numbered children
		 * tend to service more hits (and hence are more likely to have
		 * their data in cpu caches).
		 */
		to_kill = i;
	    }

	    ++total_non_dead;
	    last_non_dead = i;
#ifdef OPTIMIZE_TIMEOUTS
	    if (ss->timeout_len) {
		/* if it's a live server, with a live timeout then
		 * start checking its timeout */
		parent_score *ps = &ap_scoreboard_image->parent[i];
		if (ss->cur_vtime != ps->last_vtime) {
		    /* it has made progress, so update its last_rtime,
		     * last_vtime */
		    ps->last_rtime = now;
		    ps->last_vtime = ss->cur_vtime;
		}
		else if (ps->last_rtime + ss->timeout_len < now) {
		    /* no progress, and the timeout length has been exceeded */
		    ss->timeout_len = 0;
		    kill(ps->pid, SIGALRM);
		}
	    }
#endif
	}
    }
    max_daemons_limit = last_non_dead + 1;
    if (idle_count > ap_daemons_max_free) {
	/* kill off one child... we use SIGUSR1 because that'll cause it to
	 * shut down gracefully, in case it happened to pick up a request
	 * while we were counting
	 */
	kill(ap_scoreboard_image->parent[to_kill].pid, SIGUSR1);
	idle_spawn_rate = 1;
    }
    else if (idle_count < ap_daemons_min_free) {
	/* terminate the free list */
	if (free_length == 0) {
	    /* only report this condition once */
	    static int reported = 0;

	    if (!reported) {
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_ERR, server_conf,
			    "server reached MaxClients setting, consider"
			    " raising the MaxClients setting");
		reported = 1;
	    }
	    idle_spawn_rate = 1;
	}
	else {
	    if (idle_spawn_rate >= 8) {
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, server_conf,
		    "server seems busy, (you may need "
		    "to increase StartServers, or Min/MaxSpareServers), "
		    "spawning %d children, there are %d idle, and "
		    "%d total children", idle_spawn_rate,
		    idle_count, total_non_dead);
	    }
	    for (i = 0; i < free_length; ++i) {
		make_child(server_conf, free_slots[i], now);
	    }
	    /* the next time around we want to spawn twice as many if this
	     * wasn't good enough, but not if we've just done a graceful
	     */
	    if (hold_off_on_exponential_spawning) {
		--hold_off_on_exponential_spawning;
	    }
	    else if (idle_spawn_rate < MAX_SPAWN_RATE) {
		idle_spawn_rate *= 2;
	    }
	}
    }
    else {
	idle_spawn_rate = 1;
    }
}


static void process_child_status(int pid, ap_wait_t status)
{
    /* Child died... if it died due to a fatal error,
	* we should simply bail out.
	*/
    if ((WIFEXITED(status)) &&
	WEXITSTATUS(status) == APEXIT_CHILDFATAL) {
	ap_log_error(APLOG_MARK, APLOG_ALERT|APLOG_NOERRNO, server_conf,
			"Child %d returned a Fatal error... \n"
			"Apache is exiting!",
			pid);
	exit(APEXIT_CHILDFATAL);
    }
    if (WIFSIGNALED(status)) {
	switch (WTERMSIG(status)) {
	case SIGTERM:
	case SIGHUP:
	case SIGUSR1:
	case SIGKILL:
	    break;
	default:
#ifdef SYS_SIGLIST
#ifdef WCOREDUMP
	    if (WCOREDUMP(status)) {
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE,
		    server_conf,
		    "httpd: child pid %d exit signal %s (%d), "
		    "possible coredump in %s",
		    pid, (WTERMSIG(status) >= NumSIG) ? "" : 
		    SYS_SIGLIST[WTERMSIG(status)], WTERMSIG(status),
		    ap_coredump_dir);
	    }
	    else {
#endif
		ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE,
		    server_conf,
		    "httpd: child pid %d exit signal %s (%d)",
		    pid, SYS_SIGLIST[WTERMSIG(status)], WTERMSIG(status));
#ifdef WCOREDUMP
	    }
#endif
#else
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE,
		server_conf,
		"httpd: child pid %d exit signal %d",
		pid, WTERMSIG(status));
#endif
	}
    }
}


/*****************************************************************
 * Executive routines.
 */

#ifndef STANDALONE_MAIN
#define STANDALONE_MAIN standalone_main

static void standalone_main(int argc, char **argv)
{
    int remaining_children_to_start;

#ifdef OS2
    printf("%s \n", ap_get_server_version());
#endif

    ap_standalone = 1;

    is_graceful = 0;
    ++generation;

    if (!one_process) {
	detach();
    }
    else {
	MONCONTROL(1);
    }

    my_pid = getpid();

    do {
	copy_listeners(pconf);
	if (!is_graceful) {
	    ap_restart_time = time(NULL);
	}
#ifdef SCOREBOARD_FILE
	else if (scoreboard_fd != -1) {
	    ap_kill_cleanup(pconf, NULL, cleanup_scoreboard_file);
	    ap_kill_cleanups_for_fd(pconf, scoreboard_fd);
	}
#endif
	ap_clear_pool(pconf);
	ptrans = ap_make_sub_pool(pconf);

	server_conf = ap_read_config(pconf, ptrans, ap_server_confname);
	setup_listeners(pconf);
	ap_open_logs(server_conf, pconf);
	ap_log_pid(pconf, ap_pid_fname);
	ap_set_version();	/* create our server_version string */
	ap_init_modules(pconf, server_conf);
	version_locked++;	/* no more changes to server_version */
	SAFE_ACCEPT(accept_mutex_init(pconf));
	if (!is_graceful) {
	    reinit_scoreboard(pconf);
	}
#ifdef SCOREBOARD_FILE
	else {
	    ap_scoreboard_fname = ap_server_root_relative(pconf, ap_scoreboard_fname);
	    ap_note_cleanups_for_fd(pconf, scoreboard_fd);
	}
#endif

	set_signals();

	if (ap_daemons_max_free < ap_daemons_min_free + 1)	/* Don't thrash... */
	    ap_daemons_max_free = ap_daemons_min_free + 1;

	/* If we're doing a graceful_restart then we're going to see a lot
	 * of children exiting immediately when we get into the main loop
	 * below (because we just sent them SIGUSR1).  This happens pretty
	 * rapidly... and for each one that exits we'll start a new one until
	 * we reach at least daemons_min_free.  But we may be permitted to
	 * start more than that, so we'll just keep track of how many we're
	 * supposed to start up without the 1 second penalty between each fork.
	 */
	remaining_children_to_start = ap_daemons_to_start;
	if (remaining_children_to_start > ap_daemons_limit) {
	    remaining_children_to_start = ap_daemons_limit;
	}
	if (!is_graceful) {
	    startup_children(remaining_children_to_start);
	    remaining_children_to_start = 0;
	}
	else {
	    /* give the system some time to recover before kicking into
	     * exponential mode */
	    hold_off_on_exponential_spawning = 10;
	}

	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
		    "%s configured -- resuming normal operations",
		    ap_get_server_version());
	if (ap_suexec_enabled) {
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
		         "suEXEC mechanism enabled (wrapper: %s)", SUEXEC_BIN);
	}
	ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO, server_conf,
		    "Server built: %s", ap_get_server_built());
	restart_pending = shutdown_pending = 0;

	while (!restart_pending && !shutdown_pending) {
	    int child_slot;
	    ap_wait_t status;
	    int pid = wait_or_timeout(&status);

	    /* XXX: if it takes longer than 1 second for all our children
	     * to start up and get into IDLE state then we may spawn an
	     * extra child
	     */
	    if (pid >= 0) {
		process_child_status(pid, status);
		/* non-fatal death... note that it's gone in the scoreboard. */
		ap_sync_scoreboard_image();
		child_slot = find_child_by_pid(pid);
		Explain2("Reaping child %d slot %d", pid, child_slot);
		if (child_slot >= 0) {
		    (void) ap_update_child_status(child_slot, SERVER_DEAD,
					       (request_rec *) NULL);
		    if (remaining_children_to_start
			&& child_slot < ap_daemons_limit) {
			/* we're still doing a 1-for-1 replacement of dead
			 * children with new children
			 */
			make_child(server_conf, child_slot, time(0));
			--remaining_children_to_start;
		    }
#ifndef NO_OTHER_CHILD
		}
		else if (reap_other_child(pid, status) == 0) {
		    /* handled */
#endif
		}
		else if (is_graceful) {
		    /* Great, we've probably just lost a slot in the
		     * scoreboard.  Somehow we don't know about this
		     * child.
		     */
		    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_WARNING, server_conf,
				"long lost child came home! (pid %d)", pid);
		}
		/* Don't perform idle maintenance when a child dies,
		 * only do it when there's a timeout.  Remember only a
		 * finite number of children can die, and it's pretty
		 * pathological for a lot to die suddenly.
		 */
		continue;
	    }
	    else if (remaining_children_to_start) {
		/* we hit a 1 second timeout in which none of the previous
		 * generation of children needed to be reaped... so assume
		 * they're all done, and pick up the slack if any is left.
		 */
		startup_children(remaining_children_to_start);
		remaining_children_to_start = 0;
		/* In any event we really shouldn't do the code below because
		 * few of the servers we just started are in the IDLE state
		 * yet, so we'd mistakenly create an extra server.
		 */
		continue;
	    }

	    perform_idle_server_maintenance();
	}

	if (shutdown_pending) {
	    /* Time to gracefully shut down:
	     * Kill child processes, tell them to call child_exit, etc...
	     */
	    if (ap_killpg(pgrp, SIGTERM) < 0) {
		ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "killpg SIGTERM");
	    }
	    reclaim_child_processes(1);		/* Start with SIGTERM */

	    /* cleanup pid file on normal shutdown */
	    {
		const char *pidfile = NULL;
		pidfile = ap_server_root_relative (pconf, ap_pid_fname);
		if ( pidfile != NULL && unlink(pidfile) == 0)
		    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO,
				 server_conf,
				 "httpd: removed PID file %s (pid=%ld)",
				 pidfile, (long)getpid());
	    }

	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
			"httpd: caught SIGTERM, shutting down");
	    clean_parent_exit(0);
	}

	/* we've been told to restart */
	signal(SIGHUP, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);

	if (one_process) {
	    /* not worth thinking about */
	    clean_parent_exit(0);
	}

	if (is_graceful) {
#ifndef SCOREBOARD_FILE
	    int i;
#endif

	    /* USE WITH CAUTION:  Graceful restarts are not known to work
	     * in various configurations on the architectures we support. */
	    ap_scoreboard_image->global.exit_generation = generation;
	    update_scoreboard_global();

	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
			"SIGUSR1 received.  Doing graceful restart");

	    /* kill off the idle ones */
	    if (ap_killpg(pgrp, SIGUSR1) < 0) {
		ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "killpg SIGUSR1");
	    }
#ifndef SCOREBOARD_FILE
	    /* This is mostly for debugging... so that we know what is still
	     * gracefully dealing with existing request.  But we can't really
	     * do it if we're in a SCOREBOARD_FILE because it'll cause
	     * corruption too easily.
	     */
	    ap_sync_scoreboard_image();
	    for (i = 0; i < ap_daemons_limit; ++i) {
		if (ap_scoreboard_image->servers[i].status != SERVER_DEAD) {
		    ap_scoreboard_image->servers[i].status = SERVER_GRACEFUL;
		}
	    }
#endif
	}
	else {
	    /* Kill 'em off */
	    if (ap_killpg(pgrp, SIGHUP) < 0) {
		ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "killpg SIGHUP");
	    }
	    reclaim_child_processes(0);		/* Not when just starting up */
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, server_conf,
			"SIGHUP received.  Attempting to restart");
	}
	++generation;
    } while (restart_pending);

    /*add_common_vars(NULL);*/
}				/* standalone_main */
#else
/* prototype */
void STANDALONE_MAIN(int argc, char **argv);
#endif /* STANDALONE_MAIN */

extern char *optarg;
extern int optind;

int REALMAIN(int argc, char *argv[])
{
    int c;
    int configtestonly = 0;

#ifdef SecureWare
    if (set_auth_parameters(argc, argv) < 0)
	perror("set_auth_parameters");
    if (getluid() < 0)
	if (setluid(getuid()) < 0)
	    perror("setluid");
    if (setreuid(0, 0) < 0)
	perror("setreuid");
#endif

#ifdef SOCKS
    SOCKSinit(argv[0]);
#endif

    MONCONTROL(0);

    common_init();
    
    ap_server_argv0 = argv[0];
    ap_cpystrn(ap_server_root, HTTPD_ROOT, sizeof(ap_server_root));
    ap_cpystrn(ap_server_confname, SERVER_CONFIG_FILE, sizeof(ap_server_confname));

    ap_setup_prelinked_modules();

    while ((c = getopt(argc, argv,
				    "D:C:c:Xd:f:vVhlL:St"
#ifdef DEBUG_SIGSTOP
				    "Z:"
#endif
			)) != -1) {
	char **new;
	switch (c) {
	case 'c':
	    new = (char **)ap_push_array(ap_server_post_read_config);
	    *new = ap_pstrdup(pcommands, optarg);
	    break;
	case 'C':
	    new = (char **)ap_push_array(ap_server_pre_read_config);
	    *new = ap_pstrdup(pcommands, optarg);
	    break;
	case 'D':
	    new = (char **)ap_push_array(ap_server_config_defines);
	    *new = ap_pstrdup(pcommands, optarg);
	    break;
	case 'd':
	    ap_cpystrn(ap_server_root, optarg, sizeof(ap_server_root));
	    break;
	case 'f':
	    ap_cpystrn(ap_server_confname, optarg, sizeof(ap_server_confname));
	    break;
	case 'v':
	    ap_set_version();
	    printf("Server version: %s\n", ap_get_server_version());
	    printf("Server built:   %s\n", ap_get_server_built());
	    exit(0);
	case 'V':
	    ap_set_version();
	    show_compile_settings();
	    exit(0);
	case 'h':
	    ap_show_directives();
	    exit(0);
	case 'l':
	    ap_show_modules();
	    exit(0);
	case 'X':
	    ++one_process;	/* Weird debugging mode. */
	    break;
#ifdef DEBUG_SIGSTOP
	case 'Z':
	    raise_sigstop_flags = atoi(optarg);
	    break;
#endif
#ifdef SHARED_CORE
	case 'L':
	    /* just ignore this option here, because it has only
	     * effect when SHARED_CORE is used and then it was
	     * already handled in the Shared Core Bootstrap
	     * program.
	     */
	    break;
#endif
	case 'S':
	    ap_dump_settings = 1;
	    break;
	case 't':
	    configtestonly = 1;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }

    ap_suexec_enabled = init_suexec();
    server_conf = ap_read_config(pconf, ptrans, ap_server_confname);

    if (configtestonly) {
        fprintf(stderr, "Syntax OK\n");
        exit(0);
    }

    child_timeouts = !ap_standalone || one_process;

    if (ap_standalone) {
	ap_open_logs(server_conf, pconf);
	ap_set_version();
	ap_init_modules(pconf, server_conf);
	version_locked++;
	STANDALONE_MAIN(argc, argv);
    }
    else {
	conn_rec *conn;
	request_rec *r;
	struct sockaddr sa_server, sa_client;
	BUFF *cio;
	NET_SIZE_T l;

	/* Yes this is called twice. */
	ap_init_modules(pconf, server_conf);
	ap_open_logs(server_conf, pconf);
	ap_init_modules(pconf, server_conf);
	set_group_privs();

#ifdef MPE
	/* Only try to switch if we're running as MANAGER.SYS */
	if (geteuid() == 1 && ap_user_id > 1) {
	    GETPRIVMODE();
	    if (setuid(ap_user_id) == -1) {
		GETUSERMODE();
		ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			    "setuid: unable to change uid");
		exit(1);
	    }
	    GETUSERMODE();
	}
#else
	/* Only try to switch if we're running as root */
	if (!geteuid() && setuid(ap_user_id) == -1) {
	    ap_log_error(APLOG_MARK, APLOG_ALERT, server_conf,
			"setuid: unable to change uid");
	    exit(1);
	}
#endif
	if (ap_setjmp(jmpbuffer)) {
	    exit(0);
	}

	l = sizeof(sa_client);
	if ((getpeername(fileno(stdin), &sa_client, &l)) < 0) {
/* get peername will fail if the input isn't a socket */
	    perror("getpeername");
	    memset(&sa_client, '\0', sizeof(sa_client));
	}

	l = sizeof(sa_server);
	if (getsockname(fileno(stdin), &sa_server, &l) < 0) {
	    perror("getsockname");
	    fprintf(stderr, "Error getting local address\n");
	    exit(1);
	}
	server_conf->port = ntohs(((struct sockaddr_in *) &sa_server)->sin_port);
	cio = ap_bcreate(ptrans, B_RDWR | B_SOCKET);
#ifdef MPE
/* HP MPE 5.5 inetd only passes the incoming socket as stdin (fd 0), whereas
   HPUX inetd passes the incoming socket as stdin (fd 0) and stdout (fd 1).
   Go figure.  SR 5003355016 has been submitted to request that the existing
   functionality be documented, and then to enhance the functionality to be
   like HPUX. */

	cio->fd = fileno(stdin);
#else
	cio->fd = fileno(stdout);
#endif
	cio->fd_in = fileno(stdin);
	conn = new_connection(ptrans, server_conf, cio,
			          (struct sockaddr_in *) &sa_client,
			          (struct sockaddr_in *) &sa_server, -1);

	while ((r = ap_read_request(conn)) != NULL) {

	    if (r->status == HTTP_OK)
		ap_process_request(r);

	    if (!conn->keepalive || conn->aborted)
		break;

	    ap_destroy_pool(r->pool);
	}

	ap_bclose(cio);
    }
    exit(0);
}

#else /* ndef MULTITHREAD */


/**********************************************************************
 * Multithreaded implementation
 *
 * This code is fairly specific to Win32.
 *
 * The model used to handle requests is a set of threads. One "main"
 * thread listens for new requests. When something becomes
 * available, it does a select and places the newly available socket
 * onto a list of "jobs" (add_job()). Then any one of a fixed number
 * of "worker" threads takes the top job off the job list with
 * remove_job() and handles that connection to completion. After
 * the connection has finished the thread is free to take another
 * job from the job list.
 *
 * In the code, the "main" thread is running within the worker_main()
 * function. The first thing this function does is create the
 * worker threads, which operate in the child_sub_main() function. The
 * main thread then goes into a loop within worker_main() where they
 * do a select() on the listening sockets. The select times out once
 * per second so that the thread can check for an "exit" signal
 * from the parent process (see below). If this signal is set, the 
 * thread can exit, but only after it has accepted all incoming
 * connections already in the listen queue (since Win32 appears
 * to through away listened but unaccepted connections when a 
 * process dies).
 *
 * Because the main and worker threads exist within a single process
 * they are vulnerable to crashes or memory leaks (crashes can also
 * be caused within modules, of course). There also needs to be a 
 * mechanism to perform restarts and shutdowns. This is done by
 * creating the main & worker threads within a subprocess. A
 * main process (the "parent process") creates one (or more) 
 * processes to do the work, then the parent sits around waiting
 * for the working process to die, in which case it starts a new
 * one. The parent process also handles restarts (by creating
 * a new working process then signalling the previous working process 
 * exit ) and shutdowns (by signalling the working process to exit).
 * The parent process operates within the master_main() function. This
 * process also handles requests from the service manager (NT only).
 *
 * Signalling between the parent and working process uses a Win32
 * event. Each child has a unique name for the event, which is
 * passed to it with the -c argument when the child is spawned. The
 * parent sets (signals) this event to tell the child to die.
 * At present all children do a graceful die - they finish all
 * current jobs _and_ empty the listen queue before they exit.
 * A non-graceful die would need a second event.
 *
 * The code below starts with functions at the lowest level -
 * worker threads, and works up to the top level - the main()
 * function of the parent process.
 *
 * The scoreboard (in process memory) contains details of the worker
 * threads (within the active working process). There is no shared
 * "scoreboard" between processes, since only one is ever active
 * at once (or at most, two, when one has been told to shutdown but
 * is processes outstanding requests, and a new one has been started).
 * This is controlled by a "start_mutex" which ensures only one working
 * process is active at once.
 **********************************************************************/

/* The code protected by #ifdef UNGRACEFUL_RESTARTS/#endif sections
 * could implement a sort-of ungraceful restart for Win32. instead of
 * graceful restarts. 
 *
 * However it does not work too well because it does not intercept a
 * connection already in progress (in child_sub_main()). We'd have to
 * get that to poll on the exit event. 
 */

/*
 * Definition of jobs, shared by main and worker threads.
 */

typedef struct joblist_s {
    struct joblist_s *next;
    int sock;
} joblist;

/*
 * Globals common to main and worker threads. This structure is not
 * used by the parent process.
 */

typedef struct globals_s {
#ifdef UNGRACEFUL_RESTART
    HANDLE thread_exit_event;
#else
    int exit_now;
#endif
    semaphore *jobsemaphore;
    joblist *jobhead;
    joblist *jobtail;
    mutex *jobmutex;
    int jobcount;
} globals;

globals allowed_globals =
{0, NULL, NULL, NULL, NULL, 0};

/*
 * add_job()/remove_job() - add or remove an accepted socket from the
 * list of sockets connected to clients. allowed_globals.jobmutex protects
 * against multiple concurrent access to the linked list of jobs.
 */

void add_job(int sock)
{
    joblist *new_job;

    ap_assert(allowed_globals.jobmutex);
    /* TODO: If too many jobs in queue, sleep, check for problems */
    ap_acquire_mutex(allowed_globals.jobmutex);
    new_job = (joblist *) malloc(sizeof(joblist));
    new_job->next = NULL;
    new_job->sock = sock;
    if (allowed_globals.jobtail != NULL)
	allowed_globals.jobtail->next = new_job;
    allowed_globals.jobtail = new_job;
    if (!allowed_globals.jobhead)
	allowed_globals.jobhead = new_job;
    allowed_globals.jobcount++;
    release_semaphore(allowed_globals.jobsemaphore);
    ap_release_mutex(allowed_globals.jobmutex);
}

int remove_job(void)
{
    joblist *job;
    int sock;

#ifdef UNGRACEFUL_RESTART
    HANDLE hObjects[2];
    int rv;

    hObjects[0] = allowed_globals.jobsemaphore;
    hObjects[1] = allowed_globals.thread_exit_event;

    rv = WaitForMultipleObjects(2, hObjects, FALSE, INFINITE);
    ap_assert(rv != WAIT_FAILED);
    if (rv == WAIT_OBJECT_0 + 1) {
	/* thread_exit_now */
	APD1("thread got exit now event");
	return -1;
    }
    /* must be semaphore */
#else
    acquire_semaphore(allowed_globals.jobsemaphore);
#endif
    ap_assert(allowed_globals.jobmutex);

#ifdef UNGRACEFUL_RESTART
    if (!allowed_globals.jobhead) {
#else
    ap_acquire_mutex(allowed_globals.jobmutex);
    if (allowed_globals.exit_now && !allowed_globals.jobhead) {
#endif
	ap_release_mutex(allowed_globals.jobmutex);
	return (-1);
    }
    job = allowed_globals.jobhead;
    ap_assert(job);
    allowed_globals.jobhead = job->next;
    if (allowed_globals.jobhead == NULL)
	allowed_globals.jobtail = NULL;
    ap_release_mutex(allowed_globals.jobmutex);
    sock = job->sock;
    free(job);
    return (sock);
}

/*
 * child_sub_main() - this is the main loop for the worker threads
 *
 * Each thread runs within this function. They wait within remove_job()
 * for a job to become available, then handle all the requests on that
 * connection until it is closed, then return to remove_job().
 *
 * The worker thread will exit when it removes a job which contains
 * socket number -1. This provides a graceful thread exit, since
 * it will never exit during a connection.
 *
 * This code in this function is basically equivalent to the child_main()
 * from the multi-process (Unix) environment, except that we
 *
 *  - do not call child_init_modules (child init API phase)
 *  - block in remove_job, and when unblocked we have an already
 *    accepted socket, instead of blocking on a mutex or select().
 */

static void child_sub_main(int child_num)
{
    NET_SIZE_T clen;
    struct sockaddr sa_server;
    struct sockaddr sa_client;
    pool *ptrans;
    int requests_this_child = 0;
    int csd = -1;
    int dupped_csd = -1;
    int srv = 0;

    ptrans = ap_make_sub_pool(pconf);

    (void) ap_update_child_status(child_num, SERVER_READY, (request_rec *) NULL);

    /*
     * Setup the jump buffers so that we can return here after
     * a signal or a timeout (yeah, I know, same thing).
     */
#if defined(USE_LONGJMP)
    setjmp(jmpbuffer);
#else
    sigsetjmp(jmpbuffer, 1);
#endif
#ifdef SIGURG
    signal(SIGURG, timeout);
#endif

    while (1) {
	BUFF *conn_io;
	request_rec *r;

	/*
	 * (Re)initialize this child to a pre-connection state.
	 */

	ap_set_callback_and_alarm(NULL, 0);	/* Cancel any outstanding alarms. */
	timeout_req = NULL;	/* No request in progress */
	current_conn = NULL;
#ifdef SIGPIPE
	signal(SIGPIPE, timeout);
#endif

	ap_clear_pool(ptrans);

	(void) ap_update_child_status(child_num, SERVER_READY, (request_rec *) NULL);

	/* Get job from the job list. This will block until a job is ready.
	 * If -1 is returned then the main thread wants us to exit.
	 */
	csd = remove_job();
	if (csd == -1)
	    break;		/* time to exit */
	requests_this_child++;

	ap_note_cleanups_for_socket(ptrans, csd);

	/*
	 * We now have a connection, so set it up with the appropriate
	 * socket options, file descriptors, and read/write buffers.
	 */

	clen = sizeof(sa_server);
	if (getsockname(csd, &sa_server, &clen) < 0) {
	    ap_log_error(APLOG_MARK, APLOG_WARNING, server_conf, "getsockname");
	    continue;
	}
	clen = sizeof(sa_client);
	if ((getpeername(csd, &sa_client, &clen)) < 0) {
	    /* get peername will fail if the input isn't a socket */
	    perror("getpeername");
	    memset(&sa_client, '\0', sizeof(sa_client));
	}

	sock_disable_nagle(csd);

	(void) ap_update_child_status(child_num, SERVER_BUSY_READ,
				   (request_rec *) NULL);

	conn_io = ap_bcreate(ptrans, B_RDWR | B_SOCKET);
	dupped_csd = csd;
#if defined(NEED_DUPPED_CSD)
	if ((dupped_csd = dup(csd)) < 0) {
	    ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
			"dup: couldn't duplicate csd");
	    dupped_csd = csd;	/* Oh well... */
	}
	ap_note_cleanups_for_socket(ptrans, dupped_csd);
#endif
	ap_bpushfd(conn_io, csd, dupped_csd);

	current_conn = new_connection(ptrans, server_conf, conn_io,
				          (struct sockaddr_in *) &sa_client,
				          (struct sockaddr_in *) &sa_server,
				          child_num);

	/*
	 * Read and process each request found on our connection
	 * until no requests are left or we decide to close.
	 */

	while ((r = ap_read_request(current_conn)) != NULL) {
	    (void) ap_update_child_status(child_num, SERVER_BUSY_WRITE, r);

	    if (r->status == HTTP_OK)
		ap_process_request(r);

	    if (ap_extended_status)
		increment_counts(child_num, r);

	    if (!current_conn->keepalive || current_conn->aborted)
		break;

	    ap_destroy_pool(r->pool);
	    (void) ap_update_child_status(child_num, SERVER_BUSY_KEEPALIVE,
				       (request_rec *) NULL);

	    ap_sync_scoreboard_image();
	}

	/*
	 * Close the connection, being careful to send out whatever is still
	 * in our buffers.  If possible, try to avoid a hard close until the
	 * client has ACKed our FIN and/or has stopped sending us data.
	 */
	ap_kill_cleanups_for_socket(ptrans, csd);

#ifdef NO_LINGCLOSE
	ap_bclose(conn_io);	/* just close it */
#else
	if (r && r->connection
	    && !r->connection->aborted
	    && r->connection->client
	    && (r->connection->client->fd >= 0)) {

	    lingering_close(r);
	}
	else {
	    ap_bsetflag(conn_io, B_EOUT, 1);
	    ap_bclose(conn_io);
	}
#endif
    }
    ap_destroy_pool(ptrans);
    (void) ap_update_child_status(child_num, SERVER_DEAD, NULL);
}


void child_main(int child_num_arg)
{
    /*
     * Only reason for this function, is to pass in
     * arguments to child_sub_main() on its stack so
     * that longjump doesn't try to corrupt its local
     * variables and I don't need to make those
     * damn variables static/global
     */
    child_sub_main(child_num_arg);
}



void cleanup_thread(thread **handles, int *thread_cnt, int thread_to_clean)
{
    int i;

    free_thread(handles[thread_to_clean]);
    for (i = thread_to_clean; i < ((*thread_cnt) - 1); i++)
	handles[i] = handles[i + 1];
    (*thread_cnt)--;
}
#ifdef WIN32
/*
 * The Win32 call WaitForMultipleObjects will only allow you to wait for 
 * a maximum of MAXIMUM_WAIT_OBJECTS (current 64).  Since the threading 
 * model in the multithreaded version of apache wants to use this call, 
 * we are restricted to a maximum of 64 threads.  This is a simplistic 
 * routine that will increase this size.
 */
DWORD wait_for_many_objects(DWORD nCount, CONST HANDLE *lpHandles, 
                            DWORD dwSeconds)
{
    time_t tStopTime;
    DWORD dwRet = WAIT_TIMEOUT;
    DWORD dwIndex=0;
    BOOL bFirst = TRUE;
  
    tStopTime = time(NULL) + dwSeconds;
  
    do {
        if (!bFirst)
            Sleep(1000);
        else
            bFirst = FALSE;
          
        for (dwIndex = 0; dwIndex * MAXIMUM_WAIT_OBJECTS < nCount; dwIndex++) {
            dwRet = WaitForMultipleObjects(
                        min(MAXIMUM_WAIT_OBJECTS, 
                            nCount - (dwIndex * MAXIMUM_WAIT_OBJECTS)),
                        lpHandles + (dwIndex * MAXIMUM_WAIT_OBJECTS), 
                        0, 0);
                                           
            if (dwRet != WAIT_TIMEOUT) {                                          
              break;
            }
        }
    } while((time(NULL) < tStopTime) && (dwRet == WAIT_TIMEOUT));
    
    return dwRet;
}
#endif
/*****************************************************************
 * Executive routines.
 */

extern void main_control_server(void *); /* in hellop.c */

event *exit_event;
mutex *start_mutex;

#define MAX_SELECT_ERRORS 100

void worker_main(void)
{
    /*
     * I am writing this stuff specifically for NT.
     * have pulled out a lot of the restart and
     * graceful restart stuff, because that is only
     * useful on Unix (not sure it even makes sense
     * in a multi-threaded env.
     */
    int nthreads;
    fd_set main_fds;
    int srv;
    int clen;
    int csd;
    struct sockaddr_in sa_client;
    int total_jobs = 0;
    thread **child_handles;
    int rv;
    time_t end_time;
    int i;
    struct timeval tv;
    int wait_time = 1;
    int start_exit = 0;
    int start_mutex_released = 0;
    int max_jobs_per_exe;
    int max_jobs_after_exit_request;
    HANDLE hObjects[2];
    int count_select_errors = 0;
    pool *pchild;

    pchild = ap_make_sub_pool(pconf);

    ap_standalone = 1;
    sd = -1;
    nthreads = ap_threads_per_child;
    max_jobs_after_exit_request = ap_excess_requests_per_child;
    max_jobs_per_exe = ap_max_requests_per_child;
    if (nthreads <= 0)
	nthreads = 40;
    if (max_jobs_per_exe <= 0)
	max_jobs_per_exe = 0;
    if (max_jobs_after_exit_request <= 0)
	max_jobs_after_exit_request = max_jobs_per_exe / 10;

    if (!one_process)
	detach();

    my_pid = getpid();

    ++generation;

    copy_listeners(pconf);
    ap_restart_time = time(NULL);

    reinit_scoreboard(pconf);

    //ap_acquire_mutex(start_mutex);
    hObjects[0] = (HANDLE)start_mutex;
    hObjects[1] = (HANDLE)exit_event;
    rv = WaitForMultipleObjects(2, hObjects, FALSE, INFINITE);
    if (rv == WAIT_FAILED) {
	ap_log_error(APLOG_MARK,APLOG_ERR|APLOG_WIN32ERROR, server_conf,
	    "Waiting for start_mutex or exit_event -- process will exit");

	ap_destroy_pool(pchild);
	cleanup_scoreboard();
	exit(0);
    }
    if (rv == WAIT_OBJECT_0 + 1) {
	/* exit event signalled - exit now */
	ap_destroy_pool(pchild);
	cleanup_scoreboard();
	exit(0);
    }
    /* start_mutex obtained, continue into the select() loop */

    setup_listeners(pconf);
    if (listenmaxfd == -1) {
	/* Help, no sockets were made, better log something and exit */
	ap_log_error(APLOG_MARK, APLOG_CRIT|APLOG_NOERRNO, NULL,
		    "No sockets were created for listening");

	signal_parent();	/* tell parent to die */

	ap_destroy_pool(pchild);
	cleanup_scoreboard();
	exit(0);
    }
    set_signals();

    /*
     * - Initialize allowed_globals
     * - Create the thread table
     * - Spawn off threads
     * - Create listen socket set (done above)
     * - loop {
     *       wait for request
     *       create new job
     *   } while (!time to exit)
     * - Close all listeners
     * - Wait for all threads to complete
     * - Exit
     */

    ap_child_init_modules(pconf, server_conf);

    allowed_globals.jobsemaphore = create_semaphore(0);
    allowed_globals.jobmutex = ap_create_mutex(NULL);

    /* spawn off the threads */
    child_handles = (thread *) alloca(nthreads * sizeof(int));
    {
	int i;

	for (i = 0; i < nthreads; i++) {
	    child_handles[i] = create_thread((void (*)(void *)) child_main, (void *) i);
	}
	if (nthreads > max_daemons_limit) {
	    max_daemons_limit = nthreads;
	}
    }

    while (1) {
#if SEVERELY_VERBOSE
	APD4("child PID %d: thread_main total_jobs=%d start_exit=%d",
		my_pid, total_jobs, start_exit);
#endif
	if ((max_jobs_per_exe && (total_jobs > max_jobs_per_exe) && !start_exit)) {
	    start_exit = 1;
	    wait_time = 1;
	    ap_release_mutex(start_mutex);
	    start_mutex_released = 1;
	    APD2("process PID %d: start mutex released\n", my_pid);
	}
	if (!start_exit) {
	    rv = WaitForSingleObject(exit_event, 0);
	    ap_assert((rv == WAIT_TIMEOUT) || (rv == WAIT_OBJECT_0));
	    if (rv == WAIT_OBJECT_0) {
		APD1("child: exit event signalled, exiting");
		start_exit = 1;
		/* Lets not break yet - we may have threads to clean up */
		/* break;*/
	    }
	    rv = wait_for_many_objects(nthreads, child_handles, 0);
	    ap_assert(rv != WAIT_FAILED);
	    if (rv != WAIT_TIMEOUT) {
		rv = rv - WAIT_OBJECT_0;
		ap_assert((rv >= 0) && (rv < nthreads));
		cleanup_thread(child_handles, &nthreads, rv);
		break;
	    }
	}

#if 0
	/* Um, this made us exit before all the connections in our
	 * listen queue were dealt with. 
	 */
	if (start_exit && max_jobs_after_exit_request && (count_down-- < 0))
	    break;
#endif
	tv.tv_sec = wait_time;
	tv.tv_usec = 0;

	memcpy(&main_fds, &listenfds, sizeof(fd_set));
	srv = ap_select(listenmaxfd + 1, &main_fds, NULL, NULL, &tv);
#ifdef WIN32
	if (srv == SOCKET_ERROR) {
	    /* Map the Win32 error into a standard Unix error condition */
	    errno = WSAGetLastError();
	    srv = -1;
	}
#endif /* WIN32 */

	if (srv < 0) {
	    /* Error occurred - if EINTR, loop around with problem */
	    if (errno != EINTR) {
		/* A "real" error occurred, log it and increment the count of
		 * select errors. This count is used to ensure we don't go into
		 * a busy loop of continuour errors.
		 */
		ap_log_error(APLOG_MARK, APLOG_ERR, server_conf, "select: (listen)");
		count_select_errors++;
		if (count_select_errors > MAX_SELECT_ERRORS) {
		    ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_NOERRNO, server_conf,
			"Too many errors in select loop. Child process exiting.");
		    break;
		}
	    }
	    continue;
	}
	count_select_errors = 0;    /* reset count of errors */
	if (srv == 0) {
	    if (start_exit)
		break;
	    else
		continue;
	}

	{
	    listen_rec *lr;

	    lr = find_ready_listener(&main_fds);
	    if (lr != NULL) {
		sd = lr->fd;
	    }
	}
	do {
	    clen = sizeof(sa_client);
	    csd = accept(sd, (struct sockaddr *) &sa_client, &clen);
#ifdef WIN32
	    if (csd == INVALID_SOCKET) {
		csd = -1;
		errno = WSAGetLastError();
	    }
#endif /* WIN32 */
	} while (csd < 0 && errno == EINTR);

	if (csd < 0) {
#if defined(EPROTO) && defined(ECONNABORTED)
	    if ((errno != EPROTO) && (errno != ECONNABORTED))
#elif defined(EPROTO)
	    if (errno != EPROTO)
#elif defined(ECONNABORTED)
	    if (errno != ECONNABORTED)
#endif
		ap_log_error(APLOG_MARK, APLOG_ERR, server_conf,
			    "accept: (client socket)");
	}
	else {
	    add_job(csd);
	    total_jobs++;
	}
    }
      
    APD2("process PID %d exiting", my_pid);

    /* Get ready to shutdown and exit */
    allowed_globals.exit_now = 1;
    if (!start_mutex_released) {
	ap_release_mutex(start_mutex);
    }

#ifdef UNGRACEFUL_RESTART
    SetEvent(allowed_globals.thread_exit_event);
#else
    for (i = 0; i < nthreads; i++) {
	add_job(-1);
    }
#endif

    APD2("process PID %d waiting for worker threads to exit", my_pid);
    /* Wait for all your children */
    end_time = time(NULL) + 180;
    while (nthreads) {
        rv = wait_for_many_objects(nthreads, child_handles, 
                                   end_time - time(NULL));
	if (rv != WAIT_TIMEOUT) {
	    rv = rv - WAIT_OBJECT_0;
	    ap_assert((rv >= 0) && (rv < nthreads));
	    cleanup_thread(child_handles, &nthreads, rv);
	    continue;
	}
	break;
    }

    APD2("process PID %d killing remaining worker threads", my_pid);
    for (i = 0; i < nthreads; i++) {
	kill_thread(child_handles[i]);
	free_thread(child_handles[i]);
    }
#ifdef UNGRACEFUL_RESTART
    ap_assert(CloseHandle(allowed_globals.thread_exit_event));
#endif
    destroy_semaphore(allowed_globals.jobsemaphore);
    ap_destroy_mutex(allowed_globals.jobmutex);

    ap_child_exit_modules(pconf, server_conf);
    ap_destroy_pool(pchild);

    cleanup_scoreboard();

    APD2("process PID %d exited", my_pid);
    clean_parent_exit(0);
}				/* standalone_main */

/* Spawn a child Apache process. The child process has the command
 * line arguments from argc and argv[], plus a -Z argument giving the
 * name of an event. The child should open and poll or wait on this
 * event. When it is signalled, the child should die.  prefix is a
 * prefix string for the event name.
 * 
 * The child_num argument on entry contains a serial number for this
 * child (used to create a unique event name). On exit, this number
 * will have been incremented by one, ready for the next call.
 *
 * On exit, the value pointed to be *ev will contain the event created
 * to signal the new child process.
 *
 * The return value is the handle to the child process if successful,
 * else -1. If -1 is returned the error will already have been logged
 * by ap_log_error(). 
 */

int create_event_and_spawn(int argc, char **argv, event **ev, int *child_num, char *prefix)
{
    char buf[40], mod[200];
    int i, rv;
    char **pass_argv = (char **) alloca(sizeof(char *) * (argc + 3));

    ap_snprintf(buf, sizeof(buf), "%s_%d", prefix, ++(*child_num));
    _flushall();
    *ev = CreateEvent(NULL, TRUE, FALSE, buf);
    if (!*ev) {
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_WIN32ERROR, NULL,
	    "could not create event for child process");
	return -1;
    }
    APD2("create_event_and_spawn(): created process kill event %s", buf);

    pass_argv[0] = argv[0];
    pass_argv[1] = "-Z";
    pass_argv[2] = buf;
    for (i = 1; i < argc; i++) {
	pass_argv[i + 2] = argv[i];
    }
    pass_argv[argc + 2] = NULL;

    rv = GetModuleFileName(NULL, mod, sizeof(mod));
    if (rv == sizeof(mod)) {
	/* mod[] was not big enough for our pathname */
	ap_log_error(APLOG_MARK, APLOG_ERR, NULL,
	    "Internal error: path to Apache process too long");
	return -1;
    }
    if (rv == 0) {
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_WIN32ERROR, NULL,
	    "GetModuleFileName() for current process");
	return -1;
    }
    rv = spawnv(_P_NOWAIT, mod, pass_argv);
    if (rv == -1) {
	ap_log_error(APLOG_MARK, APLOG_ERR|APLOG_WIN32ERROR, NULL,
	    "spawn of child process %s failed", mod);
	return -1;
    }

    return rv;
}

/**********************************************************************
 * master_main - this is the parent (main) process. We create a
 * child process to do the work, then sit around waiting for either
 * the child to exit, or a restart or exit signal. If the child dies,
 * we just respawn a new one. If we have a shutdown or graceful restart,
 * tell the child to die when it is ready. If it is a non-graceful
 * restart, force the child to die immediately.
 **********************************************************************/

#define MAX_PROCESSES 50 /* must be < MAX_WAIT_OBJECTS-1 */

void cleanup_process(HANDLE *handles, HANDLE *events, int position, int *processes)
{
    int i;
    int handle = 0;

    CloseHandle(handles[position]);
    CloseHandle(events[position]);

    handle = (int)handles[position];

    for (i = position; i < (*processes)-1; i++) {
	handles[i] = handles[i + 1];
	events[i] = events[i + 1];
    }
    (*processes)--;

    APD4("cleanup_processes: removed child in slot %d handle %d, max=%d", position, handle, *processes);
}

int create_process(HANDLE *handles, HANDLE *events, int *processes, int *child_num, char *kill_event_name, int argc, char **argv)
{
    int i = *processes;
    HANDLE kill_event;
    int child_handle;

    child_handle = create_event_and_spawn(argc, argv, &kill_event, child_num, kill_event_name);
    if (child_handle <= 0) {
	return -1;
    }
    handles[i] = (HANDLE)child_handle;
    events[i] = kill_event;
    (*processes)++;

    APD4("create_processes: created child in slot %d handle %d, max=%d", 
	(*processes)-1, handles[(*processes)-1], *processes);

    return 0;
}

int master_main(int argc, char **argv)
{
    int nchild = ap_daemons_to_start;
    event **ev;
    int *child;
    int child_num = 0;
    int rv, cld;
    char buf[100];
    int i;
    time_t tmstart;
    HANDLE signal_event;	/* used to signal shutdown/restart to parent */
    HANDLE process_handles[MAX_PROCESSES];
    HANDLE process_kill_events[MAX_PROCESSES];
    int current_live_processes = 0; /* number of child process we know about */
    int processes_to_create = 0;    /* number of child processes to create */
    pool *pparent = NULL;  /* pool for the parent process. Cleaned on each restart */

    nchild = 1;	    /* only allowed one child process for current generation */
    processes_to_create = nchild;

    is_graceful = 0;
    ++generation;

    signal_event = OpenEvent(EVENT_ALL_ACCESS, FALSE, "apache-signal");
    if (!signal_event) {
	ap_log_error(APLOG_MARK, APLOG_EMERG|APLOG_WIN32ERROR, server_conf,
		    "Cannot open apache-signal event");
	exit(1);
    }

    sprintf(buf, "Apache%d", getpid());
    start_mutex = ap_create_mutex(buf);
    ev = (event **) alloca(sizeof(event *) * nchild);
    child = (int *) alloca(sizeof(int) * (nchild+1));

    while (processes_to_create--) {
	service_set_status(SERVICE_START_PENDING);
	if (create_process(process_handles, process_kill_events, 
	    &current_live_processes, &child_num, buf, argc, argv) < 0) {
	    goto die_now;
	}
    }

    service_set_status(SERVICE_RUNNING);

    restart_pending = shutdown_pending = 0;

    do { /* restart-pending */
	if (!is_graceful) {
	    ap_restart_time = time(NULL);
	}
	ap_clear_pool(pconf);
	pparent = ap_make_sub_pool(pconf);

	server_conf = ap_read_config(pconf, pparent, ap_server_confname);
	ap_open_logs(server_conf, pconf);
	ap_set_version();
	ap_init_modules(pconf, server_conf);
	version_locked++;
	if (!is_graceful)
	    reinit_scoreboard(pconf);

	restart_pending = shutdown_pending = 0;

	/* Wait for either a child process to die, or for the stop_event
	 * to be signalled by the service manager or rpc server */
	while (1) {
	    /* Next line will block forever until either a child dies, or we
	     * get signalled on the "apache-signal" event (e.g. if the user is
	     * requesting a shutdown/restart)
	     */
	    if (current_live_processes == 0) {
		/* Shouldn't happen, but better safe than sorry */
		ap_log_error(APLOG_MARK,APLOG_ERR|APLOG_NOERRNO, server_conf,
 			"master_main: no child processes alive! creating one");
		if (create_process(process_handles, process_kill_events, 
		    &current_live_processes, &child_num, buf, argc, argv) < 0) {
		    goto die_now;
		}
		if (processes_to_create) {
		    processes_to_create--;
		}
	    }
	    process_handles[current_live_processes] = signal_event;
	    rv = WaitForMultipleObjects(current_live_processes+1, (HANDLE *)process_handles, 
			FALSE, INFINITE);
	    if (rv == WAIT_FAILED) {
		/* Something serious is wrong */
		ap_log_error(APLOG_MARK,APLOG_CRIT|APLOG_WIN32ERROR, server_conf,
		    "WaitForMultipeObjects on process handles and apache-signal -- doing shutdown");
		shutdown_pending = 1;
		break;
	    }
	    ap_assert(rv != WAIT_TIMEOUT);
	    cld = rv - WAIT_OBJECT_0;
	    APD4("main process: wait finished, cld=%d handle %d (max=%d)", cld, process_handles[cld], current_live_processes);
	    if (cld == current_live_processes) {
		/* stop_event is signalled, we should exit now */
		if (ResetEvent(signal_event) == 0)
		    APD1("main process: *** ERROR: ResetEvent(stop_event) failed ***");
		APD3("main process: stop_event signalled: shutdown_pending=%d, restart_pending=%d",
		    shutdown_pending, restart_pending);
		break;
	    }
	    ap_assert(cld < current_live_processes);
	    cleanup_process(process_handles, process_kill_events, cld, &current_live_processes);
	    APD2("main_process: child in slot %d died", rv);
	    if (processes_to_create) {
		create_process(process_handles, process_kill_events, &current_live_processes, &child_num, buf, argc, argv);
		processes_to_create--;
	    }
	}
	if (!shutdown_pending && !restart_pending) {
	    ap_log_error(APLOG_MARK,APLOG_CRIT|APLOG_NOERRNO, server_conf,
		"master_main: no shutdown or restart variables set -- doing shutdown");
	    shutdown_pending = 1;
	}
	if (shutdown_pending) {
	    /* tell all child processes to die */
	    for (i = 0; i < current_live_processes; i++) {
		APD3("main process: signalling child #%d handle %d to die", i, process_handles[i]);
		if (SetEvent(process_kill_events[i]) == 0)
		    ap_log_error(APLOG_MARK,APLOG_WIN32ERROR, server_conf,
			"SetEvent for child process in slot #%d", i);
	    }

	    break;
	}
	if (restart_pending) {
	    int children_to_kill = current_live_processes;

	    APD1("--- Doing graceful restart ---");

	    processes_to_create = nchild;
	    for (i = 0; i < nchild; ++i) {
		if (current_live_processes >= MAX_PROCESSES)
		    break;
		create_process(process_handles, process_kill_events, &current_live_processes, &child_num, buf, argc, argv);
		processes_to_create--;
	    }
	    for (i = 0; i < children_to_kill; i++) {
		APD3("main process: signalling child #%d handle %d to die", i, process_handles[i]);
		if (SetEvent(process_kill_events[i]) == 0)
		    ap_log_error(APLOG_MARK,APLOG_WIN32ERROR, server_conf,
 			"SetEvent for child process in slot #%d", i);
	    }
	}
	++generation;
    } while (restart_pending);

    /* If we dropped out of the loop we definitly want to die completely. We need to
     * make sure we wait for all the child process to exit first.
     */

    APD2("*** main process shutdown, processes=%d ***", current_live_processes);

die_now:
    CloseHandle(signal_event);

    tmstart = time(NULL);
    while (current_live_processes && ((tmstart+60) > time(NULL))) {
	service_set_status(SERVICE_STOP_PENDING);
	rv = WaitForMultipleObjects(current_live_processes, (HANDLE *)process_handles, FALSE, 2000);
	if (rv == WAIT_TIMEOUT)
	    continue;
	ap_assert(rv != WAIT_FAILED);
	cld = rv - WAIT_OBJECT_0;
	ap_assert(rv < current_live_processes);
	APD4("main_process: child in #%d handle %d died, left=%d", 
	    rv, process_handles[rv], current_live_processes);
	cleanup_process(process_handles, process_kill_events, cld, &current_live_processes);
    }
    for (i = 0; i < current_live_processes; i++) {
	ap_log_error(APLOG_MARK,APLOG_ERR|APLOG_NOERRNO, server_conf,
 	    "forcing termination of child #%d (handle %d)", i, process_handles[i]);
	TerminateProcess((HANDLE) process_handles[i], 1);
    }
    service_set_status(SERVICE_STOPPED);

    /* cleanup pid file on normal shutdown */
    {
	const char *pidfile = NULL;
	pidfile = ap_server_root_relative (pparent, ap_pid_fname);
	if ( pidfile != NULL && unlink(pidfile) == 0)
	    ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_INFO,
			 server_conf,
			 "httpd: removed PID file %s (pid=%ld)",
			 pidfile, (long)getpid());
    }

    if (pparent) {
	ap_destroy_pool(pparent);
    }

    ap_destroy_mutex(start_mutex);
    return (0);
}

#ifdef WIN32
__declspec(dllexport)
     int apache_main(int argc, char *argv[])
#else
int REALMAIN(int argc, char *argv[]) 
#endif
{
    int c;
    int child = 0;
    char *cp;
    int run_as_service = 1;
    int install = 0;
    int configtestonly = 0;
    
    common_init();

    ap_server_argv0 = argv[0];

    /* Get the serverroot from the registry, if it exists. This can be
     * overridden by a command line -d argument.
     */
    if (ap_registry_get_server_root(pconf, ap_server_root, sizeof(ap_server_root)) < 0) {
	/* The error has already been logged. Actually it won't have been,
	 * because we haven't read the config files to find out where our 
	 * error log is. But we can't just ignore the error since we might
	 * end up using totally the wrong server root.
	 */
	exit(1);
    }

    if (!*ap_server_root) {
	ap_cpystrn(ap_server_root, HTTPD_ROOT, sizeof(ap_server_root));
    }
    ap_cpystrn(ap_server_confname, SERVER_CONFIG_FILE, sizeof(ap_server_confname));

    ap_setup_prelinked_modules();

    while ((c = getopt(argc, argv, "D:C:c:Xd:f:vVhlZ:iusSt")) != -1) {
        char **new;
	switch (c) {
	case 'c':
	    new = (char **)ap_push_array(ap_server_post_read_config);
	    *new = ap_pstrdup(pcommands, optarg);
	    break;
	case 'C':
	    new = (char **)ap_push_array(ap_server_pre_read_config);
	    *new = ap_pstrdup(pcommands, optarg);
	    break;
	case 'D':
	    new = (char **)ap_push_array(ap_server_config_defines);
	    *new = ap_pstrdup(pcommands, optarg);
	    break;
#ifdef WIN32
	case 'Z':
	    exit_event = open_event(optarg);
	    APD2("child: opened process event %s", optarg);
	    cp = strchr(optarg, '_');
	    ap_assert(cp);
	    *cp = 0;
	    start_mutex = ap_open_mutex(optarg);
	    child = 1;
	    break;
	case 'i':
	    install = 1;
	    break;
	case 'u':
	    install = -1;
	    break;
	case 's':
	    run_as_service = 0;
	    break;
	case 'S':
	    ap_dump_settings = 1;
	    break;
#endif /* WIN32 */
	case 'd':
	    ap_cpystrn(ap_server_root, ap_os_canonical_filename(pconf, optarg), sizeof(ap_server_root));
	    break;
	case 'f':
	    ap_cpystrn(ap_server_confname, ap_os_canonical_filename(pconf, optarg), sizeof(ap_server_confname));
	    break;
	case 'v':
	    ap_set_version();
	    printf("Server version: %s\n", ap_get_server_version());
	    printf("Server built:   %s\n", ap_get_server_built());
	    exit(0);
	case 'V':
	    ap_set_version();
	    show_compile_settings();
	    exit(0);
	case 'h':
	    ap_show_directives();
	    exit(0);
	case 'l':
	    ap_show_modules();
	    exit(0);
	case 'X':
	    ++one_process;	/* Weird debugging mode. */
	    break;
	case 't':
	    configtestonly = 1;
	    break;
	case '?':
	    usage(argv[0]);
	}
    }

    if (!child && run_as_service) {
	service_cd();
    }

    server_conf = ap_read_config(pconf, ptrans, ap_server_confname);

    if (configtestonly) {
        fprintf(stderr, "Syntax OK\n");
        exit(0);
    }

    if (!child) {
	ap_log_pid(pconf, ap_pid_fname);
    }
    ap_set_version();
    ap_init_modules(pconf, server_conf);
    ap_suexec_enabled = init_suexec();
    version_locked++;
    ap_open_logs(server_conf, pconf);
    set_group_privs();

#ifdef OS2
    printf("%s \n", ap_get_server_version());
#endif
#ifdef WIN32
    if (!child) {
	printf("%s \n", ap_get_server_version());
    }
#endif

    if (one_process && !exit_event)
	exit_event = create_event(0, 0, NULL);
    if (one_process && !start_mutex)
	start_mutex = ap_create_mutex(NULL);
    /*
     * In the future, the main will spawn off a couple
     * of children and monitor them. As soon as a child
     * exits, it spawns off a new one
     */
    if (child || one_process) {
	if (!exit_event || !start_mutex)
	    exit(-1);
	worker_main();
	ap_destroy_mutex(start_mutex);
	destroy_event(exit_event);
    }
    else {
	service_main(master_main, argc, argv,
			"Apache", install, run_as_service);
    }

    clean_parent_exit(0);
    return 0;	/* purely to avoid a warning */
}

#endif /* ndef MULTITHREAD */

#else  /* ndef SHARED_CORE_TIESTATIC */

/*
**  Standalone Tie Program for Shared Core support
**
**  It's purpose is to tie the static libraries and 
**  the shared core library under link-time and  
**  passing execution control to the real main function
**  in the shared core library under run-time.
*/

extern int ap_main(int argc, char *argv[]);

int main(int argc, char *argv[]) 
{
    return ap_main(argc, argv);
}

#endif /* ndef SHARED_CORE_TIESTATIC */
#else  /* ndef SHARED_CORE_BOOTSTRAP */

/*
**  Standalone Bootstrap Program for Shared Core support
**
**  It's purpose is to initialise the LD_LIBRARY_PATH
**  environment variable therewith the Unix loader is able
**  to start the Standalone Tie Program (see above)
**  and then replacing itself with this program by
**  immediately passing execution to it.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ap_config.h"
#include "httpd.h"

#if defined(HPUX) || defined(HPUX10) || defined(HPUX11)
#define VARNAME "SHLIB_PATH"
#else
#define VARNAME "LD_LIBRARY_PATH"
#endif

#ifndef SHARED_CORE_DIR 
#define SHARED_CORE_DIR HTTPD_ROOT "/libexec"
#endif

#ifndef SHARED_CORE_EXECUTABLE_PROGRAM
#define SHARED_CORE_EXECUTABLE_PROGRAM "libhttpd.ep"
#endif

extern char *optarg;
extern int   optind;

int main(int argc, char *argv[], char *envp[]) 
{
    char prog[MAX_STRING_LEN];
    char llp_buf[MAX_STRING_LEN];
    char **llp_slot;
    char *llp_existing;
    char *llp_dir;
    char **envpnew;
    int c, i, l;

    /* 
     * parse argument line, 
     * but only handle the -L option 
     */
    llp_dir = SHARED_CORE_DIR;
    while ((c = getopt(argc, argv, "D:C:c:Xd:f:vVhlL:SZ:t")) != -1) {
	switch (c) {
	case 'D':
	case 'C':
	case 'c':
	case 'X':
	case 'd':
	case 'f':
	case 'v':
	case 'V':
	case 'h':
	case 'l':
	case 'S':
	case 'Z':
	case 't':
	case '?':
	    break;
	case 'L':
	    llp_dir = strdup(optarg);
	    break;
	}
    }

    /* 
     * create path to SHARED_CORE_EXECUTABLE_PROGRAM
     */
    sprintf(prog, "%s/%s", llp_dir, SHARED_CORE_EXECUTABLE_PROGRAM);

    /* 
     * adjust process environment therewith the Unix loader 
     * is able to start the SHARED_CORE_EXECUTABLE_PROGRAM.
     */
    llp_slot = NULL;
    llp_existing = NULL;
    l = strlen(VARNAME);
    for (i = 0; envp[i] != NULL; i++) {
	if (strncmp(envp[i], VARNAME "=", l+1) == 0) {
	    llp_slot = &envp[i];
	    llp_existing = strchr(envp[i], '=') + 1;
	}
    }
    if (llp_slot == NULL) {
	envpnew = (char **)malloc(sizeof(char *)*(i + 2));
	memcpy(envpnew, envp, sizeof(char *)*i);
	envp = envpnew;
	llp_slot = &envp[i++];
	envp[i] = NULL;
    }
    if (llp_existing != NULL)
	 sprintf(llp_buf, "%s=%s:%s", VARNAME, llp_dir, llp_existing);
    else
	 sprintf(llp_buf, "%s=%s", VARNAME, llp_dir);
    *llp_slot = strdup(llp_buf);

    /* 
     * finally replace our process with 
     * the SHARED_CORE_EXECUTABLE_PROGRAM
     */
    if (execve(prog, argv, envp) == -1) {
	fprintf(stderr, 
		"httpd: Unable to exec Shared Core Executable Program `%s'\n",
		prog);
	return 1;
    }
    else
	return 0;
}

#endif /* ndef SHARED_CORE_BOOTSTRAP */

