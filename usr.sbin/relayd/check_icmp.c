/*	$OpenBSD: check_icmp.c,v 1.33 2012/09/19 09:49:24 reyk Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>

#include <limits.h>
#include <event.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <openssl/ssl.h>

#include "relayd.h"

void	icmp_setup(struct relayd *, struct ctl_icmp_event *, int);
void	check_icmp_add(struct ctl_icmp_event *, int, struct timeval *,
	    void (*)(int, short, void *));
int	icmp_checks_done(struct ctl_icmp_event *);
void	icmp_checks_timeout(struct ctl_icmp_event *, enum host_error);
void	send_icmp(int, short, void *);
void	recv_icmp(int, short, void *);
int	in_cksum(u_short *, int);

void
icmp_setup(struct relayd *env, struct ctl_icmp_event *cie, int af)
{
	int proto = IPPROTO_ICMP;

	if (af == AF_INET6)
		proto = IPPROTO_ICMPV6;
	if ((cie->s = socket(af, SOCK_RAW, proto)) < 0)
		fatal("icmp_init: socket");
	socket_set_blockmode(cie->s, BM_NONBLOCK);
	cie->env = env;
	cie->af = af;
}

void
icmp_init(struct relayd *env)
{
	icmp_setup(env, &env->sc_icmp_send, AF_INET);
	icmp_setup(env, &env->sc_icmp_recv, AF_INET);
	icmp_setup(env, &env->sc_icmp6_send, AF_INET6);
	icmp_setup(env, &env->sc_icmp6_recv, AF_INET6);
	env->sc_id = getpid() & 0xffff;
}

void
schedule_icmp(struct relayd *env, struct host *host)
{
	host->last_up = host->up;
	host->flags &= ~(F_CHECK_SENT|F_CHECK_DONE);

	if (((struct sockaddr *)&host->conf.ss)->sa_family == AF_INET)
		env->sc_has_icmp = 1;
	else
		env->sc_has_icmp6 = 1;
}

void
check_icmp_add(struct ctl_icmp_event *cie, int flags, struct timeval *start,
    void (*fn)(int, short, void *))
{
	struct timeval	 tv;

	if (start != NULL)
		bcopy(start, &cie->tv_start, sizeof(cie->tv_start));
	bcopy(&cie->env->sc_timeout, &tv, sizeof(tv));
	if (gettimeofday(&cie->tv_start, NULL) == -1)
		fatal("check_icmp_add: gettimeofday");
	event_del(&cie->ev);
	event_set(&cie->ev, cie->s, EV_TIMEOUT|flags, fn, cie);
	event_add(&cie->ev, &tv);
}

void
check_icmp(struct relayd *env, struct timeval *tv)
{
	if (env->sc_has_icmp) {
		check_icmp_add(&env->sc_icmp_recv, EV_READ, tv, recv_icmp);
		check_icmp_add(&env->sc_icmp_send, EV_WRITE, tv, send_icmp);
	}
	if (env->sc_has_icmp6) {
		check_icmp_add(&env->sc_icmp6_recv, EV_READ, tv, recv_icmp);
		check_icmp_add(&env->sc_icmp6_send, EV_WRITE, tv, send_icmp);
	}
}

int
icmp_checks_done(struct ctl_icmp_event *cie)
{
	struct table	*table;
	struct host	*host;

	TAILQ_FOREACH(table, cie->env->sc_tables, entry) {
		if (table->conf.flags & F_DISABLE ||
		    table->conf.check != CHECK_ICMP)
			continue;
		TAILQ_FOREACH(host, &table->hosts, entry) {
			if (((struct sockaddr *)&host->conf.ss)->sa_family !=
			    cie->af)
				continue;
			if (!(host->flags & F_CHECK_DONE))
				return (0);
		}
	}
	return (1);
}

void
icmp_checks_timeout(struct ctl_icmp_event *cie, enum host_error he)
{
	struct table	*table;
	struct host	*host;

	TAILQ_FOREACH(table, cie->env->sc_tables, entry) {
		if (table->conf.flags & F_DISABLE ||
		    table->conf.check != CHECK_ICMP)
			continue;
		TAILQ_FOREACH(host, &table->hosts, entry) {
			if (((struct sockaddr *)&host->conf.ss)->sa_family !=
			    cie->af)
				continue;
			if (!(host->flags & (F_CHECK_DONE|F_DISABLE))) {
				host->up = HOST_DOWN;
				hce_notify_done(host, he);
			}
		}
	}
}

void
send_icmp(int s, short event, void *arg)
{
	struct ctl_icmp_event	*cie = (struct ctl_icmp_event *)arg;
	struct table		*table;
	struct host		*host;
	struct sockaddr		*to;
	struct icmp		*icp;
	struct icmp6_hdr	*icp6;
	ssize_t			 r;
	u_char			 packet[ICMP_BUF_SIZE];
	socklen_t		 slen;
	int			 i = 0, ttl, mib[4];
	size_t			 len;
	u_int32_t		 id;

	if (event == EV_TIMEOUT) {
		icmp_checks_timeout(cie, HCE_ICMP_WRITE_TIMEOUT);
		return;
	}

	bzero(&packet, sizeof(packet));
	icp = (struct icmp *)packet;
	icp6 = (struct icmp6_hdr *)packet;
	if (cie->af == AF_INET) {
		icp->icmp_type = ICMP_ECHO;
		icp->icmp_code = 0;
		icp->icmp_id = htons(cie->env->sc_id);
		icp->icmp_cksum = 0;
		slen = sizeof(struct sockaddr_in);
	} else {
		icp6->icmp6_type = ICMP6_ECHO_REQUEST;
		icp6->icmp6_code = 0;
		icp6->icmp6_cksum = 0;
		icp6->icmp6_id = htons(cie->env->sc_id);
		slen = sizeof(struct sockaddr_in6);
	}

	TAILQ_FOREACH(table, cie->env->sc_tables, entry) {
		if (table->conf.check != CHECK_ICMP ||
		    table->conf.flags & F_DISABLE)
			continue;
		TAILQ_FOREACH(host, &table->hosts, entry) {
			if (host->flags & (F_DISABLE | F_CHECK_SENT) ||
			    host->conf.parentid)
				continue;
			if (((struct sockaddr *)&host->conf.ss)->sa_family !=
			    cie->af)
				continue;
			i++;
			to = (struct sockaddr *)&host->conf.ss;
			id = htonl(host->conf.id);

			if (cie->af == AF_INET) {
				icp->icmp_seq = htons(i);
				icp->icmp_cksum = 0;
				icp->icmp_mask = id;
				icp->icmp_cksum = in_cksum((u_short *)icp,
				    sizeof(packet));
			} else {
				icp6->icmp6_seq = htons(i);
				icp6->icmp6_cksum = 0;
				memcpy(packet + sizeof(*icp6), &id, sizeof(id));
				icp6->icmp6_cksum = in_cksum((u_short *)icp6,
				    sizeof(packet));
			}

			if ((ttl = host->conf.ttl) > 0)
				(void)setsockopt(s, IPPROTO_IP, IP_TTL,
				    &host->conf.ttl, sizeof(int));
			else {
				/* Revert to default TTL */
				mib[0] = CTL_NET;
				mib[1] = cie->af;
				mib[2] = IPPROTO_IP;
				mib[3] = IPCTL_DEFTTL;
				len = sizeof(ttl);
				if (sysctl(mib, 4, &ttl, &len, NULL, 0) == 0)
					(void)setsockopt(s, IPPROTO_IP, IP_TTL,
					    &ttl, sizeof(int));
			}

			r = sendto(s, packet, sizeof(packet), 0, to, slen);
			if (r == -1) {
				if (errno == EAGAIN || errno == EINTR)
					goto retry;
				host->flags |= F_CHECK_SENT|F_CHECK_DONE;
				host->up = HOST_DOWN;
			} else if (r != sizeof(packet))
				goto retry;
			host->flags |= F_CHECK_SENT;
		}
	}

	return;

 retry:
	event_again(&cie->ev, s, EV_TIMEOUT|EV_WRITE, send_icmp,
	    &cie->tv_start, &cie->env->sc_timeout, cie);
}

void
recv_icmp(int s, short event, void *arg)
{
	struct ctl_icmp_event	*cie = (struct ctl_icmp_event *)arg;
	u_char			 packet[ICMP_BUF_SIZE];
	socklen_t		 slen;
	struct sockaddr_storage	 ss;
	struct icmp		*icp;
	struct icmp6_hdr	*icp6;
	u_int16_t		 icpid;
	struct host		*host;
	ssize_t			 r;
	u_int32_t		 id;

	if (event == EV_TIMEOUT) {
		icmp_checks_timeout(cie, HCE_ICMP_READ_TIMEOUT);
		return;
	}

	bzero(&packet, sizeof(packet));
	bzero(&ss, sizeof(ss));
	slen = sizeof(ss);

	r = recvfrom(s, packet, sizeof(packet), 0,
	    (struct sockaddr *)&ss, &slen);
	if (r == -1 || r != ICMP_BUF_SIZE) {
		if (r == -1 && errno != EAGAIN && errno != EINTR)
			log_debug("%s: receive error", __func__);
		goto retry;
	}

	if (cie->af == AF_INET) {
		icp = (struct icmp *)(packet + sizeof(struct ip));
		icpid = ntohs(icp->icmp_id);
		id = icp->icmp_mask;
	} else {
		icp6 = (struct icmp6_hdr *)packet;
		icpid = ntohs(icp6->icmp6_id);
		memcpy(&id, packet + sizeof(*icp6), sizeof(id));
	}
	if (icpid != cie->env->sc_id)
		goto retry;
	id = ntohl(id);
	host = host_find(cie->env, id);
	if (host == NULL) {
		log_warn("%s: ping for unknown host received", __func__);
		goto retry;
	}
	if (bcmp(&ss, &host->conf.ss, slen)) {
		log_warnx("%s: forged icmp packet?", __func__);
		goto retry;
	}

	host->up = HOST_UP;
	host->flags |= F_CHECK_DONE;
	hce_notify_done(host, HCE_ICMP_OK);

	if (icmp_checks_done(cie))
		return;

 retry:
	event_again(&cie->ev, s, EV_TIMEOUT|EV_READ, recv_icmp,
	    &cie->tv_start, &cie->env->sc_timeout, cie);
}

/* in_cksum from ping.c --
 *	Checksum routine for Internet Protocol family headers (C Version)
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
int
in_cksum(u_short *addr, int len)
{
	int nleft = len;
	u_short *w = addr;
	int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */

	return (answer);
}
