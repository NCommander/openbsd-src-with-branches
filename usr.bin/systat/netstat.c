/*	$OpenBSD: netstat.c,v 1.10 1997/12/19 09:36:50 deraadt Exp $	*/
/*	$NetBSD: netstat.c,v 1.3 1995/06/18 23:53:07 cgd Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
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
static char sccsid[] = "@(#)netstat.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: netstat.c,v 1.10 1997/12/19 09:36:50 deraadt Exp $";
#endif /* not lint */

/*
 * netstat
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>

#include <netinet/in.h>
#include <net/route.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_seq.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_debug.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <nlist.h>
#include <paths.h>
#include "systat.h"
#include "extern.h"

static void enter __P((struct inpcb *, struct socket *, int, char *));
static char *inetname __P((struct in_addr));
static void inetprint __P((struct in_addr *, int, char *));

#define	streq(a,b)	(strcmp(a,b)==0)
#define	YMAX(w)		((w)->_maxy-1)

WINDOW *
opennetstat()
{
	sethostent(1);
	setnetent(1);
	return (subwin(stdscr, LINES-5-1, 0, 5, 0));
}

struct netinfo {
	struct	netinfo *nif_forw, *nif_prev;
	short	nif_line;		/* line on screen */
	short	nif_seen;		/* 0 when not present in list */
	short	nif_flags;
#define	NIF_LACHG	0x1		/* local address changed */
#define	NIF_FACHG	0x2		/* foreign address changed */
	short	nif_state;		/* tcp state */
	char	*nif_proto;		/* protocol */
	struct	in_addr nif_laddr;	/* local address */
	long	nif_lport;		/* local port */
	struct	in_addr	nif_faddr;	/* foreign address */
	long	nif_fport;		/* foreign port */
	long	nif_rcvcc;		/* rcv buffer character count */
	long	nif_sndcc;		/* snd buffer character count */
};

static struct {
	struct	netinfo *nif_forw, *nif_prev;
} netcb;

static	int aflag = 0;
static	int nflag = 0;
static	int lastrow = 1;
static	void enter(), inetprint();
static	char *inetname();

void
closenetstat(w)
        WINDOW *w;
{
	register struct netinfo *p;

	endhostent();
	endnetent();
	p = (struct netinfo *)netcb.nif_forw;
	while (p != (struct netinfo *)&netcb) {
		if (p->nif_line != -1)
			lastrow--;
		p->nif_line = -1;
		p = p->nif_forw;
	}
        if (w != NULL) {
		wclear(w);
		wrefresh(w);
		delwin(w);
	}
}

static struct nlist namelist[] = {
#define	X_TCBTABLE	0
	{ "_tcbtable" },
#define	X_UDBTABLE	1
	{ "_udbtable" },
	{ "" },
};

int
initnetstat()
{
	if (kvm_nlist(kd, namelist)) {
		nlisterr(namelist);
		return(0);
	}
	if (namelist[X_TCBTABLE].n_value == 0) {
		error("No symbols in namelist");
		return(0);
	}
	netcb.nif_forw = netcb.nif_prev = (struct netinfo *)&netcb;
	protos = TCP|UDP;
	return(1);
}

void
fetchnetstat()
{
	struct inpcbtable pcbtable;
	register struct inpcb *head, *prev, *next;
	register struct netinfo *p;
	struct inpcb inpcb;
	struct socket sockb;
	struct tcpcb tcpcb;
	void *off;
	int istcp;

	if (namelist[X_TCBTABLE].n_value == 0)
		return;
	for (p = netcb.nif_forw; p != (struct netinfo *)&netcb; p = p->nif_forw)
		p->nif_seen = 0;
	if (protos&TCP) {
		off = NPTR(X_TCBTABLE); 
		istcp = 1;
	}
	else if (protos&UDP) {
		off = NPTR(X_UDBTABLE); 
		istcp = 0;
	}
	else {
		error("No protocols to display");
		return;
	}
again:
	KREAD(off, &pcbtable, sizeof (struct inpcbtable));
	prev = head = (struct inpcb *)&((struct inpcbtable *)off)->inpt_queue;
	next = pcbtable.inpt_queue.cqh_first;
	while (next != head) {
		KREAD(next, &inpcb, sizeof (inpcb));
		if (inpcb.inp_queue.cqe_prev != prev) {
printf("prev = %x, head = %x, next = %x, inpcb...prev = %x\n", prev, head, next, inpcb.inp_queue.cqe_prev);
			p = netcb.nif_forw;
			for (; p != (struct netinfo *)&netcb; p = p->nif_forw)
				p->nif_seen = 1;
			error("Kernel state in transition");
			return;
		}
		prev = next;
		next = inpcb.inp_queue.cqe_next;

		if (!aflag && inet_lnaof(inpcb.inp_laddr) == INADDR_ANY)
			continue;
		if (nhosts && !checkhost(&inpcb))
			continue;
		if (nports && !checkport(&inpcb))
			continue;
		KREAD(inpcb.inp_socket, &sockb, sizeof (sockb));
		if (istcp) {
			KREAD(inpcb.inp_ppcb, &tcpcb, sizeof (tcpcb));
			enter(&inpcb, &sockb, tcpcb.t_state, "tcp");
		} else
			enter(&inpcb, &sockb, 0, "udp");
	}
	if (istcp && (protos&UDP)) {
		istcp = 0;
		off = NPTR(X_UDBTABLE);
		goto again;
	}
}

static void
enter(inp, so, state, proto)
	register struct inpcb *inp;
	register struct socket *so;
	int state;
	char *proto;
{
	register struct netinfo *p;

	/*
	 * Only take exact matches, any sockets with
	 * previously unbound addresses will be deleted
	 * below in the display routine because they
	 * will appear as ``not seen'' in the kernel
	 * data structures.
	 */
	for (p = netcb.nif_forw;
	     p != (struct netinfo *)&netcb;
	     p = p->nif_forw) {
		if (!streq(proto, p->nif_proto))
			continue;
		if (p->nif_lport != inp->inp_lport ||
		    p->nif_laddr.s_addr != inp->inp_laddr.s_addr)
			continue;
		if (p->nif_faddr.s_addr == inp->inp_faddr.s_addr &&
		    p->nif_fport == inp->inp_fport)
			break;
	}
	if (p == (struct netinfo *)&netcb) {
		if ((p = malloc(sizeof(*p))) == NULL) {
			error("Out of memory");
			return;
		}
		p->nif_prev = (struct netinfo *)&netcb;
		p->nif_forw = netcb.nif_forw;
		netcb.nif_forw->nif_prev = p;
		netcb.nif_forw = p;
		p->nif_line = -1;
		p->nif_laddr = inp->inp_laddr;
		p->nif_lport = inp->inp_lport;
		p->nif_faddr = inp->inp_faddr;
		p->nif_fport = inp->inp_fport;
		p->nif_proto = proto;
		p->nif_flags = NIF_LACHG|NIF_FACHG;
	}
	p->nif_rcvcc = so->so_rcv.sb_cc;
	p->nif_sndcc = so->so_snd.sb_cc;
	p->nif_state = state;
	p->nif_seen = 1;
}

/* column locations */
#define	LADDR	0
#define	FADDR	LADDR+23
#define	PROTO	FADDR+23
#define	RCVCC	PROTO+6
#define	SNDCC	RCVCC+7
#define	STATE	SNDCC+7


void
labelnetstat()
{
	if (namelist[X_TCBTABLE].n_type == 0)
		return;
	wmove(wnd, 0, 0); wclrtobot(wnd);
	mvwaddstr(wnd, 0, LADDR, "Local Address");
	mvwaddstr(wnd, 0, FADDR, "Foreign Address");
	mvwaddstr(wnd, 0, PROTO, "Proto");
	mvwaddstr(wnd, 0, RCVCC, "Recv-Q");
	mvwaddstr(wnd, 0, SNDCC, "Send-Q");
	mvwaddstr(wnd, 0, STATE, "(state)"); 
}

void
shownetstat()
{
	register struct netinfo *p, *q;

	/*
	 * First, delete any connections that have gone
	 * away and adjust the position of connections
	 * below to reflect the deleted line.
	 */
	p = netcb.nif_forw;
	while (p != (struct netinfo *)&netcb) {
		if (p->nif_line == -1 || p->nif_seen) {
			p = p->nif_forw;
			continue;
		}
		wmove(wnd, p->nif_line, 0); wdeleteln(wnd);
		q = netcb.nif_forw;
		for (; q != (struct netinfo *)&netcb; q = q->nif_forw)
			if (q != p && q->nif_line > p->nif_line) {
				q->nif_line--;
				/* this shouldn't be necessary */
				q->nif_flags |= NIF_LACHG|NIF_FACHG;
			}
		lastrow--;
		q = p->nif_forw;
		p->nif_prev->nif_forw = p->nif_forw;
		p->nif_forw->nif_prev = p->nif_prev;
		free(p);
		p = q;
	}
	/*
	 * Update existing connections and add new ones.
	 */
	for (p = netcb.nif_forw;
	     p != (struct netinfo *)&netcb;
	     p = p->nif_forw) {
		if (p->nif_line == -1) {
			/*
			 * Add a new entry if possible.
			 */
			if (lastrow > YMAX(wnd))
				continue;
			p->nif_line = lastrow++;
			p->nif_flags |= NIF_LACHG|NIF_FACHG;
		}
		if (p->nif_flags & NIF_LACHG) {
			wmove(wnd, p->nif_line, LADDR);
			inetprint(&p->nif_laddr, p->nif_lport, p->nif_proto);
			p->nif_flags &= ~NIF_LACHG;
		}
		if (p->nif_flags & NIF_FACHG) {
			wmove(wnd, p->nif_line, FADDR);
			inetprint(&p->nif_faddr, p->nif_fport, p->nif_proto);
			p->nif_flags &= ~NIF_FACHG;
		}
		mvwaddstr(wnd, p->nif_line, PROTO, p->nif_proto);
		mvwprintw(wnd, p->nif_line, RCVCC, "%6d", p->nif_rcvcc);
		mvwprintw(wnd, p->nif_line, SNDCC, "%6d", p->nif_sndcc);
		if (streq(p->nif_proto, "tcp"))
			if (p->nif_state < 0 || p->nif_state >= TCP_NSTATES)
				mvwprintw(wnd, p->nif_line, STATE, "%d",
				    p->nif_state);
			else
				mvwaddstr(wnd, p->nif_line, STATE,
				    tcpstates[p->nif_state]);
		wclrtoeol(wnd);
	}
	if (lastrow < YMAX(wnd)) {
		wmove(wnd, lastrow, 0); wclrtobot(wnd);
		wmove(wnd, YMAX(wnd), 0); wdeleteln(wnd);	/* XXX */
	}
}

/*
 * Pretty print an Internet address (net address + port).
 * If the nflag was specified, use numbers instead of names.
 */
static void
inetprint(in, port, proto)
	register struct in_addr *in;
	int port;
	char *proto;
{
	struct servent *sp = 0;
	char line[80], *cp;

	snprintf(line, sizeof line, "%.*s.", 16, inetname(*in));
	cp = strchr(line, '\0');
	if (!nflag && port)
		sp = getservbyport(port, proto);
	if (sp || port == 0)
		snprintf(cp, sizeof line - strlen(cp), "%.8s",
		    sp ? sp->s_name : "*");
	else
		snprintf(cp, sizeof line - strlen(cp), "%d",
		    ntohs((u_short)port));
	/* pad to full column to clear any garbage */
	cp = strchr(line, '\0');
	while (cp - line < 22 && cp - line < sizeof line-1)
		*cp++ = ' ';
	*cp = '\0';
	waddstr(wnd, line);
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give 
 * numeric value, otherwise try for symbolic name.
 */
static char *
inetname(in)
	struct in_addr in;
{
	char *cp = 0;
	static char line[50];
	struct hostent *hp;
	struct netent *np;

	if (!nflag && in.s_addr != INADDR_ANY) {
		int net = inet_netof(in);
		int lna = inet_lnaof(in);

		if (lna == INADDR_ANY) {
			np = getnetbyaddr(net, AF_INET);
			if (np)
				cp = np->n_name;
		}
		if (cp == 0) {
			hp = gethostbyaddr((char *)&in, sizeof (in), AF_INET);
			if (hp)
				cp = hp->h_name;
		}
	}
	if (in.s_addr == INADDR_ANY) {
		strncpy(line, "*", sizeof line-1);
		line[sizeof line-1] = '\0';
	} else if (cp) {
		strncpy(line, cp, sizeof line-1);
		line[sizeof line-1] = '\0';
	} else {
		in.s_addr = ntohl(in.s_addr);
#define C(x)	((x) & 0xff)
		snprintf(line, sizeof line, "%u.%u.%u.%u", C(in.s_addr >> 24),
			C(in.s_addr >> 16), C(in.s_addr >> 8), C(in.s_addr));
	}
	return (line);
}

int
cmdnetstat(cmd, args)
	char *cmd, *args;
{
	register struct netinfo *p;

	if (prefix(cmd, "all")) {
		aflag = !aflag;
		goto fixup;
	}
	if  (prefix(cmd, "numbers") || prefix(cmd, "names")) {
		int new;

		new = prefix(cmd, "numbers");
		if (new == nflag)
			return (1);
		p = netcb.nif_forw;
		for (; p != (struct netinfo *)&netcb; p = p->nif_forw) {
			if (p->nif_line == -1)
				continue;
			p->nif_flags |= NIF_LACHG|NIF_FACHG;
		}
		nflag = new;
		wclear(wnd);
		labelnetstat();
		goto redisplay;
	}
	if (!netcmd(cmd, args))
		return (0);
fixup:
	fetchnetstat();
redisplay:
	shownetstat();
	refresh();
	return (1);
}
