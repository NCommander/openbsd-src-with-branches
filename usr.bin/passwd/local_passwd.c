/*	$OpenBSD: local_passwd.c,v 1.24 2001/12/07 04:15:08 millert Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
 */

#ifndef lint
/*static const char sccsid[] = "from: @(#)local_passwd.c	5.5 (Berkeley) 5/6/91";*/
static const char rcsid[] = "$OpenBSD: local_passwd.c,v 1.24 2001/12/07 04:15:08 millert Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <login_cap.h>

static uid_t uid;
extern int pwd_gensalt(char *, int, struct passwd *, login_cap_t *, char);
extern int pwd_check(struct passwd *, login_cap_t *, char *);
extern int pwd_gettries(struct passwd *, login_cap_t *);

char *getnewpasswd(struct passwd *, login_cap_t *, int);
void kbintr(int);

int
local_passwd(uname, authenticated)
	char *uname;
	int authenticated;
{
	struct passwd *pw;
	login_cap_t *lc;
	sigset_t fullset;
	time_t period;
	int i, pfd, tfd = -1;
	int pwflags = _PASSWORD_OMITV7;

	if (!(pw = getpwnam(uname))) {
#ifdef YP
		extern int use_yp;
		if (!use_yp)
#endif
		warnx("unknown user %s.", uname);
		return(1);
	}
	if ((lc = login_getclass(pw->pw_class)) == NULL) {
		warnx("unable to get login class for user %s.", uname);
		return(1);
	}

	uid = authenticated ? pw->pw_uid : getuid();
	if (uid && uid != pw->pw_uid) {
		warnx("login/uid mismatch, username argument required.");
		return(1);
	}

	/* Get the new password. */
	pw->pw_passwd = getnewpasswd(pw, lc, authenticated);

	/* Reset password change time based on login.conf. */
	period = login_getcaptime(lc, "passwordtime", 0, 0);
	if (period > 0) {
		pw->pw_change = time(NULL) + period;
	} else {
		/*
		 * If the pw change time is the same we only need
		 * to update the spwd.db file.
		 */
		if (pw->pw_change != 0)
			pw->pw_change = 0;
		else
			pwflags = _PASSWORD_SECUREONLY;
	}

	/* Drop user's real uid and block all signals to avoid a DoS. */
	setuid(0);
	sigfillset(&fullset);
	sigdelset(&fullset, SIGINT);
	sigprocmask(SIG_BLOCK, &fullset, NULL);

	/* Get a lock on the passwd file and open it. */
	pw_init();
	for (i = 1; (tfd = pw_lock(0)) == -1; i++) {
		if (i == 4)
			(void)fputs("Attempting lock password file, "
			    "please wait or press ^C to abort", stderr);
		(void)signal(SIGINT, kbintr);
		if (i % 16 == 0)
			fputc('.', stderr);
		usleep(250000);
		(void)signal(SIGINT, SIG_IGN);
	}
	if (i >= 4)
		fputc('\n', stderr);
	pfd = open(_PATH_MASTERPASSWD, O_RDONLY, 0);
	if (pfd < 0 || fcntl(pfd, F_SETFD, 1) == -1)
		pw_error(_PATH_MASTERPASSWD, 1, 1);

	/* Update master.passwd file and rebuild spwd.db. */
	pw_copy(pfd, tfd, pw);
	if (pw_mkdb(uname, pwflags) < 0)
		pw_error(NULL, 0, 1);

	return(0);
}

char *
getnewpasswd(pw, lc, authenticated)
	struct passwd *pw;
	login_cap_t *lc;
	int authenticated;
{
	char *p;
	int tries, pwd_tries;
	char buf[_PASSWORD_LEN+1], salt[_PASSWORD_LEN];
	sig_t saveint, savequit;

	saveint = signal(SIGINT, kbintr);
	savequit = signal(SIGQUIT, kbintr);

	if (!authenticated) {
		(void)printf("Changing local password for %s.\n", pw->pw_name);
		if (uid && pw->pw_passwd[0] &&
		    strcmp(crypt(getpass("Old password:"), pw->pw_passwd),
		    pw->pw_passwd)) {
			errno = EACCES;
			pw_error(NULL, 1, 1);
		}
	}
	
	pwd_tries = pwd_gettries(pw, lc);

	for (buf[0] = '\0', tries = 0;;) {
		p = getpass("New password:");
		if (!*p) {
			(void)printf("Password unchanged.\n");
			pw_error(NULL, 0, 0);
		}
		if (strcmp(p, "s/key") == 0) {
			printf("That password collides with a system feature. Choose another.\n");
			continue;
		}
		
		if ((tries++ < pwd_tries || pwd_tries == 0) 
		    && pwd_check(pw, lc, p) == 0)
			continue;
		strlcpy(buf, p, sizeof(buf));
		if (!strcmp(buf, getpass("Retype new password:")))
			break;
		(void)printf("Mismatch; try again, EOF to quit.\n");
	}
	if (!pwd_gensalt(salt, _PASSWORD_LEN, pw, lc, 'l')) {
		(void)printf("Couldn't generate salt.\n");
		pw_error(NULL, 0, 0);
	}
	(void)signal(SIGINT, saveint);
	(void)signal(SIGQUIT, savequit);

	return(crypt(buf, salt));
}

void
kbintr(signo)
	int signo;
{
	char msg[] = "\nPassword unchanged.\n";
	struct iovec iv[5];
	extern char *__progname;

	iv[0].iov_base = msg;
	iv[0].iov_len = sizeof(msg) - 1;
	iv[1].iov_base = __progname;
	iv[1].iov_len = strlen(__progname);
	iv[2].iov_base = ": ";
	iv[2].iov_len = 2;
	iv[3].iov_base = _PATH_MASTERPASSWD;
	iv[3].iov_len = sizeof(_PATH_MASTERPASSWD) - 1;
	iv[4].iov_base = " unchanged\n";
	iv[4].iov_len = 11;
	writev(STDERR_FILENO, iv, 5);

	_exit(1);
}
