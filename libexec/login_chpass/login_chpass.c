/*	$OpenBSD: login_chpass.c,v 1.2 2001/10/24 13:06:35 mpech Exp $	*/

/*-
 * Copyright (c) 1995,1996 Berkeley Software Design, Inc. All rights reserved.
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
 *      This product includes software developed by Berkeley Software Design,
 *      Inc.
 * 4. The name of Berkeley Software Design, Inc.  may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI $From: login_chpass.c,v 1.3 1996/08/21 21:01:48 prb Exp $
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/file.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <login_cap.h>

#ifdef YP
# include <netdb.h>
# include <rpc/rpc.h>
# include <rpcsvc/yp_prot.h>
# include <rpcsvc/ypclnt.h>
# define passwd yp_passwd_rec
# include <rpcsvc/yppasswd.h>
# undef passwd
#endif

#ifdef KERBEROS
# include <netinet/in.h>
# include <kerberosIV/krb.h>
# include <kerberosIV/kadm.h>
# include <kerberosIV/kadm_err.h>
#endif

#define	_PATH_LOGIN_LCHPASS	"/usr/libexec/auth/login_lchpass"

#ifdef  YP
int	_yp_check __P((char **));
char	*ypgetnewpasswd __P((struct passwd *, char **));
struct passwd *ypgetpwnam __P((char *));
#endif

#ifdef KERBEROS
int	get_pw_new_pwd __P((char *, int, krb_principal *, int));
char	realm[REALM_SZ];
#endif

void	local_chpass __P((char **));
void	krb_chpass __P((char *, char *, char **));
void	yp_chpass __P((char *));

int
main(argc, argv)
	int argc;
	char *argv[];
{
    	struct rlimit rl;
    	char *username;
    	char *instance;
    	int c;

	rl.rlim_cur = 0;
	rl.rlim_max = 0;
	(void)setrlimit(RLIMIT_CORE, &rl);

	(void)setpriority(PRIO_PROCESS, 0, 0);

	openlog("login", LOG_ODELAY, LOG_AUTH);

    	while ((c = getopt(argc, argv, "s:v:")) != -1)
		switch(c) {
		case 'v':
			break;
		case 's':	/* service */
			if (strcmp(optarg, "login") != 0) {
				syslog(LOG_ERR, "%s: invalid service", optarg);
				exit(1);
			}
			break;
		default:
			syslog(LOG_ERR, "usage error");
			exit(1);
		}

	switch(argc - optind) {
	case 2:
		/* class is not used */
	case 1:
		username = argv[optind];
		break;
	default:
		syslog(LOG_ERR, "usage error");
		exit(1);
	}

	/* Instance ignored for all but Kerberos. */
	instance = strchr(username, '.');
	if (instance)
		*instance++ = '\0';
	else
		instance = "";
	
#ifdef KERBEROS
	if (krb_get_lrealm(realm, 0) == KSUCCESS)
		krb_chpass(username, instance, argv);
#endif
#ifdef  YP
	if (_yp_check(NULL))
		yp_chpass(username);
#endif
	local_chpass(argv);
	/* NOTREACHED */
	exit(0);
}

void
local_chpass(argv)
	char *argv[];
{

	/* login_lchpass doesn't check instance so don't bother restoring it */
	argv[0] = strrchr(_PATH_LOGIN_LCHPASS, '/') + 1;
	execv(_PATH_LOGIN_LCHPASS, argv);
	syslog(LOG_ERR, "%s: %m", _PATH_LOGIN_LCHPASS);
	exit(1);
}

#ifdef YP
void
yp_chpass(username)
	char *username;
{
	FILE *back;
	char *master;
	int r, rpcport, status;
	struct yppasswd yppasswd;
	struct passwd *pw;
	struct timeval tv;
	CLIENT *client;
	extern char *domain;

	if (!(back = fdopen(3, "a")))  {
		syslog(LOG_ERR, "reopening back channel: %m");
		exit(1);
	}

	if ((r = yp_get_default_domain(&domain)) != 0) {
		warnx("can't get local YP domain. Reason: %s", yperr_string(r));
		exit(1);
	}

	/*
	 * Find the host for the passwd map; it should be running
	 * the daemon.
	 */
	if ((r = yp_master(domain, "passwd.byname", &master)) != 0) {
		warnx("can't find the master YP server. Reason: %s",
		    yperr_string(r));
		exit(1);
	}

	/* Ask the portmapper for the port of the daemon. */
	if ((rpcport = getrpcport(master, YPPASSWDPROG,
	    YPPASSWDPROC_UPDATE, IPPROTO_UDP)) == 0) {
		warnx("master YP server not running yppasswd daemon.");
		warnx("Can't change password.");
		exit(1);
	}

	if (rpcport >= IPPORT_RESERVED) {
		warnx("yppasswd daemon is on an invalid port.");
		exit(1);
	}

	/* If user doesn't exist, just prompt for old password and exit. */
	if ((pw = ypgetpwnam(username)) == NULL) {
		char *p = getpass("Old password:");
		crypt(p, "xx");
		memset(p, 0, strlen(p));
		warnx("YP passwd database unchanged.");
		exit(1);
	}

	if (*pw->pw_passwd == '\0') {
		syslog(LOG_ERR, "%s attempting to add password", username);
		fprintf(back, BI_SILENT "\n");
		exit(0);
	}

	/* prompt for new password */
	yppasswd.newpw.pw_passwd = ypgetnewpasswd(pw, &yppasswd.oldpass);

	/* tell rpc.yppasswdd */
	yppasswd.newpw.pw_name	= pw->pw_name;
	yppasswd.newpw.pw_uid 	= pw->pw_uid;
	yppasswd.newpw.pw_gid	= pw->pw_gid;
	yppasswd.newpw.pw_gecos = pw->pw_gecos;
	yppasswd.newpw.pw_dir	= pw->pw_dir;
	yppasswd.newpw.pw_shell	= pw->pw_shell;

	client = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS, "udp");
	if (client == NULL) {
		warnx("cannot contact yppasswdd on %s: Reason: %s",
		    master, yperr_string(YPERR_YPBIND));
		free(yppasswd.newpw.pw_passwd);
		exit(1);
	}
	client->cl_auth = authunix_create_default();
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	r = clnt_call(client, YPPASSWDPROC_UPDATE,
	    xdr_yppasswd, &yppasswd, xdr_int, &status, tv);
	if (r)
		warnx("rpc to yppasswdd failed.");
	else if (status) {
		printf("Couldn't change YP password.\n");
		free(yppasswd.newpw.pw_passwd);
		exit(1);
	}
	printf("The YP password has been changed on %s, the master YP passwd server.\n",
	    master);
	free(yppasswd.newpw.pw_passwd);
	fprintf(back, BI_SILENT "\n");
	exit(0);
}
#endif

#ifdef KERBEROS
void
krb_chpass(username, instance, argv)
	char *username;
	char *instance;
	char *argv[];
{
	FILE *back;
	int rval;
	char pword[MAX_KPW_LEN];
	char tktstring[MAXPATHLEN];
	krb_principal principal;

	if (!(back = fdopen(3, "a")))  {
		syslog(LOG_ERR, "reopening back channel: %m");
		exit(1);
	}

	memset(&principal, 0, sizeof(principal));
	krb_get_default_principal(principal.name,
	    principal.instance, principal.realm);

	snprintf(tktstring, sizeof(tktstring), "%s.chpass.%s.%d",
	    TKT_ROOT, username, getpid());
	krb_set_tkt_string(tktstring);

	(void)setpriority(PRIO_PROCESS, 0, -4);

	if (get_pw_new_pwd(pword, sizeof(pword), &principal, 0)) {
		dest_tkt();
		exit(1);
	}

	rval = kadm_init_link (PWSERV_NAME, KRB_MASTER, principal.realm);
	if (rval != KADM_SUCCESS) 
		com_err(argv[0], rval, "while initializing");
	else {
		des_cblock newkey;
		char *pw_msg; /* message from server */

		des_string_to_key(pword, &newkey);
		rval = kadm_change_pw_plain((u_char *)&newkey, pword, &pw_msg);
		memset(newkey, 0, sizeof(newkey));

		if (rval == KADM_INSECURE_PW)
			warnx("Insecure password: %s", pw_msg);
		else if (rval != KADM_SUCCESS)
			com_err(argv[0], rval, "attempting to change password.");
	}
	memset(pword, 0, sizeof(pword));

	if (rval != KADM_SUCCESS)
		fprintf(stderr, "Password NOT changed.\n");
	else
		printf("Password changed.\n");

	dest_tkt();

	if (rval == 0)
		fprintf(back, BI_SILENT "\n");
    	exit(rval);
}
#endif
