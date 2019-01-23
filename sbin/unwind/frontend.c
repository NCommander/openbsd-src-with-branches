/*	$OpenBSD: frontend.c,v 1.19 2018/11/28 06:41:31 florian Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asr_private.h"
#include "uw_log.h"
#include "unwind.h"
#include "frontend.h"
#include "control.h"

#define	ROUTE_SOCKET_BUF_SIZE   16384

struct udp_ev {
	struct event		 ev;
	uint8_t			 query[65536];
	struct msghdr		 rcvmhdr;
	struct iovec		 rcviov[1];
	struct sockaddr_storage	 from;
} udp4ev, udp6ev;

struct pending_query {
	TAILQ_ENTRY(pending_query)	 entry;
	struct sockaddr_storage		 from;
	uint8_t				*query;
	ssize_t				 len;
	uint64_t			 imsg_id;
	int				 fd;
	int				 bogus;
};

TAILQ_HEAD(, pending_query)	 pending_queries;

__dead void		 frontend_shutdown(void);
void			 frontend_sig_handler(int, short, void *);
void			 frontend_startup(void);
void			 udp_receive(int, short, void *);
void			 send_answer(struct pending_query *, uint8_t *,
			     ssize_t);
void			 route_receive(int, short, void *);
void			 handle_route_message(struct rt_msghdr *,
			     struct sockaddr **);
void			 get_rtaddrs(int, struct sockaddr *,
			     struct sockaddr **);
void			 rtmget_default(void);
char			*ip_port(struct sockaddr *);
struct pending_query	*find_pending_query(uint64_t);
void			 parse_dhcp_lease(int);

struct unwind_conf	*frontend_conf;
struct imsgev		*iev_main;
struct imsgev		*iev_resolver;
struct event		 ev_route;
int			 udp4sock = -1, udp6sock = -1, routesock = -1;

void
frontend_sig_handler(int sig, short event, void *bula)
{
	/*
	 * Normal signal handler rules don't apply because libevent
	 * decouples for us.
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		frontend_shutdown();
	default:
		fatalx("unexpected signal");
	}
}

void
frontend(int debug, int verbose)
{
	struct event		 ev_sigint, ev_sigterm;
	struct passwd		*pw;
	size_t			 rcvcmsglen, sndcmsgbuflen;
	uint8_t			*rcvcmsgbuf;
	uint8_t			*sndcmsgbuf = NULL;

	frontend_conf = config_new_empty();
	control_state.fd = -1;

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if ((pw = getpwnam(UNWIND_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	unwind_process = PROC_FRONTEND;
	setproctitle("%s", log_procnames[unwind_process]);
	log_procinit(log_procnames[unwind_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (pledge("stdio unix recvfd", NULL) == -1)
		fatal("pledge");

	event_init();

	/* Setup signal handler. */
	signal_set(&ev_sigint, SIGINT, frontend_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, frontend_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Setup pipe and event handler to the parent process. */
	if ((iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_main->ibuf, 3);
	iev_main->handler = frontend_dispatch_main;
	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	if((rcvcmsgbuf = malloc(rcvcmsglen)) == NULL)
		fatal("malloc");

	udp4ev.rcviov[0].iov_base = (caddr_t)udp4ev.query;
	udp4ev.rcviov[0].iov_len = sizeof(udp4ev.query);
	udp4ev.rcvmhdr.msg_name = (caddr_t)&udp4ev.from;
	udp4ev.rcvmhdr.msg_namelen = sizeof(udp4ev.from);
	udp4ev.rcvmhdr.msg_iov = udp4ev.rcviov;
	udp4ev.rcvmhdr.msg_iovlen = 1;

	udp6ev.rcviov[0].iov_base = (caddr_t)udp6ev.query;
	udp6ev.rcviov[0].iov_len = sizeof(udp6ev.query);
	udp6ev.rcvmhdr.msg_name = (caddr_t)&udp6ev.from;
	udp6ev.rcvmhdr.msg_namelen = sizeof(udp6ev.from);
	udp6ev.rcvmhdr.msg_iov = udp6ev.rcviov;
	udp6ev.rcvmhdr.msg_iovlen = 1;

	sndcmsgbuflen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	if ((sndcmsgbuf = malloc(sndcmsgbuflen)) == NULL)
		fatal("%s", __func__);

	TAILQ_INIT(&pending_queries);

	event_dispatch();

	frontend_shutdown();
}

__dead void
frontend_shutdown(void)
{
	/* Close pipes. */
	msgbuf_write(&iev_resolver->ibuf.w);
	msgbuf_clear(&iev_resolver->ibuf.w);
	close(iev_resolver->ibuf.fd);
	msgbuf_write(&iev_main->ibuf.w);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	config_clear(frontend_conf);

	free(iev_resolver);
	free(iev_main);

	log_info("frontend exiting");
	exit(0);
}

int
frontend_imsg_compose_main(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data,
	    datalen));
}

int
frontend_imsg_compose_resolver(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_resolver, type, 0, pid, -1, data,
	    datalen));
}

void
frontend_dispatch_main(int fd, short event, void *bula)
{
	static struct unwind_conf		*nconf;
	struct unwind_forwarder		*unwind_forwarder;
	struct imsg			 imsg;
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf = &iev->ibuf;
	int				 n, shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_SOCKET_IPC:
			/*
			 * Setup pipe and event handler to the resolver
			 * process.
			 */
			if (iev_resolver) {
				log_warnx("%s: received unexpected imsg fd "
				    "to frontend", __func__);
				break;
			}
			if ((fd = imsg.fd) == -1) {
				log_warnx("%s: expected to receive imsg fd to "
				   "frontend but didn't receive any",
				   __func__);
				break;
			}

			iev_resolver = malloc(sizeof(struct imsgev));
			if (iev_resolver == NULL)
				fatal(NULL);

			imsg_init(&iev_resolver->ibuf, fd);
			iev_resolver->handler = frontend_dispatch_resolver;
			iev_resolver->events = EV_READ;

			event_set(&iev_resolver->ev, iev_resolver->ibuf.fd,
			iev_resolver->events, iev_resolver->handler, iev_resolver);
			event_add(&iev_resolver->ev, NULL);
			break;
		case IMSG_RECONF_CONF:
			if ((nconf = malloc(sizeof(struct unwind_conf))) ==
			    NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct unwind_conf));
			SIMPLEQ_INIT(&nconf->unwind_forwarder_list);
			break;
		case IMSG_RECONF_FORWARDER:
			if ((unwind_forwarder = malloc(sizeof(struct
			    unwind_forwarder))) == NULL)
				fatal(NULL);
			memcpy(unwind_forwarder, imsg.data, sizeof(struct
			    unwind_forwarder));
			SIMPLEQ_INSERT_TAIL(&nconf->unwind_forwarder_list,
			    unwind_forwarder, entry);
			break;
		case IMSG_RECONF_END:
			merge_config(frontend_conf, nconf);
			nconf = NULL;
			break;
		case IMSG_UDP6SOCK:
			if ((udp6sock = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "UDP6 fd but didn't receive any",
				    __func__);
			event_set(&udp6ev.ev, udp6sock, EV_READ | EV_PERSIST,
			    udp_receive, &udp6ev);
			break;
		case IMSG_UDP4SOCK:
			if ((udp4sock = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "UDP4 fd but didn't receive any",
				    __func__);
			event_set(&udp4ev.ev, udp4sock, EV_READ | EV_PERSIST,
			    udp_receive, &udp4ev);
			break;
		case IMSG_ROUTESOCK:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "routesocket fd but didn't receive any",
				    __func__);
			routesock = fd;
			event_set(&ev_route, fd, EV_READ | EV_PERSIST,
			    route_receive, NULL);
			break;
		case IMSG_STARTUP:
			frontend_startup();
			break;
		case IMSG_CONTROLFD:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "control fd but didn't receive any",
				    __func__);
			control_state.fd = fd;
			/* Listen on control socket. */
			TAILQ_INIT(&ctl_conns);
			control_listen();
			break;
		case IMSG_LEASEFD:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "dhcp lease fd but didn't receive any",
				    __func__);
			parse_dhcp_lease(fd);
			break;
		case IMSG_SHUTDOWN:
			frontend_imsg_compose_resolver(IMSG_SHUTDOWN, 0, NULL, 0);
			break;
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
frontend_dispatch_resolver(int fd, short event, void *bula)
{
	static struct pending_query	*pq;
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf = &iev->ibuf;
	struct imsg			 imsg;
	struct query_imsg		*query_imsg;
	int				 n, shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_SHUTDOWN:
			frontend_imsg_compose_main(IMSG_SHUTDOWN, 0, NULL, 0);
			break;
		case IMSG_ANSWER_HEADER:
			/* XX size */
			query_imsg = (struct query_imsg *)imsg.data;
			if ((pq = find_pending_query(query_imsg->id)) ==
			    NULL) {
				log_warnx("cannot find pending query %llu",
				    query_imsg->id);
				break;
			}
			if (query_imsg->err) {
				send_answer(pq, NULL, 0);
				pq = NULL;
				break;
			}
			pq->bogus = query_imsg->bogus;
			break;
		case IMSG_ANSWER:
			if (pq == NULL)
				fatalx("IMSG_ANSWER without HEADER");
			send_answer(pq, imsg.data, imsg.hdr.len -
			    IMSG_HEADER_SIZE);
			break;
		case IMSG_CTL_RESOLVER_INFO:
		case IMSG_CTL_RESOLVER_WHY_BOGUS:
		case IMSG_CTL_RESOLVER_HISTOGRAM:
		case IMSG_CTL_END:
			control_imsg_relay(&imsg);
			break;
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
frontend_startup(void)
{
	if (!event_initialized(&udp4ev.ev) || !event_initialized(&udp6ev.ev))
		fatalx("%s: did not receive a UDP4 or UDP6 socket fd from the "
		    "main process", __func__);

	if (event_initialized(&udp4ev.ev))
		event_add(&udp4ev.ev, NULL);

	if (event_initialized(&udp6ev.ev))
		event_add(&udp6ev.ev, NULL);

	if (!event_initialized(&ev_route))
		fatalx("%s: did not receive a route socket from the main "
		    "process", __func__);

	event_add(&ev_route, NULL);

	frontend_imsg_compose_main(IMSG_STARTUP_DONE, 0, NULL, 0);
	rtmget_default();
}

void
udp_receive(int fd, short events, void *arg)
{
	struct udp_ev		*udpev = (struct udp_ev *)arg;
	struct pending_query	*pq;
	struct query_imsg	*query_imsg;
	struct asr_unpack	 p;
	struct asr_dns_header	 h;
	struct asr_dns_query	 q;
	ssize_t			 len;
	char			*str_from, buf[1024];


	if ((len = recvmsg(fd, &udpev->rcvmhdr, 0)) < 0) {
		log_warn("recvmsg");
		return;
	}

	str_from = ip_port((struct sockaddr *)&udpev->from);

	_asr_unpack_init(&p, udpev->query, len);

	if (_asr_unpack_header(&p, &h) == -1) {
		log_warnx("bad query: %s, from: %s", strerror(p.err), str_from);
		return;
	}

	if (h.qdcount != 1 && h.ancount != 0 && h.nscount != 0 &&
	    h.arcount != 0) {
		log_warnx("invalid query from %s, qdcount: %d, ancount: %d "
		    "nscount: %d, arcount: %d", str_from, h.qdcount, h.ancount,
		    h.nscount, h.arcount);
		return;
	}

	log_debug("query from %s", str_from);
	log_debug(";; HEADER %s", print_header(&h, buf, sizeof(buf)));
	log_debug(";; QUERY SECTION:");
	if (_asr_unpack_query(&p, &q) == -1)
		goto error;
	log_debug("%s", print_query(&q, buf, sizeof(buf)));

	if ((pq = malloc(sizeof(*pq))) == NULL) {
		log_warn(NULL);
		return;
	}

	if ((pq->query = malloc(len)) == NULL) {
		log_warn(NULL);
		free(pq);
		return;
	}

	do {
		arc4random_buf(&pq->imsg_id, sizeof(pq->imsg_id));
	} while(find_pending_query(pq->imsg_id) != NULL);

	memcpy(pq->query, udpev->query, len);
	pq->len = len;
	pq->from = udpev->from;
	pq->fd = fd;

	if ((query_imsg = calloc(1, sizeof(*query_imsg))) == NULL) {
		log_warn(NULL);
		return;
	}

	 /* XXX */
	print_dname(q.q_dname, query_imsg->qname, sizeof(query_imsg->qname));
	query_imsg->id = pq->imsg_id;
	query_imsg->t = q.q_type;
	query_imsg->c = q.q_class;

	if (frontend_imsg_compose_resolver(IMSG_QUERY, 0, query_imsg,
	    sizeof(*query_imsg)) != -1) {
		TAILQ_INSERT_TAIL(&pending_queries, pq, entry);
	} else {
		free(query_imsg);
		/* XXX SERVFAIL */
		free(pq->query);
		free(pq);
	}

error:
	if (p.err)
		log_debug(";; ERROR AT OFFSET %zu/%zu: %s", p.offset, p.len,
		    strerror(p.err));
}

void
send_answer(struct pending_query *pq, uint8_t *answer, ssize_t len)
{
	struct asr_pack		 p;
	struct asr_unpack	 query_u, answer_u;
	struct asr_dns_header	 query_h, answer_h;

	log_debug("result for %s",
	    ip_port((struct sockaddr*)&pq->from));

	_asr_unpack_init(&query_u, pq->query, pq->len);
	_asr_unpack_header(&query_u, &query_h); /* XXX */

	if (answer == NULL) {
		answer = pq->query;
		len = pq->len;
		_asr_unpack_init(&answer_u, answer, len);
		_asr_unpack_header(&answer_u, &answer_h); /* XXX */

		answer_h.flags = 0;
		answer_h.flags |= RA_MASK;
		answer_h.flags = (answer_h.flags & ~RCODE_MASK) | SERVFAIL;
	} else {
		_asr_unpack_init(&answer_u, answer, len);
		_asr_unpack_header(&answer_u, &answer_h); /* XXX */

		if (pq->bogus) {
			if(query_h.flags & CD_MASK) {
				answer_h.id = query_h.id;
				answer_h.flags |= CD_MASK;
			} else {
				answer = pq->query;
				len = pq->len;

				_asr_unpack_init(&answer_u, answer, len);
				_asr_unpack_header(&answer_u, &answer_h); /* XXX */

				answer_h.flags = 0;
				answer_h.flags |= RA_MASK;
				answer_h.flags = (answer_h.flags & ~RCODE_MASK)
				    | SERVFAIL;
			}
		} else {
			answer_h.id = query_h.id;
		}
	}

	_asr_pack_init(&p, answer, len);
	_asr_pack_header(&p, &answer_h);

	if(sendto(pq->fd, answer, len, 0, (struct sockaddr *)
	   &pq->from, pq->from.ss_len) == -1)
		log_warn("sendto");

	TAILQ_REMOVE(&pending_queries, pq, entry);
	free(pq->query);
	free(pq);
}

char*
ip_port(struct sockaddr *sa)
{
	static char	 hbuf[NI_MAXHOST], buf[NI_MAXHOST];

	if (getnameinfo(sa, sa->sa_len, hbuf, sizeof(hbuf), NULL, 0,
	    NI_NUMERICHOST) != 0) {
		snprintf(buf, sizeof(buf), "%s", "(unknown)");
		return buf;
	}

	if (sa->sa_family == AF_INET6)
		snprintf(buf, sizeof(buf), "[%s]:%d", hbuf,
		    ((struct sockaddr_in6 *)sa)->sin6_port);
	if (sa->sa_family == AF_INET)
		snprintf(buf, sizeof(buf), "[%s]:%d", hbuf,
		    ((struct sockaddr_in *)sa)->sin_port);

	return buf;
}

struct pending_query*
find_pending_query(uint64_t id)
{
	struct pending_query	*pq;

	TAILQ_FOREACH(pq, &pending_queries, entry)
		if (pq->imsg_id == id)
			return pq;
	return NULL;
}

void
route_receive(int fd, short events, void *arg)
{
	static uint8_t			 *buf;

	struct rt_msghdr		*rtm;
	struct sockaddr			*sa, *rti_info[RTAX_MAX];
	ssize_t				 n;

	if (buf == NULL) {
		buf = malloc(ROUTE_SOCKET_BUF_SIZE);
		if (buf == NULL)
			fatal("malloc");
	}
	rtm = (struct rt_msghdr *)buf;
	if ((n = read(fd, buf, ROUTE_SOCKET_BUF_SIZE)) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		log_warn("dispatch_rtmsg: read error");
		return;
	}

	if (n == 0)
		fatal("routing socket closed");

	if (n < (ssize_t)sizeof(rtm->rtm_msglen) || n < rtm->rtm_msglen) {
		log_warnx("partial rtm of %zd in buffer", n);
		return;
	}

	if (rtm->rtm_version != RTM_VERSION)
		return;

	sa = (struct sockaddr *)(buf + rtm->rtm_hdrlen);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	handle_route_message(rtm, rti_info);
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int	i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		} else
			rti_info[i] = NULL;
	}
}

void
handle_route_message(struct rt_msghdr *rtm, struct sockaddr **rti_info)
{
	char	buf[IF_NAMESIZE], *bufp;

	switch (rtm->rtm_type) {
	case RTM_GET:
		if (rtm->rtm_errno != 0)
			break;
		if (!(rtm->rtm_flags & RTF_UP))
			break;
		if (!(rtm->rtm_addrs & RTA_DST))
			break;
		if (rti_info[RTAX_DST]->sa_family != AF_INET)
			break;
		if (((struct sockaddr_in *)rti_info[RTAX_DST])->sin_addr.
		    s_addr != INADDR_ANY)
			break;
		if (!(rtm->rtm_addrs & RTA_NETMASK))
			break;
		if (rti_info[RTAX_NETMASK]->sa_family != AF_INET)
			break;
		if (((struct sockaddr_in *)rti_info[RTAX_NETMASK])->sin_addr.
		    s_addr != INADDR_ANY)
			break;

		frontend_imsg_compose_main(IMSG_OPEN_DHCP_LEASE, 0,
		    &rtm->rtm_index, sizeof(rtm->rtm_index));

		bufp = if_indextoname(rtm->rtm_index, buf);
		if (bufp)
			log_debug("default route is on %s", buf);

		break;
	default:
		break;
	}

}

void
rtmget_default(void)
{
	static int		 rtm_seq;
	struct rt_msghdr	 rtm;
	struct sockaddr_in	 sin;
	struct iovec		 iov[5];
	long			 pad = 0;
	int			 iovcnt = 0, padlen;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_GET;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_tableid = 0; /* XXX imsg->rdomain; */
	rtm.rtm_seq = ++rtm_seq;
	rtm.rtm_addrs = RTA_DST | RTA_NETMASK;

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	/* dst */
	iov[iovcnt].iov_base = &sin;
	iov[iovcnt++].iov_len = sizeof(sin);
	rtm.rtm_msglen += sizeof(sin);
	padlen = ROUNDUP(sizeof(sin)) - sizeof(sin);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	/* mask */
	iov[iovcnt].iov_base = &sin;
	iov[iovcnt++].iov_len = sizeof(sin);
	rtm.rtm_msglen += sizeof(sin);
	padlen = ROUNDUP(sizeof(sin)) - sizeof(sin);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	if (writev(routesock, iov, iovcnt) == -1)
		log_warn("failed to send route message");
}

void
parse_dhcp_lease(int fd)
{
	FILE	*f;
	char	*line = NULL, *cur_ns = NULL, *ns = NULL;
	size_t	 linesize = 0;
	ssize_t	 linelen;
	time_t	 epoch, lease_time, now;
	char **tok, *toks[4], *p;

	if((f = fdopen(fd, "r")) == NULL) {
		log_warn("cannot read dhcp lease");
		close(fd);
		return;
	}

	now = time(NULL);

	while ((linelen = getline(&line, &linesize, f)) != -1) {
		for (tok = toks; tok < &toks[3] && (*tok = strsep(&line, " \t"))
		    != NULL;) {
			if (**tok != '\0')
				tok++;
		}
		*tok = NULL;
		if (strcmp(toks[0], "option") == 0) {
			if (strcmp(toks[1], "domain-name-servers") == 0) {
				if((p = strchr(toks[2], ';')) != NULL) {
					*p='\0';
					cur_ns = strdup(toks[2]);
				}
			}
			if (strcmp(toks[1], "dhcp-lease-time") == 0) {
				if((p = strchr(toks[2], ';')) != NULL) {
					*p='\0';
					lease_time = strtonum(toks[2], 0,
					    INT64_MAX, NULL);
				}
			}
		} else if (strcmp(toks[0], "epoch") == 0) {
			if((p = strchr(toks[1], ';')) != NULL) {
				*p='\0';
				epoch = strtonum(toks[1], 0,
				    INT64_MAX, NULL);
			}
		}
		else if (*toks[0] == '}') {
			if (epoch + lease_time > now ) {
				free(ns);
				ns = cur_ns;
				//log_debug("ns: %s, lease_time: %lld, epoch: "
				//    "%lld\n", cur_ns, lease_time, epoch);
			} else /* expired lease */
				free(cur_ns);
		}
	}
	free(line);

	if (ferror(f))
		log_warn("getline");
	fclose(f);

	if (ns != NULL) {
		log_debug("%s: ns: %s", __func__, ns);
		frontend_imsg_compose_resolver(IMSG_FORWARDER, 0, ns,
		    strlen(ns) + 1);
	}
}
