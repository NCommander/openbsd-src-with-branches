/* $OpenBSD: monitor.c,v 1.46 2005/05/23 22:21:56 ho Exp $	 */

/*
 * Copyright (c) 2003 H�kan Olsson.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <regex.h>
#include <keynote.h>

#include "conf.h"
#include "log.h"
#include "monitor.h"
#include "policy.h"
#include "ui.h"
#include "util.h"
#include "pf_key_v2.h"

struct monitor_state {
	pid_t           pid;
	int             s;
	char            root[MAXPATHLEN];
} m_state;

volatile sig_atomic_t sigchlded = 0;
extern volatile sig_atomic_t sigtermed;
static volatile sig_atomic_t cur_state = STATE_INIT;

extern void	set_slave_signals(void);

/* Private functions.  */
int             m_write_int32(int, int32_t);
int             m_write_raw(int, const char *, size_t);
int             m_read_int32(int, int32_t *);
int             m_read_raw(int, char *, size_t);
void            m_flush(int);

static void     m_priv_getfd(int);
static void     m_priv_getsocket(int);
static void     m_priv_setsockopt(int);
static void     m_priv_bind(int);
static int      m_priv_local_sanitize_path(char *, size_t, int);
static int      m_priv_check_sockopt(int, int);
static int      m_priv_check_bind(const struct sockaddr *, socklen_t);
static void     m_priv_increase_state(int);
static void     m_priv_test_state(int);

static void	m_priv_ui_init(int);
static void	m_priv_pfkey_open(int);

static void	set_monitor_signals(void);
static void	monitor_got_sigchld(int);
static void	sig_pass_to_chld(int);

/*
 * Public functions, unprivileged.
 */

/* Setup monitor context, fork, drop child privs.  */
pid_t
monitor_init(int debug)
{
	struct passwd  *pw;
	int             p[2];

	bzero(&m_state, sizeof m_state);

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, p) != 0)
		log_fatal("monitor_init: socketpair() failed");

	pw = getpwnam(ISAKMPD_PRIVSEP_USER);
	if (pw == NULL)
		log_fatalx("monitor_init: getpwnam(\"%s\") failed",
		    ISAKMPD_PRIVSEP_USER);
	strlcpy(m_state.root, pw->pw_dir, sizeof m_state.root);

	set_monitor_signals();
	m_state.pid = fork();

	if (m_state.pid == -1)
		log_fatal("monitor_init: for of unprivileged child failed");
	if (m_state.pid == 0) {
		/* The child process drops privileges. */
		set_slave_signals();

		if (chroot(pw->pw_dir) != 0 || chdir("/") != 0)
			log_fatal("monitor_init: chroot failed");

		if (setgroups(1, &pw->pw_gid) == -1 ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
			log_fatal("monitor_init: can't drop privileges");

		m_state.s = p[0];
		close(p[1]);

		LOG_DBG((LOG_MISC, 10,
		    "monitor_init: privileges dropped for child process"));
	} else {
		/* Privileged monitor. */
		setproctitle("monitor [priv]");

		m_state.s = p[1];
		close(p[0]);
	}

	/* With "-dd", stop and wait here. For gdb "attach" etc.  */
	if (debug > 1) {
		log_print("monitor_init: stopped %s PID %d fd %d%s",
		    m_state.pid ? "priv" : "child", getpid(), m_state.s,
		    m_state.pid ? ", waiting for SIGCONT" : "");
		kill(getpid(), SIGSTOP);	/* Wait here for SIGCONT.  */
		if (m_state.pid)
			kill(m_state.pid, SIGCONT); /* Continue child.  */
	}

	return m_state.pid;
}

void
monitor_exit(int code)
{
	if (m_state.pid != 0)
		kill(m_state.pid, SIGKILL);

	close(m_state.s);
	exit(code);
}

void
monitor_ui_init(void)
{
	int32_t	err;

	if (m_write_int32(m_state.s, MONITOR_UI_INIT))
		goto errout;

	if (m_read_int32(m_state.s, &err))
		goto errout;

	if (err != 0) {
		log_fatal("monitor_ui_init: parent could not create FIFO "
		    "\"%s\"", ui_fifo);
		exit(1);
	}

	ui_socket = mm_receive_fd(m_state.s);
	if (ui_socket < 0)
		log_fatal("monitor_ui_init: parent could not create FIFO "
		    "\"%s\"", ui_fifo);

	return;

errout:
	log_error("monitor_ui_init: problem talking to privileged process");
}

int
monitor_pf_key_v2_open(void)
{
	int32_t err;

	if (m_write_int32(m_state.s, MONITOR_PFKEY_OPEN))
		goto errout;

	if (m_read_int32(m_state.s, &err))
		goto errout;

	if (err < 0) {
		log_error("monitor_pf_key_v2_open: parent could not create "
		    "PF_KEY socket");
		return -1;
	}

	pf_key_v2_socket = mm_receive_fd(m_state.s);
	if (pf_key_v2_socket < 0) {
		log_error("monitor_pf_key_v2_open: mm_receive_fd() failed: %s",
		    strerror(errno));
		return -1;
	}
	return pf_key_v2_socket;

errout:
	log_error("monitor_pf_key_v2_open: problem talking to privileged "
	    "process");
	return -1;
}

int
monitor_open(const char *path, int flags, mode_t mode)
{
	int	fd, mode32 = (int32_t) mode;
	int32_t	err;
	char	pathreal[MAXPATHLEN];

	if (path[0] == '/')
		strlcpy(pathreal, path, sizeof pathreal);
	else
		snprintf(pathreal, sizeof pathreal, "%s/%s", m_state.root,
		    path);

	/* Write data to priv process.  */
	if (m_write_int32(m_state.s, MONITOR_GET_FD))
		goto errout;

	if (m_write_raw(m_state.s, pathreal, strlen(pathreal) + 1))
		goto errout;

	if (m_write_int32(m_state.s, flags))
		goto errout;

	if (m_write_int32(m_state.s, mode32))
		goto errout;

	if (m_read_int32(m_state.s, &err))
		goto errout;

	if (err != 0) {
		errno = (int) err;
		return -1;
	}
	/* Wait for response.  */
	fd = mm_receive_fd(m_state.s);
	if (fd < 0) {
		log_error("monitor_open: mm_receive_fd () failed: %s",
		    strerror(errno));
		return -1;
	}
	return fd;

errout:
	log_error("monitor_open: problem talking to privileged process");
	return -1;
}

FILE *
monitor_fopen(const char *path, const char *mode)
{
	FILE	*fp;
	int	 fd, flags = 0, saved_errno;
	mode_t	 mask, cur_umask;

	/* Only the child process is supposed to run this.  */
	if (m_state.pid)
		log_fatal("[priv] bad call to monitor_fopen");

	switch (mode[0]) {
	case 'r':
		flags = (mode[1] == '+' ? O_RDWR : O_RDONLY);
		break;
	case 'w':
		flags = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT |
		    O_TRUNC;
		break;
	case 'a':
		flags = (mode[1] == '+' ? O_RDWR : O_WRONLY) | O_CREAT |
		    O_APPEND;
		break;
	default:
		log_fatal("monitor_fopen: bad call");
	}

	cur_umask = umask(0);
	(void)umask(cur_umask);
	mask = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
	mask &= ~cur_umask;

	fd = monitor_open(path, flags, mask);
	if (fd < 0)
		return NULL;

	/* Got the fd, attach a FILE * to it.  */
	fp = fdopen(fd, mode);
	if (!fp) {
		log_error("monitor_fopen: fdopen() failed");
		saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return NULL;
	}
	return fp;
}

int
monitor_stat(const char *path, struct stat *sb)
{
	int	fd, r, saved_errno;

	/* O_NONBLOCK is needed for stat'ing fifos. */
	fd = monitor_open(path, O_RDONLY | O_NONBLOCK, 0);
	if (fd < 0)
		return -1;

	r = fstat(fd, sb);
	saved_errno = errno;
	close(fd);
	errno = saved_errno;
	return r;
}

int
monitor_setsockopt(int s, int level, int optname, const void *optval,
    socklen_t optlen)
{
	int32_t	ret, err;

	if (m_write_int32(m_state.s, MONITOR_SETSOCKOPT))
		goto errout;
	if (mm_send_fd(m_state.s, s))
		goto errout;

	if (m_write_int32(m_state.s, (int32_t)level))
		goto errout;
	if (m_write_int32(m_state.s, (int32_t)optname))
		goto errout;
	if (m_write_int32(m_state.s, (int32_t)optlen))
		goto errout;
	if (m_write_raw(m_state.s, (const char *)optval, (size_t)optlen))
		goto errout;

	if (m_read_int32(m_state.s, &err))
		goto errout;

	if (err != 0)
		errno = (int)err;

	if (m_read_int32(m_state.s, &ret))
		goto errout;

	return (int)ret;

errout:
	log_print("monitor_setsockopt: read/write error");
	return -1;
}

int
monitor_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
	int32_t	ret, err;

	if (m_write_int32(m_state.s, MONITOR_BIND))
		goto errout;
	if (mm_send_fd(m_state.s, s))
		goto errout;

	if (m_write_int32(m_state.s, (int32_t)namelen))
		goto errout;
	if (m_write_raw(m_state.s, (const char *)name, (size_t)namelen))
		goto errout;

	if (m_read_int32(m_state.s, &err))
		goto errout;

	if (err != 0)
		errno = (int)err;

	if (m_read_int32(m_state.s, &ret))
		goto errout;

	return (int)ret;

errout:
	log_print("monitor_bind: read/write error");
	return -1;
}

struct monitor_dirents *
monitor_opendir(const char *path)
{
	char           *buf, *cp;
	size_t          bufsize;
	int             fd, nbytes, entries;
	long            base;
	struct stat     sb;
	struct dirent  *dp;
	struct monitor_dirents *direntries;

	fd = monitor_open(path, 0, O_RDONLY);
	if (fd < 0) {
		log_error("monitor_opendir: opendir(\"%s\") failed", path);
		return NULL;
	}
	/* Now build a list with all dirents from fd. */
	if (fstat(fd, &sb) < 0) {
		(void)close(fd);
		return NULL;
	}
	if (!S_ISDIR(sb.st_mode)) {
		(void)close(fd);
		errno = EACCES;
		return NULL;
	}
	bufsize = sb.st_size;
	if (bufsize < sb.st_blksize)
		bufsize = sb.st_blksize;

	buf = calloc(bufsize, sizeof(char));
	if (buf == NULL) {
		(void)close(fd);
		errno = EACCES;
		return NULL;
	}
	nbytes = getdirentries(fd, buf, bufsize, &base);
	if (nbytes <= 0) {
		(void)close(fd);
		free(buf);
		errno = EACCES;
		return NULL;
	}
	(void)close(fd);

	for (entries = 0, cp = buf; cp < buf + nbytes;) {
		dp = (struct dirent *)cp;
		cp += dp->d_reclen;
		entries++;
	}

	direntries = calloc(1, sizeof(struct monitor_dirents));
	if (direntries == NULL) {
		free(buf);
		errno = EACCES;
		return NULL;
	}
	direntries->dirents = calloc(entries + 1, sizeof(struct dirent *));
	if (direntries->dirents == NULL) {
		free(buf);
		free(direntries);
		errno = EACCES;
		return NULL;
	}
	direntries->current = 0;

	for (entries = 0, cp = buf; cp < buf + nbytes;) {
		dp = (struct dirent *)cp;
		direntries->dirents[entries++] = dp;
		cp += dp->d_reclen;
	}
	direntries->dirents[entries] = NULL;

	return direntries;
}

struct dirent *
monitor_readdir(struct monitor_dirents *direntries)
{
	if (direntries->dirents[direntries->current] != NULL)
		return direntries->dirents[direntries->current++];

	return NULL;
}

void
monitor_closedir(struct monitor_dirents *direntries)
{
	free(direntries->dirents);
	free(direntries);
}

void
monitor_init_done(void)
{
	if (m_write_int32(m_state.s, MONITOR_INIT_DONE))
		log_print("monitor_init_done: read/write error");
}

/*
 * Start of code running with privileges (the monitor process).
 */

static void
set_monitor_signals(void)
{
	int n;

	for (n = 0; n < _NSIG; n++)
		signal(n, SIG_DFL);

	/* If the child dies, we should shutdown also.  */
	signal(SIGCHLD, monitor_got_sigchld);

	/* Forward some signals to the child. */
	signal(SIGTERM, sig_pass_to_chld);
	signal(SIGHUP, sig_pass_to_chld);
	signal(SIGUSR1, sig_pass_to_chld);
}

/* ARGSUSED */
static void
monitor_got_sigchld(int sig)
{
	sigchlded = 1;
}

static void
sig_pass_to_chld(int sig)
{
	int	oerrno = errno;

	if (m_state.pid != -1)
		kill(m_state.pid, sig);
	errno = oerrno;
}

/* This function is where the privileged process waits(loops) indefinitely.  */
void
monitor_loop(int debug)
{
	pid_t	 pid;
	int32_t	 msgcode;
	int	 status;

	if (!debug)
		log_to(0);

	while (cur_state < STATE_QUIT) {
		/*
		 * Currently, there is no need for us to hang around if the
		 * child is in the process of shutting down.
		 */
		if (sigtermed) {
			m_priv_increase_state(STATE_QUIT);
			kill(m_state.pid, SIGTERM);
			break;
		}

		if (sigchlded) {
			do {
				pid = waitpid(m_state.pid, &status, WNOHANG);
			} while (pid == -1 && errno == EINTR);

			if (pid == m_state.pid && (WIFEXITED(status) ||
			    WIFSIGNALED(status))) {
				m_priv_increase_state(STATE_QUIT);
				break;
			}
		}

		if (m_read_int32(m_state.s, &msgcode))
			break;

		switch (msgcode) {
		case MONITOR_GET_FD:
			m_priv_getfd(m_state.s);
			break;

		case MONITOR_UI_INIT:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_UI_INIT"));
			m_priv_test_state(STATE_INIT);
			m_priv_ui_init(m_state.s);
			break;

		case MONITOR_PFKEY_OPEN:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_PFKEY_OPEN"));
			m_priv_test_state(STATE_INIT);
			m_priv_pfkey_open(m_state.s);
			break;

		case MONITOR_GET_SOCKET:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_GET_SOCKET"));
			m_priv_test_state(STATE_INIT);
			m_priv_getsocket(m_state.s);
			break;

		case MONITOR_SETSOCKOPT:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_SETSOCKOPT"));
			m_priv_test_state(STATE_INIT);
			m_priv_setsockopt(m_state.s);
			break;

		case MONITOR_BIND:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_BIND"));
			m_priv_test_state(STATE_INIT);
			m_priv_bind(m_state.s);
			break;

		case MONITOR_INIT_DONE:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_INIT_DONE"));
			m_priv_test_state(STATE_INIT);
			m_priv_increase_state(STATE_RUNNING);
			break;

		case MONITOR_SHUTDOWN:
			LOG_DBG((LOG_MISC, 80,
			    "monitor_loop: MONITOR_SHUTDOWN"));
			m_priv_increase_state(STATE_QUIT);
			break;

		default:
			log_print("monitor_loop: got unknown code %d",
			    msgcode);
		}
	}

	exit(0);
}


/* Privileged: called by monitor_loop.  */
static void
m_priv_ui_init(int s)
{
	int32_t err;

	ui_init();

	if (ui_socket >= 0)
		err = 0;
	else
		err = -1;

	if (m_write_int32(s, err))
		goto errout;

	if (ui_socket >= 0 && mm_send_fd(s, ui_socket)) {
		close(ui_socket);
		goto errout;
	}

	/* In case of stdin, we do not close the socket. */
	if (ui_socket > 0)
		close(ui_socket);
	return;

errout:
	log_error("m_priv_ui_init: read/write operation failed");
}

/* Privileged: called by monitor_loop.  */
static void
m_priv_pfkey_open(int s)
{
	int fd;
	int32_t err;

	fd = pf_key_v2_open();

	if (fd < 0)
		err = -1;
	else
		err = 0;

	if (m_write_int32(s, err))
		goto errout;

	if (fd > 0 && mm_send_fd(s, fd)) {
		close(fd);
		goto errout;
	}
	close(fd);

	return;

errout:
	log_error("m_priv_pfkey_open: read/write operation failed");
}

/* Privileged: called by monitor_loop.  */
static void
m_priv_getfd(int s)
{
	char	path[MAXPATHLEN];
	int32_t	v, err;
	int	flags;
	mode_t	mode;

	/*
	 * We expect the following data on the socket:
	 *  u_int32_t  pathlen
	 *  <variable> path
	 *  u_int32_t  flags
	 *  u_int32_t  mode
         */

	if (m_read_raw(s, path, MAXPATHLEN))
		goto errout;

	if (m_read_int32(s, &v))
		goto errout;
	flags = (int)v;

	if (m_read_int32(s, &v))
		goto errout;
	mode = (mode_t) v;

	if (m_priv_local_sanitize_path(path, sizeof path, flags) != 0) {
		err = EACCES;
		v = -1;
	} else {
		err = 0;
		v = (int32_t)open(path, flags, mode);
		if (v < 0)
			err = (int32_t)errno;
	}

	if (m_write_int32(s, err))
		goto errout;

	if (v > 0 && mm_send_fd(s, v)) {
		close(v);
		goto errout;
	}
	close(v);
	return;

errout:
	log_error("m_priv_getfd: read/write operation failed");
}

/* Privileged: called by monitor_loop.  */
static void
m_priv_getsocket(int s)
{
	int	domain, type, protocol;
	int32_t	v, err;

	if (m_read_int32(s, &v))
		goto errout;
	domain = (int)v;

	if (m_read_int32(s, &v))
		goto errout;
	type = (int)v;

	if (m_read_int32(s, &v))
		goto errout;
	protocol = (int)v;

	err = 0;
	v = (int32_t)socket(domain, type, protocol);
	if (v < 0)
		err = (int32_t)errno;

	if (m_write_int32(s, err))
		goto errout;

	if (v > 0 && mm_send_fd(s, v)) {
		close(v);
		goto errout;
	}
	close(v);
	return;

errout:
	log_error("m_priv_getsocket: read/write operation failed");
}

/* Privileged: called by monitor_loop.  */
static void
m_priv_setsockopt(int s)
{
	int		 sock, level, optname;
	char		*optval = 0;
	socklen_t	 optlen;
	int32_t		 v, err;

	sock = mm_receive_fd(s);
	if (sock < 0)
		goto errout;

	if (m_read_int32(s, &level))
		goto errout;

	if (m_read_int32(s, &optname))
		goto errout;

	if (m_read_int32(s, (int *)&optlen))
		goto errout;

	optval = (char *)malloc(optlen);
	if (!optval)
		goto errout;

	if (m_read_raw(s, optval, optlen))
		goto errout;

	if (m_priv_check_sockopt(level, optname) != 0) {
		err = EACCES;
		v = -1;
	} else {
		err = 0;
		v = (int32_t)setsockopt(sock, level, optname, optval, optlen);
		if (v < 0)
			err = (int32_t)errno;
	}

	close(sock);
	sock = -1;

	if (m_write_int32(s, err))
		goto errout;

	if (m_write_int32(s, v))
		goto errout;

	free(optval);
	return;

errout:
	log_print("m_priv_setsockopt: read/write error");
	if (optval)
		free(optval);
	if (sock >= 0)
		close(sock);
}

/* Privileged: called by monitor_loop.  */
static void
m_priv_bind(int s)
{
	int		 sock;
	struct sockaddr *name = 0;
	socklen_t        namelen;
	int32_t          v, err;

	sock = mm_receive_fd(s);
	if (sock < 0)
		goto errout;

	if (m_read_int32(s, &v))
		goto errout;
	namelen = (socklen_t) v;

	name = (struct sockaddr *)malloc(namelen);
	if (!name)
		goto errout;

	if (m_read_raw(s, (char *)name, (size_t)namelen))
		goto errout;

	if (m_priv_check_bind(name, namelen) != 0) {
		err = EACCES;
		v = -1;
	} else {
		err = 0;
		v = (int32_t)bind(sock, name, namelen);
		if (v < 0) {
			log_error("m_priv_bind: bind(%d,%p,%d) returned %d",
			    sock, name, namelen, v);
			err = (int32_t)errno;
		}
	}

	close(sock);
	sock = -1;

	if (m_write_int32(s, err))
		goto errout;

	if (m_write_int32(s, v))
		goto errout;

	free(name);
	return;

errout:
	log_print("m_priv_bind: read/write error");
	if (name)
		free(name);
	if (sock >= 0)
		close(sock);
}

/*
 * Help functions, used by both privileged and unprivileged code
 */

/* Write a 32-bit value to a socket.  */
int
m_write_int32(int s, int32_t value)
{
	u_int32_t	v;

	memcpy(&v, &value, sizeof v);
	return (write(s, &v, sizeof v) == -1);
}

/* Write a number of bytes of data to a socket.  */
int
m_write_raw(int s, const char *data, size_t dlen)
{
	if (m_write_int32(s, (int32_t) dlen))
		return 1;
	return (write(s, data, dlen) == -1);
}

int
m_read_int32(int s, int32_t *value)
{
	u_int32_t	v;

	if (read(s, &v, sizeof v) != sizeof v)
		return 1;
	memcpy(value, &v, sizeof v);
	return 0;
}

int
m_read_raw(int s, char *data, size_t maxlen)
{
	u_int32_t	v;
	int		r;

	if (m_read_int32(s, &v))
		return 1;
	if (v > maxlen)
		return 1;
	r = read(s, data, v);
	return (r == -1);
}

/* Drain all available input on a socket.  */
void
m_flush(int s)
{
	u_int8_t	tmp;
	int             one = 1;

	ioctl(s, FIONBIO, &one);/* Non-blocking */
	while (read(s, &tmp, 1) > 0);
	ioctl(s, FIONBIO, 0);	/* Blocking */
}

/* Check that path/mode is permitted.  */
static int
m_priv_local_sanitize_path(char *path, size_t pmax, int flags)
{
	char	*p;

	/*
	 * We only permit paths starting with
	 *  /etc/isakmpd/	(read only)
	 *  /var/run/		(rw)
         */

	if (strlen(path) < strlen("/var/run/"))
		goto bad_path;

	/* Any path containing '..' is invalid.  */
	for (p = path; *p && (p - path) < (int)pmax; p++)
		if (*p == '.' && *(p + 1) == '.')
			goto bad_path;

	/* For any write-mode, only a few paths are permitted.  */
	if ((flags & O_ACCMODE) != O_RDONLY) {
		if (strncmp("/var/run/", path, strlen("/var/run/")) == 0)
			return 0;
		goto bad_path;
	}
	/* Any other path is read-only.  */
	if (strncmp(ISAKMPD_ROOT, path, strlen(ISAKMPD_ROOT)) == 0 ||
	    strncmp("/var/run/", path, strlen("/var/run/")) == 0)
		return 0;

bad_path:
	log_print("m_priv_local_sanitize_path: illegal path \"%.1023s\", "
		  "replaced with \"/dev/null\"", path);
	strlcpy(path, "/dev/null", pmax);
	return 1;
}

/* Check setsockopt */
static int
m_priv_check_sockopt(int level, int name)
{
	switch (level) {
		/* These are allowed */
		case SOL_SOCKET:
		case IPPROTO_IP:
		case IPPROTO_IPV6:
		break;

	default:
		log_print("m_priv_check_sockopt: Illegal level %d", level);
		return 1;
	}

	switch (name) {
		/* These are allowed */
	case SO_REUSEPORT:
	case SO_REUSEADDR:
	case IP_AUTH_LEVEL:
	case IP_ESP_TRANS_LEVEL:
	case IP_ESP_NETWORK_LEVEL:
	case IP_IPCOMP_LEVEL:
	case IPV6_AUTH_LEVEL:
	case IPV6_ESP_TRANS_LEVEL:
	case IPV6_ESP_NETWORK_LEVEL:
	case IPV6_IPCOMP_LEVEL:
		break;

	default:
		log_print("m_priv_check_sockopt: Illegal option name %d",
		    name);
		return 1;
	}

	return 0;
}

/* Check bind */
static int
m_priv_check_bind(const struct sockaddr *sa, socklen_t salen)
{
	in_port_t       port;

	if (sa == NULL) {
		log_print("NULL address");
		return 1;
	}
	if (SA_LEN(sa) != salen) {
		log_print("Length mismatch: %lu %lu", (unsigned long)sa->sa_len,
		    (unsigned long)salen);
		return 1;
	}
	switch (sa->sa_family) {
	case AF_INET:
		if (salen != sizeof(struct sockaddr_in)) {
			log_print("Invalid inet address length");
			return 1;
		}
		port = ((const struct sockaddr_in *)sa)->sin_port;
		break;
	case AF_INET6:
		if (salen != sizeof(struct sockaddr_in6)) {
			log_print("Invalid inet6 address length");
			return 1;
		}
		port = ((const struct sockaddr_in6 *)sa)->sin6_port;
		break;
	default:
		log_print("Unknown address family");
		return 1;
	}

	port = ntohs(port);

	if (port != ISAKMP_PORT_DEFAULT && port < 1024) {
		log_print("Disallowed port %u", port);
		return 1;
	}
	return 0;
}

/* Increase state into less permissive mode */
static void
m_priv_increase_state(int state)
{
	if (state <= cur_state)
		log_print("m_priv_increase_state: attempt to decrease state "
		    "or match current state");
	if (state < STATE_INIT || state > STATE_QUIT)
		log_print("m_priv_increase_state: attempt to switch to "
		    "invalid state");
	cur_state = state;
}

static void
m_priv_test_state(int state)
{
	if (cur_state != state)
		log_print("m_priv_test_state: Illegal state: %d != %d",
		    (int)cur_state, state);
}
