/*	$OpenBSD: nfsd.c,v 1.19 2002/07/03 22:32:33 deraadt Exp $	*/
/*	$NetBSD: nfsd.c,v 1.19 1996/02/18 23:18:56 mycroft Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
"@(#) Copyright (c) 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)nfsd.c	8.9 (Berkeley) 3/29/95";
#else
static char rcsid[] = "$NetBSD: nfsd.c,v 1.19 1996/02/18 23:18:56 mycroft Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

#ifdef ISO
#include <netiso/iso.h>
#endif
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

/* Global defs */
#ifdef DEBUG
#define	syslog(e, s)	fprintf(stderr,(s))
int	debug = 1;
#else
int	debug = 0;
#endif

struct	nfsd_srvargs nsd;

void	nonfs(int);
void	reapchild(int);
void	usage(void);

/*
 * Nfs server daemon mostly just a user context for nfssvc()
 *
 * 1 - do file descriptor and signal cleanup
 * 2 - fork the nfsd(s)
 * 3 - create server socket(s)
 * 4 - register socket with portmap
 *
 * For connectionless protocols, just pass the socket into the kernel via.
 * nfssvc().
 * For connection based sockets, loop doing accepts. When you get a new
 * socket from accept, pass the msgsock into the kernel via. nfssvc().
 * The arguments are:
 *	-c - support iso cltp clients
 *	-r - reregister with portmapper
 *	-t - support tcp nfs clients
 *	-u - support udp nfs clients
 * followed by "n" which is the number of nfsds' to fork off
 */
int
main(int argc, char *argv[])
{
	struct nfsd_args nfsdargs;
	struct sockaddr_in inetaddr, inetpeer;
#ifdef ISO
	struct sockaddr_iso isoaddr, isopeer;
#endif
	fd_set ready, sockbits;
	int ch, cltpflag, connect_type_cnt, i, maxsock = 0, msgsock;
	int nfsdcnt, nfssvc_flag, on, reregister, sock, tcpflag, tcpsock;
	int tp4cnt, tpipcnt, udpflag;
	socklen_t len;

#define	MAXNFSDCNT	20
#define	DEFNFSDCNT	 4
	nfsdcnt = DEFNFSDCNT;
	cltpflag = reregister = tcpflag = tp4cnt = tpipcnt = 0;
	tcpsock = udpflag = 0;
#ifdef ISO
#define	GETOPT	"cn:rtu"
#define	USAGE	"[-crtu] [-n num_servers]"
#else
#define	GETOPT	"n:rtu"
#define	USAGE	"[-rtu] [-n num_servers]"
#endif
	while ((ch = getopt(argc, argv, GETOPT)) != -1)
		switch (ch) {
		case 'n':
			nfsdcnt = atoi(optarg);
			if (nfsdcnt < 1 || nfsdcnt > MAXNFSDCNT) {
				warnx("nfsd count %d; reset to %d",
				    nfsdcnt, DEFNFSDCNT);
				nfsdcnt = DEFNFSDCNT;
			}
			break;
		case 'r':
			reregister = 1;
			break;
		case 't':
			tcpflag = 1;
			break;
		case 'u':
			udpflag = 1;
			break;
#ifdef ISO
		case 'c':
			cltpflag = 1;
			break;
#ifdef notyet
		case 'i':
			tp4cnt = 1;
			break;
		case 'p':
			tpipcnt = 1;
			break;
#endif /* notyet */
#endif /* ISO */
		default:
		case '?':
			usage();
		};
	argv += optind;
	argc -= optind;

	/*
	 * XXX
	 * Backward compatibility, trailing number is the count of daemons.
	 */
	if (argc > 1)
		usage();
	if (argc == 1) {
		nfsdcnt = atoi(argv[0]);
		if (nfsdcnt < 1 || nfsdcnt > MAXNFSDCNT) {
			warnx("nfsd count %d; reset to %d",
			    nfsdcnt, DEFNFSDCNT);
			nfsdcnt = DEFNFSDCNT;
		}
	}

	if (debug == 0) {
		daemon(0, 0);
		(void)signal(SIGHUP, SIG_IGN);
		(void)signal(SIGINT, SIG_IGN);
		(void)signal(SIGQUIT, SIG_IGN);
		(void)signal(SIGSYS, nonfs);
	}
	(void)signal(SIGCHLD, reapchild);

	if (reregister) {
		if (udpflag &&
		    (!pmap_set(RPCPROG_NFS, 2, IPPROTO_UDP, NFS_PORT) ||
		     !pmap_set(RPCPROG_NFS, 3, IPPROTO_UDP, NFS_PORT)))
			err(1, "can't register with portmap for UDP.");
		if (tcpflag &&
		    (!pmap_set(RPCPROG_NFS, 2, IPPROTO_TCP, NFS_PORT) ||
		     !pmap_set(RPCPROG_NFS, 3, IPPROTO_TCP, NFS_PORT)))
			err(1, "can't register with portmap for TCP.");
		return (0);
	}
	openlog("nfsd:", LOG_PID, LOG_DAEMON);

	for (i = 0; i < nfsdcnt; i++) {
		switch (fork()) {
		case -1:
			syslog(LOG_ERR, "fork: %m");
			return (1);
		case 0:
			break;
		default:
			continue;
		}

		setproctitle("server");
		nfssvc_flag = NFSSVC_NFSD;
		nsd.nsd_nfsd = NULL;
		while (nfssvc(nfssvc_flag, &nsd) < 0) {
			if (errno != ENEEDAUTH) {
				syslog(LOG_ERR, "nfssvc: %m");
				return (1);
			}
			nfssvc_flag = NFSSVC_NFSD | NFSSVC_AUTHINFAIL;
		}
		return (0);
	}

	/* If we are serving udp, set up the socket. */
	if (udpflag) {
		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			syslog(LOG_ERR, "can't create udp socket");
			return (1);
		}
		memset(&inetaddr, 0, sizeof inetaddr);
		inetaddr.sin_family = AF_INET;
		inetaddr.sin_addr.s_addr = INADDR_ANY;
		inetaddr.sin_port = htons(NFS_PORT);
		inetaddr.sin_len = sizeof(inetaddr);
		if (bind(sock,
		    (struct sockaddr *)&inetaddr, sizeof(inetaddr)) < 0) {
			syslog(LOG_ERR, "can't bind udp addr");
			return (1);
		}
		if (!pmap_set(RPCPROG_NFS, 2, IPPROTO_UDP, NFS_PORT) ||
		    !pmap_set(RPCPROG_NFS, 3, IPPROTO_UDP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register with udp portmap");
			return (1);
		}
		nfsdargs.sock = sock;
		nfsdargs.name = NULL;
		nfsdargs.namelen = 0;
		if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
			syslog(LOG_ERR, "can't Add UDP socket");
			return (1);
		}
		(void)close(sock);
	}

#ifdef ISO
	/* If we are serving cltp, set up the socket. */
	if (cltpflag) {
		if ((sock = socket(AF_ISO, SOCK_DGRAM, 0)) < 0) {
			syslog(LOG_ERR, "can't create cltp socket");
			return (1);
		}
		memset(&isoaddr, 0, sizeof(isoaddr));
		isoaddr.siso_family = AF_ISO;
		isoaddr.siso_tlen = 2;
		cp = TSEL(&isoaddr);
		*cp++ = (NFS_PORT >> 8);
		*cp = (NFS_PORT & 0xff);
		isoaddr.siso_len = sizeof(isoaddr);
		if (bind(sock,
		    (struct sockaddr *)&isoaddr, sizeof(isoaddr)) < 0) {
			syslog(LOG_ERR, "can't bind cltp addr");
			return (1);
		}
#ifdef notyet
		/*
		 * XXX
		 * Someday this should probably use "rpcbind", the son of
		 * portmap.
		 */
		if (!pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_UDP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register with udp portmap");
			return (1);
		}
#endif /* notyet */
		nfsdargs.sock = sock;
		nfsdargs.name = NULL;
		nfsdargs.namelen = 0;
		if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
			syslog(LOG_ERR, "can't add UDP socket");
			return (1);
		}
		close(sock);
	}
#endif /* ISO */

	/* Now set up the master server socket waiting for tcp connections. */
	on = 1;
	FD_ZERO(&sockbits);
	connect_type_cnt = 0;
	if (tcpflag) {
		if ((tcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			syslog(LOG_ERR, "can't create tcp socket");
			return (1);
		}
		if (setsockopt(tcpsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %m");
		memset(&inetaddr, 0, sizeof inetaddr);
		inetaddr.sin_family = AF_INET;
		inetaddr.sin_addr.s_addr = INADDR_ANY;
		inetaddr.sin_port = htons(NFS_PORT);
		inetaddr.sin_len = sizeof(inetaddr);
		if (bind(tcpsock,
		    (struct sockaddr *)&inetaddr, sizeof (inetaddr)) < 0) {
			syslog(LOG_ERR, "can't bind tcp addr");
			return (1);
		}
		if (listen(tcpsock, 5) < 0) {
			syslog(LOG_ERR, "listen failed");
			return (1);
		}
		if (!pmap_set(RPCPROG_NFS, 2, IPPROTO_TCP, NFS_PORT) ||
		    !pmap_set(RPCPROG_NFS, 3, IPPROTO_TCP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register tcp with portmap");
			return (1);
		}
		FD_SET(tcpsock, &sockbits);
		maxsock = tcpsock;
		connect_type_cnt++;
	}

#ifdef notyet
	/* Now set up the master server socket waiting for tp4 connections. */
	if (tp4flag) {
		if ((tp4sock = socket(AF_ISO, SOCK_SEQPACKET, 0)) < 0) {
			syslog(LOG_ERR, "can't create tp4 socket");
			return (1);
		}
		if (setsockopt(tp4sock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %m");
		memset(&isoaddr, 0, sizeof(isoaddr));
		isoaddr.siso_family = AF_ISO;
		isoaddr.siso_tlen = 2;
		cp = TSEL(&isoaddr);
		*cp++ = (NFS_PORT >> 8);
		*cp = (NFS_PORT & 0xff);
		isoaddr.siso_len = sizeof(isoaddr);
		if (bind(tp4sock,
		    (struct sockaddr *)&isoaddr, sizeof(isoaddr)) < 0) {
			syslog(LOG_ERR, "can't bind tp4 addr");
			return (1);
		}
		if (listen(tp4sock, 5) < 0) {
			syslog(LOG_ERR, "listen failed");
			return (1);
		}
		/*
		 * XXX
		 * Someday this should probably use "rpcbind", the son of
		 * portmap.
		 */
		if (!pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_TCP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register tcp with portmap");
			return (1);
		}
		FD_SET(tp4sock, &sockbits);
		maxsock = tp4sock;
		connect_type_cnt++;
	}

	/* Now set up the master server socket waiting for tpip connections. */
	if (tpipflag) {
		if ((tpipsock = socket(AF_INET, SOCK_SEQPACKET, 0)) < 0) {
			syslog(LOG_ERR, "can't create tpip socket");
			return (1);
		}
		if (setsockopt(tpipsock,
		    SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			syslog(LOG_ERR, "setsockopt SO_REUSEADDR: %m");
		memset(&inetaddr, 0, sizeof inetaddr);
		inetaddr.sin_family = AF_INET;
		inetaddr.sin_addr.s_addr = INADDR_ANY;
		inetaddr.sin_port = htons(NFS_PORT);
		inetaddr.sin_len = sizeof(inetaddr);
		if (bind(tpipsock,
		    (struct sockaddr *)&inetaddr, sizeof (inetaddr)) < 0) {
			syslog(LOG_ERR, "can't bind tcp addr");
			return (1);
		}
		if (listen(tpipsock, 5) < 0) {
			syslog(LOG_ERR, "listen failed");
			return (1);
		}
		/*
		 * XXX
		 * Someday this should probably use "rpcbind", the son of
		 * portmap.
		 */
		if (!pmap_set(RPCPROG_NFS, NFS_VER2, IPPROTO_TCP, NFS_PORT)) {
			syslog(LOG_ERR, "can't register tcp with portmap");
			return (1);
		}
		FD_SET(tpipsock, &sockbits);
		maxsock = tpipsock;
		connect_type_cnt++;
	}
#endif /* notyet */

	if (connect_type_cnt == 0)
		return (0);

	setproctitle("master");

	/*
	 * Loop forever accepting connections and passing the sockets
	 * into the kernel for the mounts.
	 */
	for (;;) {
		ready = sockbits;
		if (connect_type_cnt > 1) {
			if (select(maxsock + 1,
			    &ready, NULL, NULL, NULL) < 1) {
				syslog(LOG_ERR, "select failed: %m");
				return (1);
			}
		}
		if (tcpflag && FD_ISSET(tcpsock, &ready)) {
			len = sizeof(inetpeer);
			if ((msgsock = accept(tcpsock,
			    (struct sockaddr *)&inetpeer, &len)) < 0) {
				syslog(LOG_ERR, "accept failed: %m");
				return (1);
			}
			memset(inetpeer.sin_zero, 0, sizeof(inetpeer.sin_zero));
			if (setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_ERR,
				    "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&inetpeer;
			nfsdargs.namelen = sizeof(inetpeer);
			nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			(void)close(msgsock);
		}
#ifdef notyet
		if (tp4flag && FD_ISSET(tp4sock, &ready)) {
			len = sizeof(isopeer);
			if ((msgsock = accept(tp4sock,
			    (struct sockaddr *)&isopeer, &len)) < 0) {
				syslog(LOG_ERR, "accept failed: %m");
				return (1);
			}
			if (setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_ERR,
				    "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&isopeer;
			nfsdargs.namelen = len;
			nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			(void)close(msgsock);
		}
		if (tpipflag && FD_ISSET(tpipsock, &ready)) {
			len = sizeof(inetpeer);
			if ((msgsock = accept(tpipsock,
			    (struct sockaddr *)&inetpeer, &len)) < 0) {
				syslog(LOG_ERR, "Accept failed: %m");
				return (1);
			}
			if (setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_ERR, "setsockopt SO_KEEPALIVE: %m");
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&inetpeer;
			nfsdargs.namelen = len;
			nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			(void)close(msgsock);
		}
#endif /* notyet */
	}
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: nfsd %s\n", USAGE);
	exit(1);
}

void
nonfs(int signo)
{
	int save_errno = errno;
	struct syslog_data sdata = SYSLOG_DATA_INIT;

	syslog_r(LOG_ERR, &sdata, "missing system call: NFS not available.");
	errno = save_errno;
}

void
reapchild(int signo)
{
	int save_errno = errno;

	while (wait3(NULL, WNOHANG, NULL) > 0)
		;
	errno = save_errno;
}
