/*	$OpenBSD: client.c,v 1.5 2000/10/09 22:52:18 brian Exp $	*/

/*
 * Copyright (c) 2000 Network Security Technologies, Inc. http://www.netsec.net
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
 *	This product includes software developed by Network Security
 *	Technologies, Inc.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
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

#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/bpf.h>
#include <net/ppp_defs.h>
#include <errno.h>
#include <string.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <sysexits.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>

#include "pppoe.h"

#define	PPP_PROG	"/usr/sbin/ppp"

#define	STATE_EXPECT_PADO	1
#define	STATE_EXPECT_PADS	2
#define	STATE_EXPECT_SESSION	3

u_int32_t client_cookie = 0;
u_int16_t client_sessionid = 0xffff;
int pppfd = -1;
int client_state = -1;
u_int8_t etherremoteaddr[6], etherlocaladdr[6];

static int getpackets __P((int, char *, char *, struct ether_addr *,
    struct ether_addr *));
static int send_padi __P((int, struct ether_addr *, u_int8_t *));
static int send_padr __P((int, char *, struct ether_addr *,
    struct ether_addr *, struct ether_header *, struct pppoe_header *,
    struct tag_list *));
static int recv_pado __P((int, char *, struct ether_addr *,
    struct ether_addr *, struct ether_header *, struct pppoe_header *,
    u_long, u_int8_t *));
static int recv_pads __P((int, char *, char *, struct ether_addr *,
    struct ether_addr *, struct ether_header *, struct pppoe_header *,
    u_long, u_int8_t *));
static int recv_padt __P((int, struct ether_addr *, struct ether_addr *,
    struct ether_header *, struct pppoe_header *, u_long, u_int8_t *));

void timer_handler __P((int));
int timer_set __P((u_int));
int timer_clr __P((void));
int timer_hit __P((void));

int
client_mode(bfd, sysname, srvname, myea)
	int bfd;
	char *sysname, *srvname;
	struct ether_addr *myea;
{
	struct ether_addr rmea;
	fd_set fds;
	int r = 0, max;

	pppfd = -1;
	client_sessionid = 0xffff;

	r = send_padi(bfd, myea, srvname);
	if (r <= 0)
		return (r);

	FD_ZERO(&fds);
	for (;;) {
		FD_SET(bfd, &fds);
		max = bfd + 1;
		if (pppfd >= 0) {
			if (pppfd >= max)
				max = pppfd + 1;
			FD_SET(pppfd, &fds);
		}

		r = select(max, &fds, NULL, NULL, NULL);
		if (r < 0) {
			if (errno == EINTR) {
				if (timer_hit())
					timer_clr();
				break;
			}
			break;
		}
		if (FD_ISSET(bfd, &fds)) {
			r = getpackets(bfd, srvname, sysname, myea, &rmea);
			if (r <= 0)
				break;
		}
		if (pppfd >= 0 && FD_ISSET(pppfd, &fds)) {
			r = ppp_to_bpf(bfd, pppfd, myea, &rmea,
			    client_sessionid);
			if (r < 0)
				break;
		}
	}

	if (pppfd >= 0) {
		send_padt(bfd, myea, &rmea, client_sessionid);
		pppfd = -1;
	}
	return (r);
}

static int
send_padi(fd, ea, srv)
	int fd;
	struct ether_addr *ea;
	u_int8_t *srv;
{
	struct iovec iov[10];
	struct pppoe_header ph = {
		PPPOE_VERTYPE(1, 1),
		PPPOE_CODE_PADI, 0, 0
	};
	struct pppoe_tag thost, tserv;
	u_int16_t etype = htons(ETHERTYPE_PPPOEDISC);
	int i = 0;

	/* ether_header */
	iov[0].iov_base = etherbroadcastaddr;
	iov[0].iov_len = ETHER_ADDR_LEN;
	iov[1].iov_base = ea;
	iov[1].iov_len = ETHER_ADDR_LEN;
	iov[2].iov_base = &etype;
	iov[2].iov_len = sizeof(etype);

	/* pppoe_header */
	iov[3].iov_base = &ph;
	iov[3].iov_len = sizeof(ph);

	/* host-uniq tag */
	client_cookie = cookie_bake();
	thost.type = htons(PPPOE_TAG_HOST_UNIQ);
	thost.len = htons(sizeof(client_cookie));
	ph.len += sizeof(thost.type)+sizeof(thost.len)+sizeof(client_cookie);
	iov[4].iov_base = &thost;
	iov[4].iov_len = sizeof(thost.len) + sizeof(thost.type) ;
	iov[5].iov_base = &client_cookie;
	iov[5].iov_len = sizeof(client_cookie);

	/* service-name tag */
	tserv.type = htons(PPPOE_TAG_SERVICE_NAME);
	tserv.len = (srv == NULL) ? 0 : strlen(srv);
	tserv.val = srv;
	ph.len += tserv.len + sizeof(tserv.type) + sizeof(tserv.len);
	iov[6].iov_base = &tserv;
	iov[6].iov_len = sizeof(tserv.len) + sizeof(tserv.type);
	i = 7;
	if (tserv.len) {
		iov[7].iov_base = tserv.val;
		iov[7].iov_len = tserv.len;
		i = 8;
	}
	tserv.len = htons(tserv.len);

	ph.len = htons(ph.len);

	client_state = STATE_EXPECT_PADO;
	timer_set(10);
	return (writev(fd, iov, i));
}

static int
send_padr(bfd, srv, myea, rmea, eh, ph, tl)
	int bfd;
	char *srv;
	struct ether_addr *myea, *rmea;
	struct ether_header *eh;
	struct pppoe_header *ph;
	struct tag_list *tl;
{
	struct iovec iov[12];
	u_int16_t etype = htons(ETHERTYPE_PPPOEDISC);
	struct pppoe_tag hutag, svtag;
	struct tag_node *n;
	int idx = 0, slen;

	timer_set(5);
	client_state = STATE_EXPECT_PADS;

	iov[idx].iov_base = rmea;
	iov[idx++].iov_len = ETHER_ADDR_LEN;
	iov[idx].iov_base = myea;
	iov[idx++].iov_len = ETHER_ADDR_LEN;
	iov[idx].iov_base = &etype;
	iov[idx++].iov_len = sizeof(etype);

	ph->vertype = PPPOE_VERTYPE(1, 1);
	ph->code = PPPOE_CODE_PADR;
	ph->len = 0;
	ph->sessionid = 0;
	iov[idx].iov_base = ph;
	iov[idx++].iov_len = sizeof(struct pppoe_header);

	/* Host-Uniq */
	hutag.type = htons(PPPOE_TAG_HOST_UNIQ);
	hutag.len = htons(sizeof(client_cookie));
	iov[idx].iov_base = &hutag;
	iov[idx++].iov_len = sizeof(hutag.type) + sizeof(hutag.len);
	ph->len += sizeof(hutag.type)+sizeof(hutag.len)+sizeof(client_cookie);
	iov[idx].iov_base = &client_cookie;
	iov[idx++].iov_len = sizeof(client_cookie);

	/* Service-Name */
	slen = (srv == NULL) ? 0 : strlen(srv);
	svtag.type = htons(PPPOE_TAG_SERVICE_NAME);
	svtag.len = htons(slen);
	iov[idx].iov_base = &svtag;
	iov[idx++].iov_len = sizeof(hutag.type) + sizeof(hutag.len);
	ph->len += slen + sizeof(hutag.type) + sizeof(hutag.len);
	if (slen) {
		iov[idx].iov_base = srv;
		iov[idx++].iov_len = slen;
	}

	n = tag_lookup(tl, PPPOE_TAG_RELAY_SESSION, 0);
	if (n != NULL) {
		iov[idx].iov_base = &n->type;
		iov[idx++].iov_len = sizeof(n->type) + sizeof(n->len);
		if (n->len) {
			iov[idx].iov_base = &n->val;
			iov[idx++].iov_len = n->len;
		}
		ph->len += sizeof(n->type) + sizeof(n->len) + n->len;
	}

	n = tag_lookup(tl, PPPOE_TAG_AC_COOKIE, 0);
	if (n != NULL) {
		iov[idx].iov_base = &n->type;
		iov[idx++].iov_len = sizeof(n->type) + sizeof(n->len);
		if (n->len) {
			iov[idx].iov_base = n->val;
			iov[idx++].iov_len = n->len;
		}
		ph->len += sizeof(n->type) + sizeof(n->len) + n->len;
	}

	ph->len = htons(ph->len);
	tag_hton(tl);

	return (writev(bfd, iov, idx));
}

static int
getpackets(bfd, srv, sysname, myea, rmea)
	int bfd;
	char *srv, *sysname;
	struct ether_addr *myea, *rmea;
{
	static u_int8_t *pktbuf;
	u_int8_t *mpkt, *pkt, *epkt;
	struct ether_header eh;
	struct pppoe_header ph;
	struct bpf_hdr *bh;
	int rlen, r;
	u_long len;

	if (pktbuf == NULL) {
		pktbuf = (u_int8_t *)malloc(PPPOE_BPF_BUFSIZ);
		if (pktbuf == NULL)
			return (-1);
	}

	rlen = read(bfd, pktbuf, PPPOE_BPF_BUFSIZ);
	if (rlen < 0) {
		if (errno == EINTR)
			return (0);
		return (-1);
	}

	pkt = pktbuf;
	epkt = pkt + rlen;
	while (pkt < epkt) {
		bh = (struct bpf_hdr *)pkt;
		len = bh->bh_caplen;
		mpkt = pkt + bh->bh_hdrlen;

		debug_packet(mpkt, len);

		/* Pull out ethernet header */
		if (len < sizeof(struct ether_header))
			goto next;
		bcopy(mpkt, &eh, sizeof(struct ether_header));
		eh.ether_type = ntohs(eh.ether_type);
		len -= sizeof(struct ether_header);
		mpkt += sizeof(struct ether_header);

		/* Pull out pppoe header */
		if (len < sizeof(struct pppoe_header))
			goto next;
		bcopy(mpkt, &ph, sizeof(struct pppoe_header));
		len -= sizeof(struct pppoe_header);
		mpkt += sizeof(struct pppoe_header);
		ph.len = ntohs(ph.len);
		ph.sessionid = ntohs(ph.sessionid);

		if (PPPOE_VER(ph.vertype) != 1 ||
		    PPPOE_TYPE(ph.vertype) != 1)
			goto next;

		if (len > ph.len)
			len = ph.len;

		if (eh.ether_type == ETHERTYPE_PPPOEDISC) {
			/* Discovery Stage */
			switch (ph.code) {
			case PPPOE_CODE_PADO:
				r = recv_pado(bfd, srv, myea, rmea, &eh,
				    &ph, len, mpkt);
				break;
			case PPPOE_CODE_PADS:
				r = recv_pads(bfd, srv, sysname, myea, rmea,
				    &eh, &ph, len, mpkt);
				break;
			case PPPOE_CODE_PADT:
				r = recv_padt(bfd, myea, rmea, &eh, &ph,
				    len, mpkt);
				break;
			default:
				r = 0;
			}
			if (r < 0)
				return (r);
		}
		else if (eh.ether_type == ETHERTYPE_PPPOE) {
			if (client_state != STATE_EXPECT_SESSION)
				goto next;
			if (bcmp(rmea, &eh.ether_shost[0], ETHER_ADDR_LEN))
				goto next;
			if (pppfd < 0)
				goto next;
			if ((r = bpf_to_ppp(pppfd, len, mpkt)) < 0)
				return (-1);
			if (r == 0)
				continue;
		}
next:
		pkt += BPF_WORDALIGN(bh->bh_hdrlen + bh->bh_caplen);
	}
	return (1);
}


static int
recv_pado(bfd, srv, myea, rmea, eh, ph, len, pkt)
	int bfd;
	char *srv;
	struct ether_addr *myea, *rmea;
	struct ether_header *eh;
	struct pppoe_header *ph;
	u_long len;
	u_int8_t *pkt;
{
	struct tag_list tl;
	struct tag_node *n;
	int r, slen;

	if (timer_hit()) {
		timer_clr();
		return (0);
	}

	if (client_state != STATE_EXPECT_PADO)
		return (0);

	tag_init(&tl);
	if (tag_pkt(&tl, len, pkt) < 0)
		goto out;

	if (ph->sessionid != 0)
		goto out;

	if (tag_lookup(&tl, PPPOE_TAG_AC_NAME, 0) == NULL)
		goto out;

	n = tag_lookup(&tl, PPPOE_TAG_HOST_UNIQ, 0);
	if (n == NULL || n->len != sizeof(client_cookie))
		goto out;
	if (bcmp(n->val, &client_cookie, sizeof(client_cookie)))
		goto out;

	r = 0;
	slen = (srv == NULL) ? 0 : strlen(srv);
	while ((n = tag_lookup(&tl, PPPOE_TAG_SERVICE_NAME, r)) != NULL) {
		if (slen == 0)
			break;
		if (slen == 0 || n->len == 0)
			break;
		if (n->len == slen && !strncmp(srv, n->val, slen))
			break;
		r++;
	}

	if (n == NULL)
		goto out;

	bcopy(&eh->ether_shost[0], rmea, ETHER_ADDR_LEN);

	timer_clr();
	r = send_padr(bfd, srv, myea, rmea, eh, ph, &tl);
	tag_destroy(&tl);
	return (r);

out:
	tag_destroy(&tl);
	return (0);
}

static int
recv_pads(bfd, srv, sysname, myea, rmea, eh, ph, len, pkt)
	int bfd;
	char *srv, *sysname;
	struct ether_addr *myea, *rmea;
	struct ether_header *eh;
	struct pppoe_header *ph;
	u_long len;
	u_int8_t *pkt;
{
	struct tag_node *n;
	struct tag_list tl;

	if (timer_hit()) {
		timer_clr();
		return (0);
	}

	if (bcmp(rmea, &eh->ether_shost[0], ETHER_ADDR_LEN))
		return (0);

	if (client_state != STATE_EXPECT_PADS)
		return (0);

	tag_init(&tl);
	if (tag_pkt(&tl, len, pkt) < 0)
		goto out;

	n = tag_lookup(&tl, PPPOE_TAG_HOST_UNIQ, 0);
	if (n == NULL || n->len != sizeof(client_cookie))
		goto out;
	if (bcmp(n->val, &client_cookie, sizeof(client_cookie)))
		goto out;

	if (ph->sessionid == 0) {
		timer_clr();
		return (-1);
	}

	timer_clr();

	pppfd = fileno(stdin);
	if (pppfd < 0) {
		send_padt(bfd, myea, rmea, ph->sessionid);
		return (-1);
	}

	client_state = STATE_EXPECT_SESSION;
	client_sessionid = ph->sessionid;

out:
	tag_destroy(&tl);
	return (0);
}

static int
recv_padt(bfd, myea, rmea, eh, ph, len, pkt)
	int bfd;
	struct ether_addr *myea, *rmea;
	struct ether_header *eh;
	struct pppoe_header *ph;
	u_long len;
	u_int8_t *pkt;
{
	if (bcmp(&eh->ether_shost[0], rmea, ETHER_ADDR_LEN))
		return (0);

	if (client_sessionid != 0xffff && ph->sessionid == client_sessionid)
		return (-1);

	return (0);
}

static volatile int timer_alarm;
static struct sigaction timer_oact;

void
timer_handler(sig)
	int sig;
{
	timer_alarm = 1;
}

int
timer_set(sec)
	u_int sec;
{
	struct sigaction act;
	struct itimerval it;

	timer_alarm = 0;
	if (sigemptyset(&act.sa_mask) < 0)
		return (-1);
	act.sa_flags = 0;
	act.sa_handler = timer_handler;
	if (sigaction(SIGALRM, &act, &timer_oact) < 0)
		return (-1);

	timerclear(&it.it_interval);
	timerclear(&it.it_value);
	it.it_value.tv_sec = sec;
	if (setitimer(ITIMER_REAL, &it, NULL) == -1) {
		sigaction(SIGALRM, &timer_oact, NULL);
		return (-1);
	}

	return (0);
}

int
timer_clr(void)
{
	struct itimerval it;
	int r1, r2;

	timerclear(&it.it_interval);
	timerclear(&it.it_value);
	r1 = setitimer(ITIMER_REAL, &it, NULL);
	r2 = sigaction(SIGALRM, &timer_oact, NULL);
	timer_alarm = 0;

	if (r1 || r2)
		return (-1);
	return (0);
}

int
timer_hit(void)
{
	return (timer_alarm);
}
