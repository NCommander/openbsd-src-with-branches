/*
 * Copyright (c) 1994 Mats O Jansson <moj@stacken.kth.se>
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef LINT
static char rcsid[] = "$Id: yppasswdd_mkpw.c,v 1.2 1994/05/29 10:20:26 moj Exp root $";
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpcsvc/yppasswd.h>
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#include "yplog.h"

extern int noshell;
extern int nogecos;
extern int nopw;
extern int make;
extern char make_arg[];

int
make_passwd(argp)
	yppasswd *argp;
{
	struct passwd *pw;
	int pfd, tfd;
	
	yplog_line("enter make_passwd");

	if (!(pw = getpwnam(argp->newpw.pw_name))) {
	  yplog_date("yppasswdd: unknown user");
	  yplog_line(argp->newpw.pw_name);
	  return(TRUE);
	}
	
	yplog_line("get user done");

	if (strcmp(crypt(argp->oldpass, pw->pw_passwd), pw->pw_passwd) != 0) {
	  yplog_date("yppasswdd: incorrect password");
	  yplog_line(argp->newpw.pw_name);
	  return(TRUE);
	}
	
	yplog_line("password ok");

	pw_init();
	pfd = pw_lock();
	tfd = pw_tmp();

	/*
	 * Get the new password.  Reset passwd change time to zero; when
	 * classes are implemented, go and get the "offset" value for this
	 * class and reset the timer.
	 */
	if (!(nopw)) {
	  pw->pw_passwd = argp->newpw.pw_passwd;
	  pw->pw_change = 0;
	}

	if (!(nogecos)) {
	  pw->pw_gecos = argp->newpw.pw_gecos;
	}

	if (!(noshell)) {
	  pw->pw_shell = argp->newpw.pw_shell;
	}

	yplog_line("before pw_copy");

	pw_copy(pfd, tfd, pw);

	yplog_line("before pw_mkdb");

/*
	if (!pw_mkdb())
		pw_error((char *)NULL, 0, 0);
*/
	pw_mkdb();

	yplog_line("before fork");

	if (fork() == 0) {
	  chdir("/var/yp");
	  (void) umask(022);
	  system(make_arg);
	  exit(0);
	}
	
	yplog_line("exit make_passwd");

	return(FALSE);

};

/*
int
do_mkdb()
{
	int pstat;
	pid_t pid;

	(void)printf("%s: rebuilding the database...\n", progname);
	(void)fflush(stdout);
	if (!(pid = vfork())) {
		execl(_PATH_PWD_MKDB, "pwd_mkdb", "-p", tempname, NULL);
		pw_error(_PATH_PWD_MKDB, 1, 1);
	}
	pid = waitpid(pid, &pstat, 0);
	if (pid == -1 || !WIFEXITED(pstat) || WEXITSTATUS(pstat) != 0)
		return(0);
	(void)printf("%s: done\n", progname);
	return(1);
}
*/
