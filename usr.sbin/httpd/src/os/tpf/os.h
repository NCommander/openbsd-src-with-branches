#ifndef APACHE_OS_H
#define APACHE_OS_H

/*
 * This file is included in all Apache source code. It contains definitions
 * of facilities available on _this_ operating system (HAVE_* macros),
 * and prototypes of OS specific functions defined in os.c or os-inline.c
 */

#define PLATFORM "TPF"

/************************************************************************
 *  PJ26895 provides support for non_socket_select.
 *  You can determine if this apar is applied to your system by looking
 *  at i$pwbl.h.  If the function non_socket_select is defined,
 *  then add #define TPF_HAVE_NONSOCKET_SELECT
 *  else add #define TPF_NO_NONSOCKET_SELECT
 *
 *  One of these two #defines is required and must be added here in os.h
 *  before the following check.
 ************************************************************************/

#if !defined(TPF_HAVE_NONSOCKET_SELECT) && !defined(TPF_NO_NONSOCKET_SELECT)
   #error "You must define whether your system supports non_socket_select()"
   #error "See src/os/tpf/os.h for instructions"
#endif

#if defined(TPF_HAVE_NONSOCKET_SELECT) && defined(TPF_NO_NONSOCKET_SELECT)
   #error "TPF_HAVE_NONSOCKET_SELECT and TPF_NO_NONSOCKET_SELECT"
   #error "cannot both be defined"
   #error "See src/os/tpf/os.h for instructions"
#endif

/************************************************************************
 *  PJ27387 or PJ26188 provides support for tpf_sawnc.
 *  You can determine if this apar is applied to your system by looking at
 *  tpfapi.h or i$fsdd.h.  If the function tpf_sawnc is defined,
 *  then add #define TPF_HAVE_SAWNC
 *  else add #define TPF_NO_SAWNC
 *
 *  One of these two #defines is required and must be added here in os.h
 *  before the following check.
 ************************************************************************/

#if !defined(TPF_HAVE_SAWNC) && !defined(TPF_NO_SAWNC)
   #error "You must define whether your system supports tpf_sawnc()"
   #error "See src/os/tpf/os.h for instructions"
#endif

#if defined(TPF_HAVE_SAWNC) && defined(TPF_NO_SAWNC)
   #error "TPF_HAVE_SAWNC and TPF_NO_SAWNC"
   #error "cannot both be defined"
   #error "See src/os/tpf/os.h for instructions"
#endif

/* if the compiler defined errno then undefine it
   and pick up the correct definition from errno.h */
#if defined(errno) && !defined(__errnoh)
#undef errno
#include <errno.h>
#endif

/* If APAR PJ27277 (which shipped on PUT13) has been applied */
/* then we want to #define TPF_FORK_EXTENDED so Perl CGIs will work. */
/* Rather than hardcoding it we'll check for "environ" in stdlib.h, */
/* which was also added by PJ27277. */
#include <stdlib.h>
#if defined(environ) && !defined(TPF_FORK_EXTENDED)
#define TPF_FORK_EXTENDED
#endif

#include <sysapi.h>  
#include "ap_config.h"

#ifdef HAVE_ISNAN
#undef HAVE_ISNAN
#endif

#if !defined(INLINE) && defined(USE_GNU_INLINE)
/* Compiler supports inline, so include the inlineable functions as
 * part of the header
 */
#define INLINE extern ap_inline
#include "os-inline.c"
#endif

#ifndef INLINE
/* Compiler does not support inline, so prototype the inlineable functions
 * as normal
 */
extern int ap_os_is_path_absolute(const char *f);
#endif

/* Other ap_os_ routines not used by this platform */

#define ap_os_is_filename_valid(f)          (1)
#define ap_os_kill(pid, sig)                kill(pid, sig)

#include <strings.h>
#ifndef __strings_h

#define FD_SETSIZE    2048 
 
typedef long fd_mask;

#define NBBY    8    /* number of bits in a byte */
#define NFDBITS (sizeof(fd_mask) * NBBY)
#define  howmany(x, y)  (((x)+((y)-1))/(y))

typedef struct fd_set { 
        fd_mask fds_bits [howmany(FD_SETSIZE, NFDBITS)];
} fd_set; 

#define FD_CLR(n, p)((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define FD_ISSET(n, p)((p)->fds_bits[(n)/NFDBITS] & (1 <<((n) % NFDBITS)))
#define  FD_ZERO(p)   memset((char *)(p), 0, sizeof(*(p)))
#endif
    
#ifdef FD_SET
#undef FD_SET
#define FD_SET(n, p) (0)
#endif

#define  RESOURCE_KEY ((void*) 0xC1C2C1C3)

/* TPF doesn't have, or need, tzset (it is used in mod_expires.c) */
#define tzset()
 
/* definitions for the file descriptor inheritance table */
#define TPF_FD_LIST_SIZE 4000

enum FILE_TYPE { PIPE_OUT = 1, PIPE_IN, PIPE_ERR };

typedef struct tpf_fd_item {
    int            fd;
    enum FILE_TYPE file_type;
    char           *fname;
}TPF_FD_ITEM;

typedef struct tpf_fd_list {
    void           *next_avail_byte;
    void           *last_avail_byte;
    unsigned int   nbr_of_items;
    TPF_FD_ITEM    first_item;
}TPF_FD_LIST;

#include <i$netd.h>
typedef struct apache_input {
    void                *scoreboard_heap;   /* scoreboard system heap address */
    int                 scoreboard_fd;      /* scoreboard file descriptor */
    int                 slot;               /* child number */
    int                 generation;         /* server generation number */
    int                 listeners[10];
    time_t              restart_time;
    TPF_FD_LIST         *tpf_fds;           /* fd inheritance table ptr */
    void                *shm_static_ptr;    /* shm ptr for static pages */
}APACHE_TPF_INPUT;

typedef union ebw_area {
    INETD_SERVER_INPUT parent;
    APACHE_TPF_INPUT   child;
}EBW_AREA;
 
extern void *tpf_shm_static_ptr;            /* mod_tpf_shm_static */
#define TPF_SHM_STATIC_SIZE 200000
#define MMAP_SEGMENT_SIZE 32767             /* writev can handle 32767 */
#define _SYS_UIO_H_                         /* writev */

typedef struct tpf_fork_child {
     char  *filename;
     enum { FORK_NAME = 1, FORK_FILE = 2 } prog_type;
     void  *subprocess_env;
}TPF_FORK_CHILD;

int tpf_accept(int sockfd, struct sockaddr *peer, int *paddrlen);
extern int tpf_child;

struct server_rec;
pid_t os_fork(struct server_rec *s, int slot);
void ap_tpf_zinet_checks(int standalone,
                         const char *servername,
                         struct server_rec *s);
int os_check_server(char *server);
void show_os_specific_compile_settings(void);
char *getpass(const char *prompt);
int killpg(pid_t pgrp, int sig);
extern char *ap_server_argv0;
extern int scoreboard_fd;
#include <signal.h>
#ifndef SIGPIPE
#define SIGPIPE 14
#endif
#ifdef NSIG
#undef NSIG
#endif

/* various #defines for ServerType/ZINET model checks: */

#define TPF_SERVERTYPE_MSG \
        "ServerType inetd is not supported on TPF" \
        " -- Apache startup aborted"

#ifdef INETD_IDCF_MODEL_DAEMON
#define TPF_STANDALONE_CONFLICT_MSG \
        "ServerType standalone requires ZINET model DAEMON or NOLISTEN" \
        " -- Apache startup aborted"
#define TPF_NOLISTEN_WARNING \
        "ZINET model DAEMON is preferred over model NOLISTEN"
#else
#define INETD_IDCF_MODEL_DAEMON -1
#define TPF_STANDALONE_CONFLICT_MSG \
        "ServerType standalone requires ZINET model NOLISTEN" \
        " -- Apache startup aborted"
#endif

#define TPF_UNABLE_TO_DETERMINE_ZINET_MODEL \
        "Unable to determine ZINET model: inetd_getServer call failed" \
        " -- Apache startup aborted"

#endif /*! APACHE_OS_H*/
