/*	$OpenBSD: local_passwd.c,v 1.15 2001/07/04 17:20:24 millert Exp $	*/

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
static const char rcsid[] = "$OpenBSD: local_passwd.c,v 1.15 2001/07/04 17:20:24 millert Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <util.h>
#include <login_cap.h>

static uid_t uid;
extern int pwd_gensalt __P((char *, int, struct passwd *, login_cap_t *, char));
extern int pwd_check __P((struct passwd *, login_cap_t *, char *));
extern int pwd_gettries __P((struct passwd *, login_cap_t *));

char *getnewpasswd __P((struct passwd *, login_cap_t *, int));
void kbintr __P((int));

int
local_passwd(uname, authenticated)
	char *uname;
	int authenticated;
{
	struct passwd *pw;
	login_cap_t *lc;
	sigset_t fullset;
	time_t period;
	int pfd, tfd = -1;
	char *s = NULL;

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
	if (period > 0)
		pw->pw_change = time(NULL) + period;
	else
		pw->pw_change = 0;

	/* Drop user's real uid and block all signals to avoid a DoS. */
	setuid(0);
	sigfillset(&fullset);
	sigdelset(&fullset, SIGINT);
	sigprocmask(SIG_BLOCK, &fullset, NULL);

	/* Get a lock on the passwd file and open it. */
	pw_init();
	for (;;) {
		int i, c, d;

		(void)fputs("Please wait", stderr);
		for (i = 0; i < (s ? 64 : 8) && (tfd = pw_lock(0)) == -1; i++) {
			(void)signal(SIGINT, kbintr);
			fputc('.', stderr);
			usleep(250000);
			(void)signal(SIGINT, SIG_IGN);
		}
		fputc('\n', stderr);
		if (tfd != -1)
			break;

		/* Unable to lock passwd file, let the user decide. */
		if (errno == EEXIST) {
			if (s == NULL)
				s = "The passwd file is busy,";
			else
				s = "The passwd file is still busy,";
		} else
			s = "Unable to open passwd temp file,";
		(void)fprintf(stderr,
		    "%s do you want to wait until it is available? [y/n] ", s);
		(void)signal(SIGINT, kbintr);
		c = getchar();
		(void)signal(SIGINT, SIG_IGN);
		if (c != '\n')
			while ((d = getchar()) != '\n' && d != EOF)
				;
		if (tolower(c) != 'y') {
			printf("Password unchanged\n");
			exit(1);
		}
	}
	pfd = open(_PATH_MASTERPASSWD, O_RDONLY, 0);
	if (pfd < 0 || fcntl(pfd, F_SETFD, 1) == -1)
		pw_error(_PATH_MASTERPASSWD, 1, 1);

	/* Update master.passwd file and build .db version. */
	pw_copy(pfd, tfd, pw);
	if (pw_mkdb(uname) < 0)
		pw_error((char *)NULL, 0, 1);

	return(0);
}

char *
getnewpasswd(pw, lc, authenticated)
	struct passwd *pw;
	login_cap_t *lc;
	int authenticated;
{
	register char *p;
	int tries, pwd_tries;
	char buf[_PASSWORD_LEN+1], salt[_PASSWORD_LEN];

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
	return(crypt(buf, salt));
}

void
kbintr(signo)
	int signo;
{
	char msg[] = "\nPassword unchanged\n";

	write(STDOUT_FILENO, msg, sizeof(msg) - 1);
	_exit(1);
}
