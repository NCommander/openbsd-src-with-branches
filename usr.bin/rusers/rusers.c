/*	$OpenBSD: rusers.c,v 1.14 2001/11/01 23:37:42 millert Exp $	*/

/*
 * Copyright (c) 2001 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 *  Copyright (c) 1993 John Brezak
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] = "$OpenBSD: rusers.c,v 1.14 2001/11/01 23:37:42 millert Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_rmt.h>
#include <rpcsvc/rusers.h>
#include <rpcsvc/rnusers.h>	/* Old protocol version */
#include <arpa/inet.h>
#include <net/if.h>
#include <err.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Preferred formatting */
#define HOST_WIDTH 17
#define LINE_WIDTH 8
#define NAME_WIDTH 8

int search_host(struct in_addr);
void remember_host(char **);
void fmt_idle(int, char *, size_t);
void print_longline(int, u_int, char *, char *, char *, char *, int);
void onehost(char *);
void allhosts(void);
void alarmclock(int);
bool_t rusers_reply(char *, struct sockaddr_in *);
bool_t rusers_reply_3(char *, struct sockaddr_in *);
enum clnt_stat get_reply(int, in_port_t, u_long, struct rpc_msg *,
    struct rmtcallres *, bool_t (*)());
__dead void usage(void);

int longopt;
int allopt;
long termwidth;
extern char *__progname;

struct host_list {
	struct host_list *next;
	struct in_addr addr;
} *hosts;

#define MAX_BROADCAST_SIZE 1400

int
main(int argc, char **argv)
{
	struct winsize win;
	char *cp, *ep;
	int ch;
	
	while ((ch = getopt(argc, argv, "al")) != -1)
		switch (ch) {
		case 'a':
			allopt++;
			break;
		case 'l':
			longopt++;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}

	if (isatty(STDOUT_FILENO)) {
		if ((cp = getenv("COLUMNS")) != NULL && *cp != '\0') {
			termwidth = strtol(cp, &ep, 10);
			if (*ep != '\0' || termwidth >= INT_MAX ||
			    termwidth < 0)
				termwidth = 0;
		}
		if (termwidth == 0 &&
		    ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) == 0 &&
		    win.ws_col > 0)
			termwidth = win.ws_col;
		else
			termwidth = 80;
	}
	setlinebuf(stdout);
	if (argc == optind)
		allhosts();
	else {
		for (; optind < argc; optind++)
			(void) onehost(argv[optind]);
	}
	exit(0);
}

int
search_host(struct in_addr addr)
{
	struct host_list *hp;
	
	if (!hosts)
		return(0);

	for (hp = hosts; hp != NULL; hp = hp->next) {
		if (hp->addr.s_addr == addr.s_addr)
			return(1);
	}
	return(0);
}

void
remember_host(char **ap)
{
	struct host_list *hp;

	for (; *ap; ap++) {
		if (!(hp = malloc(sizeof(struct host_list))))
			err(1, NULL);
		hp->addr.s_addr = *(in_addr_t *)*ap;
		hp->next = hosts;
		hosts = hp;
	}
}

void
fmt_idle(int idle, char *idle_time, size_t idle_time_len)
{
	int days, hours, minutes, seconds;

	switch (idle) {
	case 0:
		*idle_time = '\0';
		break;
	case INT_MAX:
		strlcpy(idle_time, "??", idle_time_len);
		break;
	default:
		seconds = idle;
		days = seconds / (60*60*24);
		seconds %= (60*60*24);
		hours = seconds / (60*60);
		seconds %= (60*60);
		minutes = seconds / 60;
		seconds %= 60;
		if (idle >= (24*60*60))
			snprintf(idle_time, idle_time_len,
			    "%d days, %d:%02d:%02d",
			    days, hours, minutes, seconds);
		else if (idle >= (60*60))
			snprintf(idle_time, idle_time_len, "%2d:%02d:%02d",
			    hours, minutes, seconds);
		else if (idle > 60)
			snprintf(idle_time, idle_time_len, "%2d:%02d",
			    minutes, seconds);
		else
			snprintf(idle_time, idle_time_len, "   :%02d", idle);
		break;
	}
}

void
print_longline(int ut_time, u_int idle, char *host, char *user, char *line,
	       char *remhost, int remhostmax)
{
	char date[32], idle_time[64];
	char remote[RUSERS_MAXHOSTLEN + 1];
	int len;

	strftime(date, sizeof(date), "%h %d %R", localtime((time_t *)&ut_time));
	date[sizeof(date) - 1] = '\0';
	fmt_idle(idle, idle_time, sizeof(idle_time));
	len = termwidth -
	    (MAX(strlen(user), NAME_WIDTH) + 1 + HOST_WIDTH + 1 + LINE_WIDTH +
	    1 + strlen(date) + 1 + MAX(8, strlen(idle_time)) + 1 + 2);
	if (len > 0 && *remhost != '\0')
		snprintf(remote, sizeof(remote), "(%.*s)",
		    MIN(len, remhostmax), remhost);
	else
		remote[0] = '\0';
	len = HOST_WIDTH - MIN(HOST_WIDTH, strlen(host)) +
	    LINE_WIDTH - MIN(LINE_WIDTH, strlen(line));
	printf("%-*s %.*s:%.*s%-*s %-12s %8s %s\n",
	    NAME_WIDTH, user, HOST_WIDTH, host, LINE_WIDTH, line,
	    len, "", date, idle_time, remote);
}

bool_t
rusers_reply(char *replyp, struct sockaddr_in *raddrp)
{
	char user[RNUSERS_MAXUSERLEN + 1];
	char utline[RNUSERS_MAXLINELEN + 1];
	utmpidlearr *up = (utmpidlearr *)replyp;
	struct hostent *hp;
	char *host, *taddrs[2];
	int i;
	
	if (search_host(raddrp->sin_addr))
		return(0);

	if (!allopt && !up->uia_cnt)
		return(0);
	
	hp = gethostbyaddr((char *)&raddrp->sin_addr,
	    sizeof(struct in_addr), AF_INET);
	if (hp) {
		host = hp->h_name;
		remember_host(hp->h_addr_list);
	} else {
		host = inet_ntoa(raddrp->sin_addr);
		taddrs[0] = (char *)&raddrp->sin_addr;
		taddrs[1] = NULL;
		remember_host(taddrs);
	}
	
	if (!longopt)
		printf("%-*.*s ", HOST_WIDTH, HOST_WIDTH, host);
	
	for (i = 0; i < up->uia_cnt; i++) {
		/* NOTE: strncpy() used below for non-terminated strings. */
		strncpy(user, up->uia_arr[i]->ui_utmp.ut_name,
		    sizeof(user) - 1);
		user[sizeof(user) - 1] = '\0';
		if (longopt) {
			strncpy(utline, up->uia_arr[i]->ui_utmp.ut_line,
			    sizeof(utline) - 1);
			utline[sizeof(utline) - 1] = '\0';
			print_longline(up->uia_arr[i]->ui_utmp.ut_time,
			    up->uia_arr[i]->ui_idle, host, user, utline,
			    up->uia_arr[i]->ui_utmp.ut_host, RNUSERS_MAXHOSTLEN);
		} else {
			fputs(user, stdout);
			putchar(' ');
		}
	}
	if (!longopt)
		putchar('\n');
	
	return(0);
}

bool_t
rusers_reply_3(char *replyp, struct sockaddr_in *raddrp)
{
	char user[RUSERS_MAXUSERLEN + 1];
	char utline[RUSERS_MAXLINELEN + 1];
	utmp_array *up3 = (utmp_array *)replyp;
	struct hostent *hp;
	char *host, *taddrs[2];
	int i;
	
	if (search_host(raddrp->sin_addr))
		return(0);

	if (!allopt && !up3->utmp_array_len)
		return(0);

	hp = gethostbyaddr((char *)&raddrp->sin_addr,
	    sizeof(struct in_addr), AF_INET);
	if (hp) {
		host = hp->h_name;
		remember_host(hp->h_addr_list);
	} else {
		host = inet_ntoa(raddrp->sin_addr);
		taddrs[0] = (char *)&raddrp->sin_addr;
		taddrs[1] = NULL;
		remember_host(taddrs);
	}
	
	if (!longopt)
		printf("%-*.*s ", HOST_WIDTH, HOST_WIDTH, host);
	
	for (i = 0; i < up3->utmp_array_len; i++) {
		/* NOTE: strncpy() used below for non-terminated strings. */
		strncpy(user, up3->utmp_array_val[i].ut_user,
		    sizeof(user) - 1);
		user[sizeof(user) - 1] = '\0';
		if (longopt) {
			strncpy(utline, up3->utmp_array_val[i].ut_line,
			    sizeof(utline) - 1);
			utline[sizeof(utline) - 1] = '\0';
			print_longline(up3->utmp_array_val[i].ut_time,
			    up3->utmp_array_val[i].ut_idle, host, user, utline,
			    up3->utmp_array_val[i].ut_host, RUSERS_MAXHOSTLEN);
		} else {
			fputs(user, stdout);
			putchar(' ');
		}
	}
	if (!longopt)
		putchar('\n');
	
	return(0);
}

void
onehost(char *host)
{
	utmpidlearr up;
	utmp_array up3;
	CLIENT *rusers_clnt;
	struct sockaddr_in sin;
	struct hostent *hp;
	struct timeval tv = { 25, 0 };
	int error;
	
	hp = gethostbyname(host);
	if (hp == NULL)
		errx(1, "unknown host \"%s\"", host);

	/* try version 3 first */
	rusers_clnt = clnt_create(host, RUSERSPROG, RUSERSVERS_3, "udp");
	if (rusers_clnt == NULL) {
		clnt_pcreateerror(__progname);
		exit(1);
	}

	memset(&up3, 0, sizeof(up3));
	error = clnt_call(rusers_clnt, RUSERSPROC_NAMES, xdr_void, NULL,
	    xdr_utmp_array, &up3, tv);
	switch (error) {
	case RPC_SUCCESS:
		sin.sin_addr.s_addr = *(int *)hp->h_addr;
		rusers_reply_3((char *)&up3, &sin);
		clnt_destroy(rusers_clnt);
		return;
	case RPC_PROGVERSMISMATCH:
		clnt_destroy(rusers_clnt);
		break;
	default:
		clnt_perror(rusers_clnt, __progname);
		clnt_destroy(rusers_clnt);
		exit(1);
	}

	/* fall back to version 2 */
	rusers_clnt = clnt_create(host, RUSERSPROG, RUSERSVERS_IDLE, "udp");
	if (rusers_clnt == NULL) {
		clnt_pcreateerror(__progname);
		exit(1);
	}

	memset(&up, 0, sizeof(up));
	error = clnt_call(rusers_clnt, RUSERSPROC_NAMES, xdr_void, NULL,
	    xdr_utmpidlearr, &up, tv);
	if (error != RPC_SUCCESS) {
		clnt_perror(rusers_clnt, __progname);
		clnt_destroy(rusers_clnt);
		exit(1);
	}
	sin.sin_addr.s_addr = *(int *)hp->h_addr;
	rusers_reply((char *)&up, &sin);
	clnt_destroy(rusers_clnt);
}

enum clnt_stat
get_reply(int sock, in_port_t port, u_long xid, struct rpc_msg *msgp,
	  struct rmtcallres *resp, bool_t (*callback)())
{
	ssize_t inlen;
	socklen_t fromlen;
	struct sockaddr_in raddr;
	char inbuf[UDPMSGSIZE];
	XDR xdr;

retry:
	msgp->acpted_rply.ar_verf = _null_auth;
	msgp->acpted_rply.ar_results.where = (caddr_t)resp;
	msgp->acpted_rply.ar_results.proc = xdr_rmtcallres;

	fromlen = sizeof(struct sockaddr);
	inlen = recvfrom(sock, inbuf, sizeof(inbuf), 0,
	    (struct sockaddr *)&raddr, &fromlen);
	if (inlen < 0) {
		if (errno == EINTR)
			goto retry;
		return (RPC_CANTRECV);
	}
	if (inlen < sizeof(u_int32_t))
		goto retry;

	/*
	 * If the reply we got matches our request, decode the
	 * replay and pass it to the callback function.
	 */
	xdrmem_create(&xdr, inbuf, (u_int)inlen, XDR_DECODE);
	if (xdr_replymsg(&xdr, msgp)) {
		if ((msgp->rm_xid == xid) &&
		    (msgp->rm_reply.rp_stat == MSG_ACCEPTED) &&
		    (msgp->acpted_rply.ar_stat == SUCCESS)) {
			raddr.sin_port = htons(port);
			(void)(*callback)(resp->results_ptr, &raddr);
		}
	}
	xdr.x_op = XDR_FREE;
	msgp->acpted_rply.ar_results.proc = xdr_void;
	(void)xdr_replymsg(&xdr, msgp);
	(void)(*resp->xdr_results)(&xdr, resp->results_ptr);
	xdr_destroy(&xdr);

	return(RPC_SUCCESS);
}

enum clnt_stat
rpc_setup(int *fdp, XDR *xdr, struct rpc_msg *msg, struct rmtcallargs *args,
	  AUTH *unix_auth, char *buf)
{
	int on = 1;

	if ((*fdp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		return(RPC_CANTSEND);

	if (setsockopt(*fdp, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) < 0)
		return(RPC_CANTSEND);

	msg->rm_xid = arc4random();
	msg->rm_direction = CALL;
	msg->rm_call.cb_rpcvers = RPC_MSG_VERSION;
	msg->rm_call.cb_prog = PMAPPROG;
	msg->rm_call.cb_vers = PMAPVERS;
	msg->rm_call.cb_proc = PMAPPROC_CALLIT;
	msg->rm_call.cb_cred = unix_auth->ah_cred;
	msg->rm_call.cb_verf = unix_auth->ah_verf;

	xdrmem_create(xdr, buf, MAX_BROADCAST_SIZE, XDR_ENCODE);
	if (!xdr_callmsg(xdr, msg) || !xdr_rmtcall_args(xdr, args))
		return(RPC_CANTENCODEARGS);

	return(RPC_SUCCESS);
}

void
allhosts(void)
{
	enum clnt_stat stat;
	struct itimerval timeout;
	AUTH *unix_auth = authunix_create_default();
	size_t outlen[2];
	int sock[2] = { -1, -1 };
	int i, maxfd, rval;
	u_long xid[2], port[2];
	fd_set *fds = NULL;
	struct sockaddr_in *sin, baddr;
	struct rmtcallargs args;
	struct rmtcallres res[2];
	struct rpc_msg msg[2];
	struct ifaddrs *ifa, *ifap = NULL;
	char buf[2][MAX_BROADCAST_SIZE];
	utmpidlearr up;
	utmp_array up3;
	XDR xdr;

	if (getifaddrs(&ifap) != 0)
		err(1, "can't get list of interface addresses");

	memset(&up, 0, sizeof(up));
	memset(&up3, 0, sizeof(up3));
	memset(&baddr, 0, sizeof(baddr));
	memset(&res, 0, sizeof(res));
	memset(&msg, 0, sizeof(msg));

	args.prog = RUSERSPROG;
	args.vers = RUSERSVERS_IDLE;
	args.proc = RUSERSPROC_NAMES;
	args.xdr_args = xdr_void;
	args.args_ptr = NULL;

	stat = rpc_setup(&sock[0], &xdr, &msg[0], &args, unix_auth, buf[0]);
	if (stat != RPC_SUCCESS)
		goto cleanup;
	xid[0] = msg[0].rm_xid;
	outlen[0] = xdr_getpos(&xdr);
	xdr_destroy(&xdr);

	args.vers = RUSERSVERS_3;
	stat = rpc_setup(&sock[1], &xdr, &msg[1], &args, unix_auth, buf[1]);
	if (stat != RPC_SUCCESS)
		goto cleanup;
	xid[1] = msg[1].rm_xid;
	outlen[1] = xdr_getpos(&xdr);
	xdr_destroy(&xdr);

	maxfd = MAX(sock[0], sock[1]) + 1;
	fds = (fd_set *)calloc(howmany(maxfd, NFDBITS), sizeof(fd_mask));
	if (fds == NULL)
		err(1, NULL);

	baddr.sin_len = sizeof(struct sockaddr_in);
	baddr.sin_family = AF_INET;
	baddr.sin_port = htons(PMAPPORT);
	baddr.sin_addr.s_addr = htonl(INADDR_ANY);

	res[0].port_ptr = &port[0];
	res[0].xdr_results = xdr_utmpidlearr;
	res[0].results_ptr = (caddr_t)&up;

	res[1].port_ptr = &port[1];
	res[1].xdr_results = xdr_utmp_array;
	res[1].results_ptr = (caddr_t)&up3;

	(void)signal(SIGALRM, alarmclock);

	/*
	 * We do 6 runs through the loop.  On even runs we send
	 * a version 3 broadcast.  On odd ones we send a version 2
	 * broadcast.  This should give version 3 replies enough
	 * of an 'edge' over the old version 2 ones in most cases.
	 * We select() waiting for replies for 5 seconds in between
	 * each broadcast.
	 */
	for (i = 0; i < 6; i++) {
		for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr->sa_family != AF_INET ||
			    !(ifa->ifa_flags & IFF_BROADCAST) ||
			    !(ifa->ifa_flags & IFF_UP) ||
			    ifa->ifa_broadaddr == NULL ||
			    ifa->ifa_broadaddr->sa_family != AF_INET)
				continue;
			sin = (struct sockaddr_in *)ifa->ifa_broadaddr;
			baddr.sin_addr = sin->sin_addr;

			/* use protocol 2 or 3 depending on i (odd or even) */
			if (i & 1) {
				if (sendto(sock[0], buf[0], outlen[0], 0,
				    (struct sockaddr *)&baddr,
				    sizeof(struct sockaddr)) != outlen[0])
					err(1, "can't send broadcast packet");
			} else {
				if (sendto(sock[1], buf[1], outlen[1], 0,
				    (struct sockaddr *)&baddr,
				    sizeof(struct sockaddr)) != outlen[1])
					err(1, "can't send broadcast packet");
			}
		}

		/*
		 * We stay in the select loop for ~5 seconds
		 */
		timerclear(&timeout.it_value);
		timeout.it_value.tv_sec = 5;
		timeout.it_value.tv_usec = 0;
		for (;;) {
			FD_SET(sock[0], fds);
			FD_SET(sock[1], fds);
			setitimer(ITIMER_REAL, &timeout, NULL);
			rval = select(maxfd, fds, NULL, NULL, NULL);
			setitimer(ITIMER_REAL, NULL, &timeout);
			if (rval == -1) {
				if (errno == EINTR)
					break;
				err(1, "select");	/* shouldn't happen */
			}
			if (FD_ISSET(sock[1], fds)) {
				stat = get_reply(sock[1], (in_port_t)port[1],
				    xid[1], &msg[1], &res[1], rusers_reply_3);
				if (stat != RPC_SUCCESS)
					goto cleanup;
			}
			if (FD_ISSET(sock[0], fds)) {
				stat = get_reply(sock[0], (in_port_t)port[0],
				    xid[0], &msg[0], &res[0], rusers_reply);
				if (stat != RPC_SUCCESS)
					goto cleanup;
			}
		}
	}
cleanup:
	if (ifap != NULL)
		freeifaddrs(ifap);
	if (fds != NULL)
		free(fds);
	if (sock[0] >= 0)
		(void)close(sock[0]);
	if (sock[1] >= 0)
		(void)close(sock[1]);
	AUTH_DESTROY(unix_auth);
	if (stat != RPC_SUCCESS) {
		clnt_perrno(stat);
		exit(1);
	}
}

void
alarmclock(int signo)
{

	;		/* just interupt */
}

void
usage(void)
{

	fprintf(stderr, "usage: %s [-la] [hosts ...]\n", __progname);
	exit(1);
}
