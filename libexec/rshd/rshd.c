/*-
 * Copyright (c) 1988, 1989, 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
static char copyright[] =
"@(#) Copyright (c) 1988, 1989, 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/* from: static char sccsid[] = "@(#)rshd.c	8.2 (Berkeley) 4/6/94"; */
static char *rcsid = "$Id: rshd.c,v 1.29 1999/11/15 01:03:28 deraadt Exp $";
#endif /* not lint */

/*
 * remote shell server:
 *	[port]\0
 *	remuser\0
 *	locuser\0
 *	command\0
 *	data
 */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

int	keepalive = 1;
int	check_all;
int	log_success;		/* If TRUE, log all successful accesses */
int	sent_null;

void	 doit __P((struct sockaddr *));
void	 error __P((const char *, ...));
void	 getstr __P((char *, int, char *));
int	 local_domain __P((char *));
char	*topdomain __P((char *));
void	 usage __P((void));

#ifdef	KERBEROS
#include <des.h>
#include <kerberosIV/krb.h>
#define	VERSION_SIZE	9
#define SECURE_MESSAGE  "This rsh session is using DES encryption for all transmissions.\r\n"

#ifdef CRYPT
#define OPTIONS		"alnkvxL"
#else
#define	OPTIONS		"alnkvL"
#endif

char	authbuf[sizeof(AUTH_DAT)];
char	tickbuf[sizeof(KTEXT_ST)];
int	doencrypt, use_kerberos, vacuous;
des_key_schedule schedule;
#ifdef CRYPT
int des_read __P((int, char *, int));
int des_write __P((int, char *, int));
void desrw_clear_key __P(());
void desrw_set_key __P((des_cblock *, des_key_schedule *));
#endif
#else
#define	OPTIONS	"alnL"
#endif

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int __check_rhosts_file;
	struct linger linger;
	int ch, on = 1, fromlen;
	struct sockaddr_storage from;

	openlog("rshd", LOG_PID | LOG_ODELAY, LOG_DAEMON);

	opterr = 0;
	while ((ch = getopt(argc, argv, OPTIONS)) != -1)
		switch (ch) {
		case 'a':
			check_all = 1;
			break;
		case 'l':
			__check_rhosts_file = 0;
			break;
		case 'n':
			keepalive = 0;
			break;
#ifdef	KERBEROS
		case 'k':
			use_kerberos = 1;
			break;

		case 'v':
			vacuous = 1;
			break;

#ifdef CRYPT
		case 'x':
			doencrypt = 1;
			break;
#endif
#endif
		case 'L':
			log_success = 1;
			break;
		case '?':
		default:
			usage();
			break;
		}

	argc -= optind;
	argv += optind;

#ifdef	KERBEROS
	if (use_kerberos && vacuous) {
		syslog(LOG_ERR, "only one of -k and -v allowed");
		exit(2);
	}
#ifdef CRYPT
	if (doencrypt && !use_kerberos) {
		syslog(LOG_ERR, "-k is required for -x");
		exit(2);
	}
#endif
#endif

	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0) {
		/* syslog(LOG_ERR, "getpeername: %m"); */
		exit(1);
	}
	if (keepalive &&
	    setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, (char *)&on,
	    sizeof(on)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	linger.l_onoff = 1;
	linger.l_linger = 60;			/* XXX */
	if (setsockopt(0, SOL_SOCKET, SO_LINGER, (char *)&linger,
	    sizeof (linger)) < 0)
		syslog(LOG_WARNING, "setsockopt (SO_LINGER): %m");
	doit((struct sockaddr *)&from);
	/* NOTREACHED */
	return 0;
}

char	username[20] = "USER=";
char	logname[23] = "LOGNAME=";
char	homedir[64] = "HOME=";
char	shell[64] = "SHELL=";
char	path[100] = "PATH=";
char	*envinit[] =
	    {homedir, shell, path, logname, username, 0};
char	**environ;

void
doit(fromp)
	struct sockaddr *fromp;
{
	extern char *__rcmd_errstr;	/* syslog hook from libc/net/rcmd.c. */
	struct addrinfo hints, *res, *res0;
	int gaierror;
	struct passwd *pwd;
	u_short port;
	in_port_t *portp;
	fd_set ready, readfrom;
	int cc, nfd, pv[2], pid, s = 0;
	int one = 1;
	char *hostname, *errorstr, *errorhost = (char *) NULL;
	char *cp, sig, buf[BUFSIZ];
	char cmdbuf[NCARGS+1], locuser[16], remuser[16];
	char remotehost[2 * MAXHOSTNAMELEN + 1];
	char hostnamebuf[2 * MAXHOSTNAMELEN + 1];
	char naddr[NI_MAXHOST];
	char saddr[NI_MAXHOST];
	char raddr[NI_MAXHOST];
	char pbuf[NI_MAXSERV];
#ifdef NI_WITHSCOPEID
	const int niflags = NI_NUMERICHOST | NI_NUMERICSERV | NI_WITHSCOPEID;
#else
	const int niflags = NI_NUMERICHOST | NI_NUMERICSERV;
#endif

#ifdef	KERBEROS
	AUTH_DAT	*kdata = (AUTH_DAT *) NULL;
	KTEXT		ticket = (KTEXT) NULL;
	char		instance[INST_SZ], version[VERSION_SIZE];
	struct		sockaddr_storage fromaddr;
	int		rc;
	long		authopts;
#ifdef CRYPT
	int		pv1[2], pv2[2];
	fd_set		wready, writeto;
#endif

	if (sizeof(fromaddr) < fromp->sa_len) {
		syslog(LOG_ERR, "malformed \"from\" address (af %d)",
		    fromp->sa_family);
		exit(1);
	}
	memcpy(&fromaddr, fromp, fromp->sa_len);
#endif

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
#ifdef DEBUG
	{ int t = open(_PATH_TTY, 2);
	  if (t >= 0) {
		ioctl(t, TIOCNOTTY, (char *)0);
		(void) close(t);
	  }
	}
#endif
	switch (fromp->sa_family) {
	case AF_INET:
		portp = &((struct sockaddr_in *)fromp)->sin_port;
		break;
	case AF_INET6:
		portp = &((struct sockaddr_in6 *)fromp)->sin6_port;
		break;
	default:
		syslog(LOG_ERR, "malformed \"from\" address (af %d)",
		    fromp->sa_family);
		exit(1);
	}
	if (getnameinfo(fromp, fromp->sa_len, naddr, sizeof(naddr),
	    pbuf, sizeof(pbuf), niflags) != 0) {
		syslog(LOG_ERR, "malformed \"from\" address (af %d)",
		    fromp->sa_family);
		exit(1);
	}

#ifdef IP_OPTIONS
	if (fromp->sa_family == AF_INET)
      {
	struct ipoption opts;
	int optsize = sizeof(opts), ipproto, i;
	struct protoent *ip;

	if ((ip = getprotobyname("ip")) != NULL)
		ipproto = ip->p_proto;
	else
		ipproto = IPPROTO_IP;
	if (!getsockopt(0, ipproto, IP_OPTIONS, (char *)&opts, &optsize) &&
	    optsize != 0) {
		for (i = 0; (void *)&opts.ipopt_list[i] - (void *)&opts <
		    optsize; ) {	
			u_char c = (u_char)opts.ipopt_list[i];
			if (c == IPOPT_LSRR || c == IPOPT_SSRR)
				exit(1);
			if (c == IPOPT_EOL)
				break;
			i += (c == IPOPT_NOP) ? 1 : (u_char)opts.ipopt_list[i+1];
		}
	}
      }
#endif

#ifdef	KERBEROS
	if (!use_kerberos)
#endif
		if (ntohs(*portp) >= IPPORT_RESERVED ||
		    ntohs(*portp) < IPPORT_RESERVED/2) {
			syslog(LOG_NOTICE|LOG_AUTH,
			    "Connection from %s on illegal port %u",
			    naddr, ntohs(*portp));
			exit(1);
		}

	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;
		if ((cc = read(STDIN_FILENO, &c, 1)) != 1) {
			if (cc < 0)
				syslog(LOG_NOTICE, "read: %m");
			shutdown(0, 1+1);
			exit(1);
		}
		if (c== 0)
			break;
		port = port * 10 + c - '0';
	}

	(void) alarm(0);
	if (port != 0) {
		int lport;
#ifdef	KERBEROS
		if (!use_kerberos)
#endif
			if (port >= IPPORT_RESERVED ||
			    port < IPPORT_RESERVED/2) {
				syslog(LOG_ERR, "2nd port not reserved");
				exit(1);
			}
		*portp = htons(port);
		lport = IPPORT_RESERVED - 1;
		s = rresvport_af(&lport, fromp->sa_family);
		if (s < 0) {
			syslog(LOG_ERR, "can't get stderr port: %m");
			exit(1);
		}
		if (connect(s, (struct sockaddr *)fromp, fromp->sa_len) < 0) {
			syslog(LOG_INFO, "connect second port %d: %m", port);
			exit(1);
		}
	}

#ifdef	KERBEROS
	if (vacuous) {
		error("rshd: remote host requires Kerberos authentication\n");
		exit(1);
	}
#endif

#ifdef notdef
	/* from inetd, socket is already on 0, 1, 2 */
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);
#endif
	errorstr = NULL;
	if (getnameinfo(fromp, fromp->sa_len, saddr, sizeof(saddr),
			NULL, 0, NI_NAMEREQD)== 0) {
		/*
		 * If name returned by gethostbyaddr is in our domain,
		 * attempt to verify that we haven't been fooled by someone
		 * in a remote net; look up the name and check that this
		 * address corresponds to the name.
		 */
		hostname = saddr;
#ifdef	KERBEROS
		if (!use_kerberos)
#endif
		if (check_all || local_domain(saddr)) {
			strncpy(remotehost, saddr, sizeof(remotehost) - 1);
			remotehost[sizeof(remotehost) - 1] = 0;
			errorhost = remotehost;
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = fromp->sa_family;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_CANONNAME;
			gaierror = getaddrinfo(remotehost, pbuf, &hints, &res0);
			if (gaierror) {
				syslog(LOG_INFO,
				    "Couldn't look up address for %s: %s",
				    remotehost, gai_strerror(gaierror));
				errorstr =
				"Couldn't look up address for your host (%s)\n";
				hostname = naddr;
			} else {
				for (res = res0; res; res = res->ai_next) {
					if (res->ai_family != fromp->sa_family)
						continue;
					if (res->ai_addrlen != fromp->sa_len)
						continue;
					if (getnameinfo(res->ai_addr,
						res->ai_addrlen,
						raddr, sizeof(raddr), NULL, 0,
						niflags) == 0
					 && strcmp(naddr, raddr) == 0) {
						hostname = res->ai_canonname
							? res->ai_canonname
							: saddr;
						break;
					}
				}
				if (res == NULL) {
					syslog(LOG_NOTICE,
					  "Host addr %s not listed for host %s",
					    naddr, res0->ai_canonname
							? res0->ai_canonname
							: saddr);
					errorstr =
					    "Host address mismatch for %s\n";
					hostname = naddr;
				}
				freeaddrinfo(res);
			}
		}
		hostname = strncpy(hostnamebuf, hostname,
		    sizeof(hostnamebuf) - 1);
	} else
		errorhost = hostname = strncpy(hostnamebuf,
		    naddr, sizeof(hostnamebuf) - 1);

	hostnamebuf[sizeof(hostnamebuf) - 1] = '\0';
#ifdef	KERBEROS
	if (use_kerberos) {
		kdata = (AUTH_DAT *) authbuf;
		ticket = (KTEXT) tickbuf;
		authopts = 0L;
		strcpy(instance, "*");
		version[VERSION_SIZE - 1] = '\0';
#ifdef CRYPT
		if (doencrypt) {
			struct sockaddr_in local_addr;
			rc = sizeof(local_addr);
			if (getsockname(0, (struct sockaddr *)&local_addr,
			    &rc) < 0) {
				syslog(LOG_ERR, "getsockname: %m");
				error("rshd: getsockname: %m");
				exit(1);
			}
			authopts = KOPT_DO_MUTUAL;
			rc = krb_recvauth(authopts, 0, ticket,
				"rcmd", instance, &fromaddr,
				&local_addr, kdata, "", schedule,
				version);
			desrw_set_key(&kdata->session, &schedule);
		} else
#endif
			rc = krb_recvauth(authopts, 0, ticket, "rcmd",
				instance, &fromaddr,
				(struct sockaddr_in *) 0,
				kdata, "", (struct des_ks_struct *) 0,
				version);
		if (rc != KSUCCESS) {
			error("Kerberos authentication failure: %s\n",
				  krb_get_err_text(rc));
			exit(1);
		}
	} else
#endif

	getstr(remuser, sizeof(remuser), "remuser");
	getstr(locuser, sizeof(locuser), "locuser");
	getstr(cmdbuf, sizeof(cmdbuf), "command");
	setpwent();
	pwd = getpwnam(locuser);
	if (pwd == NULL) {
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: unknown login. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
		if (errorstr == NULL)
			errorstr = "Permission denied.\n";
		goto fail;
	}

	setegid(pwd->pw_gid);
	seteuid(pwd->pw_uid);
	if (chdir(pwd->pw_dir) < 0) {
		(void) chdir("/");
#ifdef notdef
		syslog(LOG_INFO|LOG_AUTH,
		    "%s@%s as %s: no home directory. cmd='%.80s'",
		    remuser, hostname, locuser, cmdbuf);
		error("No remote directory.\n");
		exit(1);
#endif
	}
	seteuid(0);
	setegid(0);	/* XXX use a saved gid instead? */

#ifdef	KERBEROS
	if (use_kerberos) {
		if (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0') {
			if (kuserok(kdata, locuser) != 0) {
				syslog(LOG_INFO|LOG_AUTH,
				    "Kerberos rsh denied to %s.%s@%s",
				    kdata->pname, kdata->pinst, kdata->prealm);
				error("Permission denied.\n");
				exit(1);
			}
		}
	} else
#endif
	if (errorstr ||
	    (pwd->pw_passwd != 0 && *pwd->pw_passwd != '\0' &&
	    iruserok_sa(fromp, fromp->sa_len, pwd->pw_uid == 0,
	    remuser, locuser) < 0)) {
		if (__rcmd_errstr)
			syslog(LOG_INFO|LOG_AUTH,
			    "%s@%s as %s: permission denied (%s). cmd='%.80s'",
			    remuser, hostname, locuser, __rcmd_errstr,
			    cmdbuf);
		else
			syslog(LOG_INFO|LOG_AUTH,
			    "%s@%s as %s: permission denied. cmd='%.80s'",
			    remuser, hostname, locuser, cmdbuf);
fail:
		if (errorstr == NULL)
			errorstr = "Permission denied.\n";
		error(errorstr, errorhost);
		exit(1);
	}

	if (pwd->pw_uid && !access(_PATH_NOLOGIN, F_OK)) {
		error("Logins currently disabled.\n");
		exit(1);
	}

	(void) write(STDERR_FILENO, "\0", 1);
	sent_null = 1;

	if (port) {
		if (pipe(pv) < 0) {
			error("Can't make pipe.\n");
			exit(1);
		}
#ifdef CRYPT
#ifdef KERBEROS
		if (doencrypt) {
			if (pipe(pv1) < 0) {
				error("Can't make 2nd pipe.\n");
				exit(1);
			}
			if (pipe(pv2) < 0) {
				error("Can't make 3rd pipe.\n");
				exit(1);
			}
		}
#endif
#endif
		pid = fork();
		if (pid == -1)  {
			error("Can't fork; try again.\n");
			exit(1);
		}
		if (pid) {
#ifdef CRYPT
#ifdef KERBEROS
			if (doencrypt) {
				static char msg[] = SECURE_MESSAGE;
				(void) close(pv1[1]);
				(void) close(pv2[1]);
				des_write(s, msg, sizeof(msg) - 1);

			} else
#endif
#endif
			{
				(void) close(0);
				(void) close(1);
			}
			(void) close(2);
			(void) close(pv[1]);

			FD_ZERO(&readfrom);
			FD_SET(s, &readfrom);
			FD_SET(pv[0], &readfrom);
			if (pv[0] > s)
				nfd = pv[0];
			else
				nfd = s;
#ifdef CRYPT
#ifdef KERBEROS
			if (doencrypt) {
				FD_ZERO(&writeto);
				FD_SET(pv2[0], &writeto);
				FD_SET(pv1[0], &readfrom);

				nfd = MAX(nfd, pv2[0]);
				nfd = MAX(nfd, pv1[0]);
			} else
#endif
#endif
				ioctl(pv[0], FIONBIO, (char *)&one);

			/* should set s nbio! */
			nfd++;
			do {
				ready = readfrom;
#ifdef CRYPT
#ifdef KERBEROS
				if (doencrypt) {
					wready = writeto;
					if (select(nfd, &ready,
					    &wready, (fd_set *) 0,
					    (struct timeval *) 0) < 0)
						break;
				} else
#endif
#endif
					if (select(nfd, &ready, (fd_set *)0,
					  (fd_set *)0, (struct timeval *)0) < 0)
						break;
				if (FD_ISSET(s, &ready)) {
					int	ret;
#ifdef CRYPT
#ifdef KERBEROS
					if (doencrypt)
						ret = des_read(s, &sig, 1);
					else
#endif
#endif
						ret = read(s, &sig, 1);
					if (ret <= 0)
						FD_CLR(s, &readfrom);
					else
						killpg(pid, sig);
				}
				if (FD_ISSET(pv[0], &ready)) {
					errno = 0;
					cc = read(pv[0], buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(s, 1+1);
						FD_CLR(pv[0], &readfrom);
					} else {
#ifdef CRYPT
#ifdef KERBEROS
						if (doencrypt)
							(void)
							  des_write(s, buf, cc);
						else
#endif
#endif
							(void)
							  write(s, buf, cc);
					}
				}
#ifdef CRYPT
#ifdef KERBEROS
				if (doencrypt && FD_ISSET(pv1[0], &ready)) {
					errno = 0;
					cc = read(pv1[0], buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(pv1[0], 1+1);
						FD_CLR(pv1[0], &readfrom);
					} else
						(void) des_write(STDOUT_FILENO,
						    buf, cc);
				}

				if (doencrypt && FD_ISSET(pv2[0], &wready)) {
					errno = 0;
					cc = des_read(STDIN_FILENO,
					    buf, sizeof(buf));
					if (cc <= 0) {
						shutdown(pv2[0], 1+1);
						FD_CLR(pv2[0], &writeto);
					} else
						(void) write(pv2[0], buf, cc);
				}
#endif
#endif

			} while (FD_ISSET(s, &readfrom) ||
#ifdef CRYPT
#ifdef KERBEROS
			    (doencrypt && FD_ISSET(pv1[0], &readfrom)) ||
#endif
#endif
			    FD_ISSET(pv[0], &readfrom));
			exit(0);
		}
		setpgrp(0, getpid());
		(void) close(s);
		(void) close(pv[0]);
#ifdef CRYPT
#ifdef KERBEROS
		if (doencrypt) {
			close(pv1[0]); close(pv2[0]);
			dup2(pv1[1], 1);
			dup2(pv2[1], 0);
			close(pv1[1]);
			close(pv2[1]);
		}
#endif
#endif
		dup2(pv[1], 2);
		close(pv[1]);
	}
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;
#if	BSD > 43
	if (setlogin(pwd->pw_name) < 0)
		syslog(LOG_ERR, "setlogin() failed: %m");
#endif
	(void) setegid((gid_t)pwd->pw_gid);
	(void) setgid((gid_t)pwd->pw_gid);
	initgroups(pwd->pw_name, pwd->pw_gid);
	(void) seteuid((uid_t)pwd->pw_uid);
	(void) setuid((uid_t)pwd->pw_uid);
	environ = envinit;
	strncat(homedir, pwd->pw_dir, sizeof(homedir)-6);
	strcat(path, _PATH_DEFPATH);
	strncat(shell, pwd->pw_shell, sizeof(shell)-7);
	strncat(logname, pwd->pw_name, sizeof(logname)-9);
	strncat(username, pwd->pw_name, sizeof(username)-6);
	cp = strrchr(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;
	endpwent();
	if (log_success || pwd->pw_uid == 0) {
#ifdef	KERBEROS
		if (use_kerberos)
		    syslog(LOG_INFO|LOG_AUTH,
			"Kerberos shell from %s.%s@%s on %s as %s, cmd='%.80s'",
			kdata->pname, kdata->pinst, kdata->prealm,
			hostname, locuser, cmdbuf);
		else
#endif
		    syslog(LOG_INFO|LOG_AUTH, "%s@%s as %s: cmd='%.80s'",
			remuser, hostname, locuser, cmdbuf);
	}
	execl(pwd->pw_shell, cp, "-c", cmdbuf, 0);
	perror(pwd->pw_shell);
	exit(1);
}

/*
 * Report error to client.  Note: can't be used until second socket has
 * connected to client, or older clients will hang waiting for that
 * connection first.
 */
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#ifdef __STDC__
error(const char *fmt, ...)
#else
error(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int len;
	char *bp, buf[BUFSIZ];
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	bp = buf;
	if (sent_null == 0) {
		*bp++ = 1;
		len = 1;
	} else
		len = 0;
	(void)vsnprintf(bp, sizeof(buf) - 1, fmt, ap);
	(void)write(STDERR_FILENO, buf, len + strlen(bp));
}

void
getstr(buf, cnt, err)
	char *buf, *err;
	int cnt;
{
	char c;

	do {
		if (read(STDIN_FILENO, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0) {
			error("%s too long\n", err);
			exit(1);
		}
	} while (c != 0);
}

/*
 * Check whether host h is in our local domain,
 * defined as sharing the last two components of the domain part,
 * or the entire domain part if the local domain has only one component.
 * If either name is unqualified (contains no '.'),
 * assume that the host is local, as it will be
 * interpreted as such.
 */
int
local_domain(h)
	char *h;
{
	char localhost[MAXHOSTNAMELEN];
	char *p1, *p2;

	localhost[0] = 0;
	(void) gethostname(localhost, sizeof(localhost));
	p1 = topdomain(localhost);
	p2 = topdomain(h);
	if (p1 == NULL || p2 == NULL || !strcasecmp(p1, p2))
		return (1);
	return (0);
}

char *
topdomain(h)
	char *h;
{
	char *p, *maybe = NULL;
	int dots = 0;

	for (p = h + strlen(h); p >= h; p--) {
		if (*p == '.') {
			if (++dots == 2)
				return (p);
			maybe = p;
		}
	}
	return (maybe);
}

void
usage()
{

	syslog(LOG_ERR, "usage: rshd [-%s]", OPTIONS);
	exit(2);
}
