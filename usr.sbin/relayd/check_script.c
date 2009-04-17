/*	$OpenBSD: check_script.c,v 1.7 2008/12/05 16:37:55 reyk Exp $	*/

/*
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <net/if.h>

#include <limits.h>
#include <event.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pwd.h>
#include <err.h>

#include <openssl/ssl.h>

#include "relayd.h"

void	 script_sig_alarm(int);

extern struct imsgbuf		*ibuf_main;
pid_t				 child = -1;

void
check_script(struct host *host)
{
	struct ctl_script	 scr;

	host->last_up = host->up;
	host->flags &= ~(F_CHECK_SENT|F_CHECK_DONE);

	scr.host = host->conf.id;
	imsg_compose(ibuf_main, IMSG_SCRIPT, 0, 0, -1, &scr, sizeof(scr));
}

void
script_done(struct relayd *env, struct ctl_script *scr)
{
	struct host		*host;

	if ((host = host_find(env, scr->host)) == NULL)
		fatalx("hce_dispatch_parent: invalid host id");

	if (scr->retval < 0)
		host->up = HOST_UNKNOWN;
	else if (scr->retval == 0)
		host->up = HOST_DOWN;
	else
		host->up = HOST_UP;
	host->flags |= F_CHECK_DONE;

	hce_notify_done(host, host->up == HOST_UP ?
	    HCE_SCRIPT_OK : HCE_SCRIPT_FAIL);
}

void
script_sig_alarm(int sig)
{
	int save_errno = errno;

	if (child != -1)
		kill(child, SIGKILL);
	errno = save_errno;
}

int
script_exec(struct relayd *env, struct ctl_script *scr)
{
	int			 status = 0, ret = 0;
	sig_t			 save_quit, save_int, save_chld;
	struct itimerval	 it;
	struct timeval		*tv;
	const char		*file, *arg;
	struct host		*host;
	struct table		*table;
	struct passwd		*pw;

	if ((host = host_find(env, scr->host)) == NULL)
		fatalx("script_exec: invalid host id");
	if ((table = table_find(env, host->conf.tableid)) == NULL)
		fatalx("script_exec: invalid table id");

	arg = host->conf.name;
	file = table->conf.path;
	tv = &table->conf.timeout;

	save_quit = signal(SIGQUIT, SIG_IGN);
	save_int = signal(SIGINT, SIG_IGN);
	save_chld = signal(SIGCHLD, SIG_DFL);

	switch (child = fork()) {
	case -1:
		ret = -1;
		goto done;
	case 0:
		signal(SIGQUIT, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGCHLD, SIG_DFL);

		if ((pw = getpwnam(RELAYD_USER)) == NULL)
			fatal("script_exec: getpwnam");
		if (chdir("/") == -1)
			fatal("script_exec: chdir(\"/\")");
		if (setgroups(1, &pw->pw_gid) ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
			fatal("script_exec: can't drop privileges");

		/*
		 * close fds before executing an external program, to
		 * prevent access to internal fds, eg. IMSG connections
		 * of internal processes.
		 */
		closefrom(STDERR_FILENO + 1);

		execlp(file, file, arg, (char *)NULL);
		_exit(0);
		break;
	default:
		/* Kill the process after a timeout */
		signal(SIGALRM, script_sig_alarm);
		bzero(&it, sizeof(it));
		bcopy(tv, &it.it_value, sizeof(it.it_value));
		setitimer(ITIMER_REAL, &it, NULL);

		waitpid(child, &status, 0);
		break;
	}

	switch (ret) {
	case -1:
		ret = -1;
		break;
	default:
		if (WIFEXITED(status))
			ret = WEXITSTATUS(status);
		else
			ret = -1;
	}

 done:
	/* Disable the process timeout timer */
	bzero(&it, sizeof(it));
	setitimer(ITIMER_REAL, &it, NULL);
	child = -1;

	signal(SIGQUIT, save_quit);
	signal(SIGINT, save_int);
	signal(SIGCHLD, save_chld);
	signal(SIGALRM, SIG_DFL);

	return (ret);
}
