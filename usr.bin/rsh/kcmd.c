/*	$OpenBSD: kcmd.c,v 1.15 2002/02/17 19:42:31 millert Exp $	*/
/*	$NetBSD: kcmd.c,v 1.2 1995/03/21 07:58:32 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
#if 0
static char Xsccsid[] = "derived from @(#)rcmd.c 5.17 (Berkeley) 6/27/88";
static char sccsid[] = "@(#)kcmd.c	8.2 (Berkeley) 8/19/93";
#else
static char rcsid[] = "$OpenBSD: kcmd.c,v 1.15 2002/02/17 19:42:31 millert Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <des.h>
#include <kerberosIV/krb.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <err.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define	START_PORT	5120	 /* arbitrary */

int	getport(int *);
int	kcmd(int *, char **, u_short, char *, char *, char *,
	    int *, KTEXT, char *, char *, CREDENTIALS *,
	    Key_schedule, MSG_DAT *, struct sockaddr_in *,
	    struct sockaddr_in *, long);

int
kcmd(sock, ahost, rport, locuser, remuser, cmd, fd2p, ticket, service, realm,
    cred, schedule, msg_data, laddr, faddr, authopts)
	int *sock;
	char **ahost;
	u_short rport;
	char *locuser, *remuser, *cmd;
	int *fd2p;
	KTEXT ticket;
	char *service;
	char *realm;
	CREDENTIALS *cred;
	Key_schedule schedule;
	MSG_DAT *msg_data;
	struct sockaddr_in *laddr, *faddr;
	long authopts;
{
	int s, timo = 1, pid;
	sigset_t mask, oldmask;
	struct sockaddr_in sin, from;
	char c;
	int lport = IPPORT_RESERVED - 1;
	struct hostent *hp;
	int rc;
	char *host_save;
	int status;

	pid = getpid();
	hp = gethostbyname(*ahost);
	if (hp == NULL) {
		herror(*ahost);
		return (-1);
	}
	if ((host_save = strdup(hp->h_name)) == NULL) {
		warn("can't allocate memory");
		return (-1);
	}
	*ahost = host_save;

	/* If realm is null, look up from table */
	if (realm == NULL || realm[0] == '\0')
		realm = krb_realmofhost(host_save);

	sigemptyset(&mask);
	sigaddset(&mask, SIGURG);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);
	for (;;) {
		s = getport(&lport);
		if (s < 0) {
			if (errno == EAGAIN)
				fprintf(stderr,
					"kcmd(socket): All ports in use\n");
			else
				perror("kcmd: socket");
			sigprocmask(SIG_SETMASK, &oldmask, NULL);
			return (-1);
		}
		fcntl(s, F_SETOWN, pid);
		bzero(&sin, sizeof sin);
		sin.sin_len = sizeof(struct sockaddr_in);
		sin.sin_family = hp->h_addrtype;
		sin.sin_port = rport;
		bcopy(hp->h_addr_list[0], &sin.sin_addr, hp->h_length);
		if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
			break;
		(void) close(s);
		if (errno == EADDRINUSE) {
			lport--;
			continue;
		}
		/*
		 * don't wait very long for Kerberos rcmd.
		 */
		if (errno == ECONNREFUSED && timo <= 4) {
			/* sleep(timo); don't wait at all here */
			timo *= 2;
			continue;
		}
		if (hp->h_addr_list[1] != NULL) {
			int oerrno = errno;

			fprintf(stderr,
			    "kcmd: connect to address %s: ",
			    inet_ntoa(sin.sin_addr));
			errno = oerrno;
			perror(NULL);
			hp->h_addr_list++;
			bcopy(hp->h_addr_list[0], &sin.sin_addr, hp->h_length);
			fprintf(stderr, "Trying %s...\n",
				inet_ntoa(sin.sin_addr));
			continue;
		}
		if (errno != ECONNREFUSED)
			perror(hp->h_name);
		sigprocmask(SIG_SETMASK, &oldmask, NULL);
		return (-1);
	}
	if (fd2p == 0) {
		write(s, "", 1);
		lport = 0;
	} else {
		char num[8];
		int s2 = getport(&lport), s3;
		int len = sizeof(from);

		if (s2 < 0) {
			status = -1;
			goto bad;
		}
		listen(s2, 1);
		(void) snprintf(num, sizeof(num), "%d", lport);
		if (write(s, num, strlen(num) + 1) != strlen(num) + 1) {
			perror("kcmd(write): setting up stderr");
			(void) close(s2);
			status = -1;
			goto bad;
		}
again:
                s3 = accept(s2, (struct sockaddr *)&from, &len);
		/*
		 * XXX careful for ftp bounce attacks. If discovered, shut them
		 * down and check for the real auxiliary channel to connect.
		 */
		if (from.sin_family == AF_INET && from.sin_port == htons(20)) {
			(void) close(s3);
			goto again;
		}
		(void) close(s2);
		if (s3 < 0) {
			perror("kcmd:accept");
			lport = 0;
			status = -1;
			goto bad;
		}
		*fd2p = s3;
		from.sin_port = ntohs(from.sin_port);
		if (from.sin_family != AF_INET ||
		    from.sin_port >= IPPORT_RESERVED ||
		    from.sin_port < IPPORT_RESERVED / 2) {
			fprintf(stderr,
			 "kcmd(socket): protocol failure in circuit setup.\n");
			status = -1;
			goto bad2;
		}
	}
	/*
	 * Kerberos-authenticated service.  Don't have to send locuser,
	 * since its already in the ticket, and we'll extract it on
	 * the other side.
	 */
	/* (void) write(s, locuser, strlen(locuser)+1); */

	/* set up the needed stuff for mutual auth, but only if necessary */
	if (authopts & KOPT_DO_MUTUAL) {
		int sin_len;
		*faddr = sin;

		sin_len = sizeof(struct sockaddr_in);
		if (getsockname(s, (struct sockaddr *)laddr, &sin_len) < 0) {
			perror("kcmd(getsockname)");
			status = -1;
			goto bad2;
		}
	}
	if ((status = krb_sendauth(authopts, s, ticket, service, *ahost,
			       realm, (unsigned long) getpid(), msg_data,
			       cred, schedule,
			       laddr,
			       faddr,
			       "KCMDV0.1")) != KSUCCESS)
		goto bad2;

	(void) write(s, remuser, strlen(remuser)+1);
	(void) write(s, cmd, strlen(cmd)+1);

	if ((rc = read(s, &c, 1)) != 1) {
		if (rc == -1)
			perror(*ahost);
		else
			fprintf(stderr,"kcmd: bad connection with remote host\n");
		status = -1;
		goto bad2;
	}
	if (c != '\0') {
		while (read(s, &c, 1) == 1) {
			(void) write(2, &c, 1);
			if (c == '\n')
				break;
		}
		status = -1;
		goto bad2;
	}
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	*sock = s;
	return (KSUCCESS);
bad2:
	if (lport)
		(void) close(*fd2p);
bad:
	(void) close(s);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	return (status);
}

int
getport(alport)
	int *alport;
{
	struct sockaddr_in sin;
	int s;

	/* First try to get a "reserved" [sic] port, for interoperability with
	   broken klogind (aix, e.g.) */

	s = rresvport(alport);
	if (s >= 0)
		return s;

	/* Failed; if EACCES, we're not root, so just get an unreserved port
	   and hope that's good enough */

	if (errno != EACCES)
		return -1;

	if (*alport < IPPORT_RESERVED)
		*alport = START_PORT;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return (-1);
	for (;;) {
		sin.sin_port = htons((u_short)*alport);
		if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) >= 0)
			return (s);
		if (errno != EADDRINUSE) {
			(void) close(s);
			return (-1);
		}
		(*alport)--;
		if (*alport == IPPORT_RESERVED) {
			(void) close(s);
			errno = EAGAIN;		/* close */
			return (-1);
		}
	}
}
