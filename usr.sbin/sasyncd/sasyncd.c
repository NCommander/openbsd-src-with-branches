/*	$OpenBSD: sasyncd.c,v 1.3 2005/04/03 12:03:43 ho Exp $	*/

/*
 * Copyright (c) 2005 H�kan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

/*
 * This code was written under funding by Multicom Security AB.
 */


#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "sasyncd.h"

static void
privdrop(void)
{
	struct passwd	*pw = getpwnam(SASYNCD_USER);

	if (!pw) {
		log_err("%s: getpwnam(\"%s\") failed", __progname,
		    SASYNCD_USER);
		exit(1);
	}

	if (chroot(pw->pw_dir) != 0 || chdir("/") != 0) {
		log_err("%s: chroot failed", __progname);
		exit(1);
	}

	if (setgroups(1, &pw->pw_gid) || setegid(pw->pw_gid) ||
	    setgid(pw->pw_gid) || seteuid(pw->pw_uid) || setuid(pw->pw_uid)) {
		log_err("%s: failed to drop privileges", __progname);
		exit(1);
	}
}

volatile int daemon_shutdown = 0;

/* Called by signal handler for controlled daemon shutdown. */
static void
sasyncd_stop(int s)
{
	daemon_shutdown++;
}

static int
sasyncd_run(void)
{
	struct timeval	*timeout, tv;
	fd_set		*rfds, *wfds;
	size_t		 fdsetsize;
	int		 maxfd, n;

	n = getdtablesize();
	fdsetsize = howmany(n, NFDBITS) * sizeof(fd_mask);

	rfds = (fd_set *)malloc(fdsetsize);
	if (!rfds) {
		log_err("malloc(%lu) failed", (unsigned long)fdsetsize);
		return -1;
	}

	wfds = (fd_set *)malloc(fdsetsize);
	if (!wfds) {
		log_err("malloc(%lu) failed", (unsigned long)fdsetsize);
		return -1;
	}

	signal(SIGINT, sasyncd_stop);
	signal(SIGTERM, sasyncd_stop);
	signal(SIGHUP, sasyncd_stop);

	while (!daemon_shutdown) {
		memset(rfds, 0, fdsetsize);
		memset(wfds, 0, fdsetsize);
		maxfd = net_set_rfds(rfds);
		n = net_set_pending_wfds(wfds);
		if (n > maxfd)
			maxfd = n;

		pfkey_set_rfd(rfds);
		pfkey_set_pending_wfd(wfds);
		if (cfgstate.pfkey_socket + 1 > maxfd)
			maxfd = cfgstate.pfkey_socket + 1;

		timeout = &tv;
		timer_next_event(&tv);

		n = select(maxfd, rfds, wfds, 0, timeout);
		if (n == -1) {
			if (errno != EINTR) {
				log_err("select()");
				sleep(1);
			}
		} else if (n) {
			net_handle_messages(rfds);
			net_send_messages(wfds);
			pfkey_read_message(rfds);
			pfkey_send_message(wfds);
		}
		timer_run();
	}
	free(rfds);
	free(wfds);
	return 0;
}

int
main(int argc, char **argv)
{
	int	r;

	/* Init. */
	closefrom(STDERR_FILENO + 1);
	for (r = 0; r <= 2; r++)
		if (fcntl(r, F_GETFL, 0) == -1 && errno == EBADF)
			(void)open("/dev/null", r ? O_WRONLY : O_RDONLY, 0);

	for (r = 1; r < _NSIG; r++)
		signal(r, SIG_DFL);

	log_init(__progname);
	timer_init();

	r = conf_init(argc, argv);
	if (r > 1) {
		fprintf(stderr, "Usage: %s [-c config-file] [-d] [-v[v]]\n",
		    __progname);
		fprintf(stderr, "Default configuration file is %s\n",
		    SASYNCD_CFGFILE);
	}
	if (r)
		return 1;

	if (geteuid() != 0) {
		/* No point in continuing. */
		fprintf(stderr, "This daemon needs to be run as root.\n");
		return 1;
	}

	if (carp_init())
		return 1;
	if (pfkey_init(0))
		return 1;
	if (net_init())
		return 1;

	/* Drop privileges. */
	privdrop();

	if (!cfgstate.debug)
		if (daemon(1, 0)) {
			perror("daemon()");
			exit(1);
		}

	/* Main loop. */
	sasyncd_run();

	/* Shutdown. */
	log_msg(0, "shutting down...");

	net_shutdown();
	pfkey_shutdown();
	return 0;
}

/* Special for compiling with Boehms GC. See Makefile and sasyncd.h  */
#if defined (GC_DEBUG)
char *
gc_strdup(const char *x)
{
        char *strcpy(char *,const char *);
        char *y = malloc(strlen(x) + 1);
        return strcpy(y,x);
}
#endif
