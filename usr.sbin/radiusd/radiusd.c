/*	$OpenBSD: radiusd.c,v 1.34 2024/01/08 04:16:48 yasuoka Exp $	*/

/*
 * Copyright (c) 2013, 2023 Internet Initiative Japan Inc.
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <imsg.h>
#include <md5.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <radius.h>

#include "radiusd.h"
#include "radiusd_local.h"
#include "log.h"
#include "util.h"
#include "imsg_subr.h"

static int		 radiusd_start(struct radiusd *);
static void		 radiusd_stop(struct radiusd *);
static void		 radiusd_free(struct radiusd *);
static void		 radiusd_listen_on_event(int, short, void *);
static void		 radiusd_listen_handle_packet(struct radiusd_listen *,
			    RADIUS_PACKET *, struct sockaddr *, socklen_t);
static void		 radiusd_on_sigterm(int, short, void *);
static void		 radiusd_on_sigint(int, short, void *);
static void		 radiusd_on_sighup(int, short, void *);
static void		 radiusd_on_sigchld(int, short, void *);
static void		 raidus_query_access_request(struct radius_query *);
static void		 radius_query_access_response(struct radius_query *);
static const char	*radius_code_string(int);
static int		 radiusd_access_response_fixup (struct radius_query *);



static void		 radiusd_module_reset_ev_handler(
			    struct radiusd_module *);
static int		 radiusd_module_imsg_read(struct radiusd_module *,
			    bool);
static void		 radiusd_module_imsg(struct radiusd_module *,
			    struct imsg *);

static struct radiusd_module_radpkt_arg *
			 radiusd_module_recv_radpkt(struct radiusd_module *,
			    struct imsg *, uint32_t, const char *);
static void		 radiusd_module_on_imsg_io(int, short, void *);
void			 radiusd_module_start(struct radiusd_module *);
void			 radiusd_module_stop(struct radiusd_module *);
static void		 radiusd_module_close(struct radiusd_module *);
static void		 radiusd_module_userpass(struct radiusd_module *,
			    struct radius_query *);
static void		 radiusd_module_access_request(struct radiusd_module *,
			    struct radius_query *);
static void		 radiusd_module_request_decoration(
			    struct radiusd_module *, struct radius_query *);
static void		 radiusd_module_response_decoration(
			    struct radiusd_module *, struct radius_query *);
static int		 imsg_compose_radius_packet(struct imsgbuf *,
			    uint32_t, u_int, RADIUS_PACKET *);

static u_int		 radius_query_id_seq = 0;
int			 debug = 0;

static __dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dn] [-f file]\n", __progname);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	extern char		*__progname;
	const char		*conffile = CONFFILE;
	int			 ch;
	struct radiusd		*radiusd;
	bool			 noaction = false;
	struct passwd		*pw;

	while ((ch = getopt(argc, argv, "df:n")) != -1)
		switch (ch) {
		case 'd':
			debug++;
			break;

		case 'f':
			conffile = optarg;
			break;

		case 'n':
			noaction = true;
			break;

		default:
			usage();
			/* NOTREACHED */
		}

	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	if ((radiusd = calloc(1, sizeof(*radiusd))) == NULL)
		err(1, "calloc");
	TAILQ_INIT(&radiusd->listen);
	TAILQ_INIT(&radiusd->query);

	log_init(debug);
	if (parse_config(conffile, radiusd) != 0)
		errx(EXIT_FAILURE, "config error");
	if (noaction) {
		fprintf(stderr, "configuration OK\n");
		exit(EXIT_SUCCESS);
	}

	if (debug == 0)
		daemon(0, 0);
	event_init();

	if ((pw = getpwnam(RADIUSD_USER)) == NULL)
		errx(EXIT_FAILURE, "user `%s' is not found in password "
		    "database", RADIUSD_USER);

	if (chroot(pw->pw_dir) == -1)
		err(EXIT_FAILURE, "chroot");
	if (chdir("/") == -1)
		err(EXIT_FAILURE, "chdir(\"/\")");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		err(EXIT_FAILURE, "cannot drop privileges");

	signal(SIGPIPE, SIG_IGN);
	openlog(NULL, LOG_PID, LOG_DAEMON);

	signal_set(&radiusd->ev_sigterm, SIGTERM, radiusd_on_sigterm, radiusd);
	signal_set(&radiusd->ev_sigint,  SIGINT,  radiusd_on_sigint,  radiusd);
	signal_set(&radiusd->ev_sighup,  SIGHUP,  radiusd_on_sighup,  radiusd);
	signal_set(&radiusd->ev_sigchld, SIGCHLD, radiusd_on_sigchld, radiusd);

	if (radiusd_start(radiusd) != 0)
		errx(EXIT_FAILURE, "start failed");

	if (pledge("stdio inet", NULL) == -1)
		err(EXIT_FAILURE, "pledge");

	if (event_loop(0) < 0)
		radiusd_stop(radiusd);

	radiusd_free(radiusd);
	event_base_free(NULL);

	exit(EXIT_SUCCESS);
}

static int
radiusd_start(struct radiusd *radiusd)
{
	struct radiusd_listen	*l;
	struct radiusd_module	*module;
	int			 s;
	char			 hbuf[NI_MAXHOST];

	TAILQ_FOREACH(l, &radiusd->listen, next) {
		if (getnameinfo(
		    (struct sockaddr *)&l->addr, l->addr.ipv4.sin_len,
		    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0) {
			log_warn("%s: getnameinfo()", __func__);
			goto on_error;
		}
		if ((s = socket(l->addr.ipv4.sin_family,
		    l->stype | SOCK_NONBLOCK, l->sproto)) == -1) {
			log_warn("Listen %s port %d is failed: socket()",
			    hbuf, (int)htons(l->addr.ipv4.sin_port));
			goto on_error;
		}
		if (bind(s, (struct sockaddr *)&l->addr, l->addr.ipv4.sin_len)
		    != 0) {
			log_warn("Listen %s port %d is failed: bind()",
			    hbuf, (int)htons(l->addr.ipv4.sin_port));
			close(s);
			goto on_error;
		}
		if (l->addr.ipv4.sin_family == AF_INET)
			log_info("Start listening on %s:%d/udp", hbuf,
			    (int)ntohs(l->addr.ipv4.sin_port));
		else
			log_info("Start listening on [%s]:%d/udp", hbuf,
			    (int)ntohs(l->addr.ipv4.sin_port));
		event_set(&l->ev, s, EV_READ | EV_PERSIST,
		    radiusd_listen_on_event, l);
		if (event_add(&l->ev, NULL) != 0) {
			log_warn("event_add() failed at %s()", __func__);
			close(s);
			goto on_error;
		}
		l->sock = s;
		l->radiusd = radiusd;
	}

	signal_add(&radiusd->ev_sigterm, NULL);
	signal_add(&radiusd->ev_sigint, NULL);
	signal_add(&radiusd->ev_sighup, NULL);
	signal_add(&radiusd->ev_sigchld, NULL);

	TAILQ_FOREACH(module, &radiusd->module, next) {
		if (debug > 0)
			radiusd_module_set(module, "_debug", 0, NULL);
		radiusd_module_start(module);
	}

	return (0);
on_error:
	radiusd_stop(radiusd);

	return (-1);
}

static void
radiusd_stop(struct radiusd *radiusd)
{
	char			 hbuf[NI_MAXHOST];
	struct radiusd_listen	*l;
	struct radiusd_module	*module;

	TAILQ_FOREACH_REVERSE(l, &radiusd->listen, radiusd_listen_head, next) {
		if (l->sock >= 0) {
			if (getnameinfo(
			    (struct sockaddr *)&l->addr, l->addr.ipv4.sin_len,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
				strlcpy(hbuf, "error", sizeof(hbuf));
			if (l->addr.ipv4.sin_family == AF_INET)
				log_info("Stop listening on %s:%d/udp", hbuf,
				    (int)ntohs(l->addr.ipv4.sin_port));
			else
				log_info("Stop listening on [%s]:%d/udp", hbuf,
				    (int)ntohs(l->addr.ipv4.sin_port));
			event_del(&l->ev);
			close(l->sock);
		}
		l->sock = -1;
	}
	TAILQ_FOREACH(module, &radiusd->module, next) {
		radiusd_module_stop(module);
		radiusd_module_close(module);
	}
	if (signal_pending(&radiusd->ev_sigterm, NULL))
		signal_del(&radiusd->ev_sigterm);
	if (signal_pending(&radiusd->ev_sigint, NULL))
		signal_del(&radiusd->ev_sigint);
	if (signal_pending(&radiusd->ev_sighup, NULL))
		signal_del(&radiusd->ev_sighup);
	if (signal_pending(&radiusd->ev_sigchld, NULL))
		signal_del(&radiusd->ev_sigchld);
}

static void
radiusd_free(struct radiusd *radiusd)
{
	int				 i;
	struct radiusd_listen		*listn, *listnt;
	struct radiusd_client		*client, *clientt;
	struct radiusd_module		*module, *modulet;
	struct radiusd_module_ref	*modref, *modreft;
	struct radiusd_authentication	*authen, *authent;

	TAILQ_FOREACH_SAFE(authen, &radiusd->authen, next, authent) {
		TAILQ_REMOVE(&radiusd->authen, authen, next);
		free(authen->auth);
		TAILQ_FOREACH_SAFE(modref, &authen->deco, next, modreft) {
			TAILQ_REMOVE(&authen->deco, modref, next);
			free(modref);
		}
		for (i = 0; authen->username[i] != NULL; i++)
			free(authen->username[i]);
		free(authen->username);
		free(authen);
	}
	TAILQ_FOREACH_SAFE(module, &radiusd->module, next, modulet) {
		TAILQ_REMOVE(&radiusd->module, module, next);
		radiusd_module_unload(module);
	}
	TAILQ_FOREACH_SAFE(client, &radiusd->client, next, clientt) {
		TAILQ_REMOVE(&radiusd->client, client, next);
		explicit_bzero(client->secret, sizeof(client->secret));
		free(client);
	}
	TAILQ_FOREACH_SAFE(listn, &radiusd->listen, next, listnt) {
		TAILQ_REMOVE(&radiusd->listen, listn, next);
		free(listn);
	}
	free(radiusd);
}

/***********************************************************************
 * Network event handlers
 ***********************************************************************/
#define IPv4_cmp(_in, _addr, _mask) (				\
	((_in)->s_addr & (_mask)->addr.ipv4.s_addr) ==		\
	    (_addr)->addr.ipv4.s_addr)
#define	s6_addr32(_in6)	((uint32_t *)(_in6)->s6_addr)
#define IPv6_cmp(_in6, _addr, _mask) (				\
	((s6_addr32(_in6)[3] & (_mask)->addr.addr32[3])		\
	    == (_addr)->addr.addr32[3]) &&			\
	((s6_addr32(_in6)[2] & (_mask)->addr.addr32[2])		\
	    == (_addr)->addr.addr32[2]) &&			\
	((s6_addr32(_in6)[1] & (_mask)->addr.addr32[1])		\
	    == (_addr)->addr.addr32[1]) &&			\
	((s6_addr32(_in6)[0] & (_mask)->addr.addr32[0])		\
	    == (_addr)->addr.addr32[0]))

static void
radiusd_listen_on_event(int fd, short evmask, void *ctx)
{
	int				 sz;
	RADIUS_PACKET			*packet = NULL;
	struct sockaddr_storage		 peer;
	socklen_t			 peersz;
	struct radiusd_listen		*listn = ctx;
	static u_char			 buf[65535];

	if (evmask & EV_READ) {
		peersz = sizeof(peer);
		if ((sz = recvfrom(listn->sock, buf, sizeof(buf), 0,
		    (struct sockaddr *)&peer, &peersz)) == -1) {
			if (errno == EAGAIN)
				return;
			log_warn("%s: recvfrom() failed", __func__);
			return;
		}
		RADIUSD_ASSERT(peer.ss_family == AF_INET ||
		    peer.ss_family == AF_INET6);
		if ((packet = radius_convert_packet(buf, sz)) == NULL)
			log_warn("%s: radius_convert_packet() failed",
			    __func__);
		else
			radiusd_listen_handle_packet(listn, packet,
			    (struct sockaddr *)&peer, peersz);
	}
}

static void
radiusd_listen_handle_packet(struct radiusd_listen *listn,
    RADIUS_PACKET *packet, struct sockaddr *peer, socklen_t peerlen)
{
	int				 i, req_id, req_code;
	static char			 username[256];
	char				 peerstr[NI_MAXHOST + NI_MAXSERV + 30];
	struct radiusd_authentication	*authen;
	struct radiusd_client		*client;
	struct radius_query		*q = NULL;
#define in(_x)	(((struct sockaddr_in  *)_x)->sin_addr)
#define in6(_x)	(((struct sockaddr_in6 *)_x)->sin6_addr)

	req_id = radius_get_id(packet);
	req_code = radius_get_code(packet);
	/* prepare some information about this messages */
	if (addrport_tostring(peer, peerlen, peerstr, sizeof(peerstr)) ==
	    NULL) {
		log_warn("%s: getnameinfo() failed", __func__);
		goto on_error;
	}

	/*
	 * Find a matching `client' entry
	 */
	TAILQ_FOREACH(client, &listn->radiusd->client, next) {
		if (client->af != peer->sa_family)
			continue;
		if (peer->sa_family == AF_INET && IPv4_cmp(
		    &in(peer), &client->addr, &client->mask))
			break;
		else if (peer->sa_family == AF_INET6 && IPv6_cmp(
		    &in6(peer), &client->addr, &client->mask))
			break;
	}
	if (client == NULL) {
		log_warnx("Received %s(code=%d) from %s id=%d: no `client' "
		    "matches", radius_code_string(req_code), req_code, peerstr,
		    req_id);
		goto on_error;
	}

	/* Check the client's Message-Authenticator */
	if (client->msgauth_required && !radius_has_attr(packet,
	    RADIUS_TYPE_MESSAGE_AUTHENTICATOR)) {
		log_warnx("Received %s(code=%d) from %s id=%d: no message "
		    "authenticator", radius_code_string(req_code), req_code,
		    peerstr, req_id);
		goto on_error;
	}

	if (radius_has_attr(packet, RADIUS_TYPE_MESSAGE_AUTHENTICATOR) &&
	    radius_check_message_authenticator(packet, client->secret) != 0) {
		log_warnx("Received %s(code=%d) from %s id=%d: bad message "
		    "authenticator", radius_code_string(req_code), req_code,
		    peerstr, req_id);
		goto on_error;
	}

	/*
	 * Find a duplicate request.  In RFC 2865, it has the same source IP
	 * address and source UDP port and Identifier.
	 */
	TAILQ_FOREACH(q, &listn->radiusd->query, next) {
		if (peer->sa_family == q->clientaddr.ss_family &&
		    ((peer->sa_family == AF_INET && in(&q->clientaddr).s_addr ==
		    in(peer).s_addr) || (peer->sa_family == AF_INET6 &&
		    IN6_ARE_ADDR_EQUAL(&in6(&q->clientaddr), &in6(peer)))) &&
		    ((struct sockaddr_in *)&q->clientaddr)->sin_port ==
		    ((struct sockaddr_in *)peer)->sin_port &&
		    req_id == q->req_id)
			break;	/* found it */
	}
	if (q != NULL) {
		log_info("Received %s(code=%d) from %s id=%d: duplicate "
		    "request by q=%u", radius_code_string(req_code), req_code,
		    peerstr, req_id, q->id);
		/* XXX RFC 5080 suggests to answer the cached result */
		goto on_error;
	}

	if ((q = calloc(1, sizeof(struct radius_query))) == NULL) {
		log_warn("%s: Out of memory", __func__);
		goto on_error;
	}
	if (radius_get_string_attr(packet, RADIUS_TYPE_USER_NAME, username,
	    sizeof(username)) != 0) {
		log_info("Received %s(code=%d) from %s id=%d: no User-Name "
		    "attribute", radius_code_string(req_code), req_code,
		    peerstr, req_id);
	} else
		strlcpy(q->username, username, sizeof(q->username));

	q->id = ++radius_query_id_seq;
	q->clientaddrlen = peerlen;
	memcpy(&q->clientaddr, peer, peerlen);
	q->listen = listn;
	q->req = packet;
	q->client = client;
	q->req_id = req_id;
	radius_get_authenticator(packet, q->req_auth);
	packet = NULL;
	TAILQ_INSERT_TAIL(&listn->radiusd->query, q, next);

	switch (req_code) {
	case RADIUS_CODE_ACCESS_REQUEST:
		/*
		 * Find a matching `authenticate' entry
		 */
		TAILQ_FOREACH(authen, &listn->radiusd->authen, next) {
			for (i = 0; authen->username[i] != NULL; i++) {
				if (fnmatch(authen->username[i], username, 0)
				    == 0)
					goto found;
			}
		}
 found:
		if (authen == NULL) {
			log_warnx("Received %s(code=%d) from %s id=%d "
			    "username=%s: no `authenticate' matches.",
			    radius_code_string(req_code), req_code, peerstr,
			    req_id, username);
			goto on_error;
		}
		q->authen = authen;

		if (!MODULE_DO_USERPASS(authen->auth->module) &&
		    !MODULE_DO_ACCSREQ(authen->auth->module)) {
			log_warnx("Received %s(code=%d) from %s id=%d "
			    "username=%s: module `%s' is not running.",
			    radius_code_string(req_code), req_code, peerstr,
			    req_id, username, authen->auth->module->name);
			goto on_error;
		}

		log_info("Received %s(code=%d) from %s id=%d username=%s "
		    "q=%u: `%s' authentication is starting",
		    radius_code_string(req_code), req_code, peerstr, q->req_id,
		    q->username, q->id, q->authen->auth->module->name);

		raidus_query_access_request(q);
		return;
	default:
		log_info("Received %s(code=%d) from %s id=%d: %s is not "
		    "supported in this implementation", radius_code_string(
		    req_code), req_code, peerstr, req_id, radius_code_string(
		    req_code));
		break;
	}
on_error:
	if (packet != NULL)
		radius_delete_packet(packet);
	if (q != NULL)
		radiusd_access_request_aborted(q);
#undef in
#undef in6
}

static void
raidus_query_access_request(struct radius_query *q)
{
	struct radiusd_authentication	*authen = q->authen;

	/* first or next request decoration */
	for (;;) {
		if (q->deco == NULL)
			q->deco = TAILQ_FIRST(&q->authen->deco);
		else
			q->deco = TAILQ_NEXT(q->deco, next);
		if (q->deco == NULL || MODULE_DO_REQDECO(q->deco->module))
			break;
	}

	if (q->deco != NULL)
		radiusd_module_request_decoration(q->deco->module, q);
	else {
		RADIUSD_ASSERT(authen->auth != NULL);
		if (MODULE_DO_ACCSREQ(authen->auth->module))
			radiusd_module_access_request(authen->auth->module, q);
		else if (MODULE_DO_USERPASS(authen->auth->module))
			radiusd_module_userpass(authen->auth->module, q);
	}
}

static void
radius_query_access_response(struct radius_query *q)
{
	int		 sz, res_id, res_code;
	char		 buf[NI_MAXHOST + NI_MAXSERV + 30];

	/* first or next response decoration */
	for (;;) {
		if (q->deco == NULL)
			q->deco = TAILQ_FIRST(&q->authen->deco);
		else
			q->deco = TAILQ_NEXT(q->deco, next);
		if (q->deco == NULL || MODULE_DO_RESDECO(q->deco->module))
			break;
	}

	if (q->deco != NULL) {
		radiusd_module_response_decoration(q->deco->module, q);
		return;
	}

	if (radiusd_access_response_fixup(q) != 0)
		goto on_error;

	res_id = radius_get_id(q->res);
	res_code = radius_get_code(q->res);

	/* Reset response/message authenticator */
	if (radius_has_attr(q->res, RADIUS_TYPE_MESSAGE_AUTHENTICATOR))
		radius_del_attr_all(q->res, RADIUS_TYPE_MESSAGE_AUTHENTICATOR);
	radius_put_message_authenticator(q->res, q->client->secret);
	radius_set_response_authenticator(q->res, q->client->secret);

	log_info("Sending %s(code=%d) to %s id=%u q=%u",
	    radius_code_string(res_code), res_code,
	    addrport_tostring((struct sockaddr *)&q->clientaddr,
		    q->clientaddrlen, buf, sizeof(buf)), res_id, q->id);

	if ((sz = sendto(q->listen->sock, radius_get_data(q->res),
	    radius_get_length(q->res), 0,
	    (struct sockaddr *)&q->clientaddr, q->clientaddrlen)) <= 0)
		log_warn("Sending a RADIUS response failed");
on_error:
	radiusd_access_request_aborted(q);
}

/***********************************************************************
 * Callback functions from the modules
 ***********************************************************************/
void
radiusd_access_request_answer(struct radius_query *q)
{
	const char	*authen_secret = q->authen->auth->module->secret;

	radius_set_request_packet(q->res, q->req);

	if (authen_secret == NULL) {
		/*
		 * The module diddn't check the authenticators
		 */
		if (radius_check_response_authenticator(q->res,
		    q->client->secret) != 0) {
			log_info("Response from module has bad response "
			    "authenticator: id=%d", q->id);
			goto on_error;
		}
		if (radius_has_attr(q->res,
		    RADIUS_TYPE_MESSAGE_AUTHENTICATOR) &&
		    radius_check_message_authenticator(q->res,
		    q->client->secret) != 0) {
			log_info("Response from module has bad message "
			    "authenticator: id=%d", q->id);
			goto on_error;
		}
	}

	RADIUSD_ASSERT(q->deco == NULL);
	radius_query_access_response(q);

	return;
on_error:
	radiusd_access_request_aborted(q);
}

void
radiusd_access_request_aborted(struct radius_query *q)
{
	if (q->req != NULL)
		radius_delete_packet(q->req);
	if (q->res != NULL)
		radius_delete_packet(q->res);
	TAILQ_REMOVE(&q->listen->radiusd->query, q, next);
	free(q);
}

/***********************************************************************
 * Signal handlers
 ***********************************************************************/
static void
radiusd_on_sigterm(int fd, short evmask, void *ctx)
{
	struct radiusd	*radiusd = ctx;

	log_info("Received SIGTERM");
	radiusd_stop(radiusd);
}

static void
radiusd_on_sigint(int fd, short evmask, void *ctx)
{
	struct radiusd	*radiusd = ctx;

	log_info("Received SIGINT");
	radiusd_stop(radiusd);
}

static void
radiusd_on_sighup(int fd, short evmask, void *ctx)
{
	log_info("Received SIGHUP");
}

static void
radiusd_on_sigchld(int fd, short evmask, void *ctx)
{
	struct radiusd		*radiusd = ctx;
	struct radiusd_module	*module;
	pid_t			 pid;
	int			 status;

	log_debug("Received SIGCHLD");
	while ((pid = wait3(&status, WNOHANG, NULL)) != 0) {
		if (pid == -1)
			break;
		TAILQ_FOREACH(module, &radiusd->module, next) {
			if (module->pid == pid) {
				if (WIFEXITED(status))
					log_warnx("module `%s'(pid=%d) exited "
					    "with status %d", module->name,
					    (int)pid, WEXITSTATUS(status));
				else
					log_warnx("module `%s'(pid=%d) exited "
					    "by signal %d", module->name,
					    (int)pid, WTERMSIG(status));
				break;
			}
		}
		if (!module) {
			if (WIFEXITED(status))
				log_warnx("unkown child process pid=%d exited "
				    "with status %d", (int)pid,
				     WEXITSTATUS(status));
			else
				log_warnx("unkown child process pid=%d exited "
				    "by signal %d", (int)pid,
				    WTERMSIG(status));
		}
	}
}

static const char *
radius_code_string(int code)
{
	int			i;
	struct _codestrings {
		int		 code;
		const char	*string;
	} codestrings[] = {
	    { RADIUS_CODE_ACCESS_REQUEST,	"Access-Request" },
	    { RADIUS_CODE_ACCESS_ACCEPT,	"Access-Accept" },
	    { RADIUS_CODE_ACCESS_REJECT,	"Access-Reject" },
	    { RADIUS_CODE_ACCOUNTING_REQUEST,	"Accounting-Request" },
	    { RADIUS_CODE_ACCOUNTING_RESPONSE,	"Accounting-Response" },
	    { RADIUS_CODE_ACCESS_CHALLENGE,	"Access-Challenge" },
	    { RADIUS_CODE_STATUS_SERVER,	"Status-Server" },
	    { RADIUS_CODE_STATUS_CLIENT,	"Status-Client" },
	    { -1,				NULL }
	};

	for (i = 0; codestrings[i].code != -1; i++)
		if (codestrings[i].code == code)
			return (codestrings[i].string);

	return ("Unknown");
}

void
radiusd_conf_init(struct radiusd *conf)
{

	TAILQ_INIT(&conf->listen);
	TAILQ_INIT(&conf->module);
	TAILQ_INIT(&conf->authen);
	TAILQ_INIT(&conf->client);

	return;
}

/*
 * Fix some attributes which depend the secret value.
 */
static int
radiusd_access_response_fixup(struct radius_query *q)
{
	int		 res_id;
	size_t		 attrlen;
	u_char		 req_auth[16], attrbuf[256];
	const char	*authen_secret = q->authen->auth->module->secret;

	radius_get_authenticator(q->req, req_auth);

	if ((authen_secret != NULL &&
	    strcmp(authen_secret, q->client->secret) != 0) ||
	    timingsafe_bcmp(q->req_auth, req_auth, 16) != 0) {
		const char *olds = q->client->secret;
		const char *news = authen_secret;

		if (news == NULL)
			news = olds;

		/* RFC 2865 Tunnel-Password */
		attrlen = sizeof(attrbuf);
		if (radius_get_raw_attr(q->res, RADIUS_TYPE_TUNNEL_PASSWORD,
		    attrbuf, &attrlen) == 0) {
			radius_attr_unhide(news, req_auth,
			    attrbuf, attrbuf + 3, attrlen - 3);
			radius_attr_hide(olds, q->req_auth,
			    attrbuf, attrbuf + 3, attrlen - 3);

			radius_del_attr_all(q->res,
			    RADIUS_TYPE_TUNNEL_PASSWORD);
			radius_put_raw_attr(q->res,
			    RADIUS_TYPE_TUNNEL_PASSWORD, attrbuf, attrlen);
		}

		/* RFC 2548 Microsoft MPPE-{Send,Recv}-Key */
		attrlen = sizeof(attrbuf);
		if (radius_get_vs_raw_attr(q->res, RADIUS_VENDOR_MICROSOFT,
		    RADIUS_VTYPE_MPPE_SEND_KEY, attrbuf, &attrlen) == 0) {

			/* Re-crypt the KEY */
			radius_attr_unhide(news, req_auth,
			    attrbuf, attrbuf + 2, attrlen - 2);
			radius_attr_hide(olds, q->req_auth,
			    attrbuf, attrbuf + 2, attrlen - 2);

			radius_del_vs_attr_all(q->res, RADIUS_VENDOR_MICROSOFT,
			    RADIUS_VTYPE_MPPE_SEND_KEY);
			radius_put_vs_raw_attr(q->res, RADIUS_VENDOR_MICROSOFT,
			    RADIUS_VTYPE_MPPE_SEND_KEY, attrbuf, attrlen);
		}
		attrlen = sizeof(attrbuf);
		if (radius_get_vs_raw_attr(q->res, RADIUS_VENDOR_MICROSOFT,
		    RADIUS_VTYPE_MPPE_RECV_KEY, attrbuf, &attrlen) == 0) {

			/* Re-crypt the KEY */
			radius_attr_unhide(news, req_auth,
			    attrbuf, attrbuf + 2, attrlen - 2);
			radius_attr_hide(olds, q->req_auth,
			    attrbuf, attrbuf + 2, attrlen - 2);

			radius_del_vs_attr_all(q->res, RADIUS_VENDOR_MICROSOFT,
			    RADIUS_VTYPE_MPPE_RECV_KEY);
			radius_put_vs_raw_attr(q->res, RADIUS_VENDOR_MICROSOFT,
			    RADIUS_VTYPE_MPPE_RECV_KEY, attrbuf, attrlen);
		}
	}

	res_id = radius_get_id(q->res);
	if (res_id != q->req_id) {
		/* authentication server change the id */
		radius_set_id(q->res, q->req_id);
	}

	return (0);
}

void
radius_attr_hide(const char *secret, const char *authenticator,
    const u_char *salt, u_char *plain, int plainlen)
{
	int	  i, j;
	u_char	  b[16];
	MD5_CTX	  md5ctx;

	i = 0;
	do {
		MD5Init(&md5ctx);
		MD5Update(&md5ctx, secret, strlen(secret));
		if (i == 0) {
			MD5Update(&md5ctx, authenticator, 16);
			if (salt != NULL)
				MD5Update(&md5ctx, salt, 2);
		} else
			MD5Update(&md5ctx, plain + i - 16, 16);
		MD5Final(b, &md5ctx);

		for (j = 0; j < 16 && i < plainlen; i++, j++)
			plain[i] ^= b[j];
	} while (i < plainlen);
}

void
radius_attr_unhide(const char *secret, const char *authenticator,
    const u_char *salt, u_char *crypt0, int crypt0len)
{
	int	  i, j;
	u_char	  b[16];
	MD5_CTX	  md5ctx;

	i = 16 * ((crypt0len - 1) / 16);
	while (i >= 0) {
		MD5Init(&md5ctx);
		MD5Update(&md5ctx, secret, strlen(secret));
		if (i == 0) {
			MD5Update(&md5ctx, authenticator, 16);
			if (salt != NULL)
				MD5Update(&md5ctx, salt, 2);
		} else
			MD5Update(&md5ctx, crypt0 + i - 16, 16);
		MD5Final(b, &md5ctx);

		for (j = 0; j < 16 && i + j < crypt0len; j++)
			crypt0[i + j] ^= b[j];
		i -= 16;
	}
}

static struct radius_query *
radiusd_find_query(struct radiusd *radiusd, u_int q_id)
{
	struct radius_query	*q;

	TAILQ_FOREACH(q, &radiusd->query, next) {
		if (q->id == q_id)
			return (q);
	}
	return (NULL);
}

/***********************************************************************
 * radiusd module handling
 ***********************************************************************/
struct radiusd_module *
radiusd_module_load(struct radiusd *radiusd, const char *path, const char *name)
{
	struct radiusd_module		*module = NULL;
	pid_t				 pid;
	int				 ival, pairsock[] = { -1, -1 };
	const char			*av[3];
	ssize_t				 n;
	struct imsg			 imsg;

	module = calloc(1, sizeof(struct radiusd_module));
	if (module == NULL)
		fatal("Out of memory");
	module->radiusd = radiusd;

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pairsock) == -1) {
		log_warn("Could not load module `%s'(%s): pipe()", name, path);
		goto on_error;
	}

	pid = fork();
	if (pid == -1) {
		log_warn("Could not load module `%s'(%s): fork()", name, path);
		goto on_error;
	}
	if (pid == 0) {
		setsid();
		close(pairsock[0]);
		av[0] = path;
		av[1] = name;
		av[2] = NULL;
		dup2(pairsock[1], STDIN_FILENO);
		dup2(pairsock[1], STDOUT_FILENO);
		close(pairsock[1]);
		closefrom(STDERR_FILENO + 1);
		execv(path, (char * const *)av);
		log_warn("Failed to execute %s", path);
		_exit(EXIT_FAILURE);
	}
	close(pairsock[1]);

	module->fd = pairsock[0];
	if ((ival = fcntl(module->fd, F_GETFL)) == -1) {
		log_warn("Could not load module `%s': fcntl(F_GETFL)",
		    name);
		goto on_error;
	}
	if (fcntl(module->fd, F_SETFL, ival | O_NONBLOCK) == -1) {
		log_warn(
		    "Could not load module `%s': fcntl(F_SETFL,O_NONBLOCK)",
		    name);
		goto on_error;
	}
	strlcpy(module->name, name, sizeof(module->name));
	module->pid = pid;
	imsg_init(&module->ibuf, module->fd);

	if (imsg_sync_read(&module->ibuf, MODULE_IO_TIMEOUT) <= 0 ||
	    (n = imsg_get(&module->ibuf, &imsg)) <= 0) {
		log_warnx("Could not load module `%s': module didn't "
		    "respond", name);
		goto on_error;
	}
	if (imsg.hdr.type != IMSG_RADIUSD_MODULE_LOAD) {
		imsg_free(&imsg);
		log_warnx("Could not load module `%s': unknown imsg type=%d",
		    name, imsg.hdr.type);
		goto on_error;
	}

	module->capabilities =
	    ((struct radiusd_module_load_arg *)imsg.data)->cap;

	log_debug("Loaded module `%s' successfully.  pid=%d", module->name,
	    module->pid);
	imsg_free(&imsg);

	return (module);

on_error:
	free(module);
	if (pairsock[0] >= 0)
		close(pairsock[0]);
	if (pairsock[1] >= 0)
		close(pairsock[1]);

	return (NULL);
}

void
radiusd_module_start(struct radiusd_module *module)
{
	int		 datalen;
	struct imsg	 imsg;
	struct timeval	 tv = { 0, 0 };

	RADIUSD_ASSERT(module->fd >= 0);
	imsg_compose(&module->ibuf, IMSG_RADIUSD_MODULE_START, 0, 0, -1,
	    NULL, 0);
	imsg_sync_flush(&module->ibuf, MODULE_IO_TIMEOUT);
	if (imsg_sync_read(&module->ibuf, MODULE_IO_TIMEOUT) <= 0 ||
	    imsg_get(&module->ibuf, &imsg) <= 0) {
		log_warnx("Module `%s' could not start: no response",
		    module->name);
		goto on_fail;
	}

	datalen = imsg.hdr.len - IMSG_HEADER_SIZE;
	if (imsg.hdr.type != IMSG_OK) {
		if (imsg.hdr.type == IMSG_NG) {
			if (datalen > 0)
				log_warnx("Module `%s' could not start: %s",
				    module->name, (char *)imsg.data);
			else
				log_warnx("Module `%s' could not start",
				    module->name);
		} else
			log_warnx("Module `%s' could not started: module "
			    "returned unknown message type %d", module->name,
			    imsg.hdr.type);
		goto on_fail;
	}

	event_set(&module->ev, module->fd, EV_READ, radiusd_module_on_imsg_io,
	    module);
	event_add(&module->ev, &tv);
	log_debug("Module `%s' started successfully", module->name);

	return;
on_fail:
	radiusd_module_close(module);
	return;
}

void
radiusd_module_stop(struct radiusd_module *module)
{
	module->stopped = true;

	if (module->secret != NULL) {
		freezero(module->secret, strlen(module->secret));
		module->secret = NULL;
	}

	if (module->fd >= 0) {
		imsg_compose(&module->ibuf, IMSG_RADIUSD_MODULE_STOP, 0, 0, -1,
		    NULL, 0);
		radiusd_module_reset_ev_handler(module);
	}
}

static void
radiusd_module_close(struct radiusd_module *module)
{
	if (module->fd >= 0) {
		event_del(&module->ev);
		imsg_clear(&module->ibuf);
		close(module->fd);
		module->fd = -1;
	}
}

void
radiusd_module_unload(struct radiusd_module *module)
{
	free(module->radpkt);
	radiusd_module_close(module);
	free(module);
}

static void
radiusd_module_on_imsg_io(int fd, short evmask, void *ctx)
{
	struct radiusd_module	*module = ctx;
	int			 ret;

	if (evmask & EV_WRITE)
		module->writeready = true;

	if (evmask & EV_READ || module->ibuf.r.wpos > IMSG_HEADER_SIZE) {
		if (radiusd_module_imsg_read(module,
		    (evmask & EV_READ)? true : false) == -1)
			goto on_error;
	}

	while (module->writeready && module->ibuf.w.queued) {
		ret = msgbuf_write(&module->ibuf.w);
		if (ret > 0)
			continue;
		module->writeready = false;
		if (ret == 0 && errno == EAGAIN)
			break;
		log_warn("Failed to write to module `%s': msgbuf_write()",
		    module->name);
		goto on_error;
	}
	radiusd_module_reset_ev_handler(module);

	return;
on_error:
	radiusd_module_close(module);
}

static void
radiusd_module_reset_ev_handler(struct radiusd_module *module)
{
	short		 evmask;
	struct timeval	*tvp = NULL, tv = { 0, 0 };

	RADIUSD_ASSERT(module->fd >= 0);
	event_del(&module->ev);

	evmask = EV_READ;
	if (module->ibuf.w.queued) {
		if (!module->writeready)
			evmask |= EV_WRITE;
		else
			tvp = &tv;	/* fire immediately */
	} else if (module->ibuf.r.wpos > IMSG_HEADER_SIZE)
		tvp = &tv;		/* fire immediately */

	/* module stopped and no event handler is set */
	if (evmask & EV_WRITE && tvp == NULL && module->stopped) {
		/* stop requested and no more to write */
		radiusd_module_close(module);
		return;
	}

	event_set(&module->ev, module->fd, evmask, radiusd_module_on_imsg_io,
	    module);
	if (event_add(&module->ev, tvp) == -1) {
		log_warn("Could not set event handlers for module `%s': "
		    "event_add()", module->name);
		radiusd_module_close(module);
	}
}

static int
radiusd_module_imsg_read(struct radiusd_module *module, bool doread)
{
	int		 n;
	struct imsg	 imsg;

	if (doread) {
		if ((n = imsg_read(&module->ibuf)) == -1 || n == 0) {
			if (n == -1 && errno == EAGAIN)
				return (0);
			if (n == -1)
				log_warn("Receiving a message from module `%s' "
				    "failed: imsg_read", module->name);
			/* else closed */
			radiusd_module_close(module);
			return (-1);
		}
	}
	for (;;) {
		if ((n = imsg_get(&module->ibuf, &imsg)) == -1) {
			log_warn("Receiving a message from module `%s' failed: "
			    "imsg_get", module->name);
			return (-1);
		}
		if (n == 0)
			return (0);
		radiusd_module_imsg(module, &imsg);
	}

	return (0);
}

static void
radiusd_module_imsg(struct radiusd_module *module, struct imsg *imsg)
{
	int			 datalen;
	struct radius_query	*q;
	u_int			 q_id;

	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;
	switch (imsg->hdr.type) {
	case IMSG_RADIUSD_MODULE_NOTIFY_SECRET:
		if (datalen > 0) {
			module->secret = strdup(imsg->data);
			if (module->secret == NULL)
				log_warn("Could not handle NOTIFY_SECRET "
				    "from `%s'", module->name);
		}
		break;
	case IMSG_RADIUSD_MODULE_USERPASS_OK:
	case IMSG_RADIUSD_MODULE_USERPASS_FAIL:
	    {
		char			*msg = NULL;
		const char		*msgtypestr;

		msgtypestr = (imsg->hdr.type == IMSG_RADIUSD_MODULE_USERPASS_OK)
		    ? "USERPASS_OK" : "USERPASS_NG";

		q_id = *(u_int *)imsg->data;
		if (datalen > (ssize_t)sizeof(u_int))
			msg = (char *)(((u_int *)imsg->data) + 1);

		q = radiusd_find_query(module->radiusd, q_id);
		if (q == NULL) {
			log_warnx("Received %s from `%s', but query id=%u "
			    "unknown", msgtypestr, module->name, q_id);
			break;
		}

		if ((q->res = radius_new_response_packet(
		    (imsg->hdr.type == IMSG_RADIUSD_MODULE_USERPASS_OK)
		    ? RADIUS_CODE_ACCESS_ACCEPT : RADIUS_CODE_ACCESS_REJECT,
		    q->req)) == NULL) {
			log_warn("radius_new_response_packet() failed");
			radiusd_access_request_aborted(q);
		} else {
			if (msg)
				radius_put_string_attr(q->res,
				    RADIUS_TYPE_REPLY_MESSAGE, msg);
			radius_set_response_authenticator(q->res,
			    q->client->secret);
			radiusd_access_request_answer(q);
		}
		break;
	    }
	case IMSG_RADIUSD_MODULE_ACCSREQ_ANSWER:
	case IMSG_RADIUSD_MODULE_REQDECO_DONE:
	case IMSG_RADIUSD_MODULE_RESDECO_DONE:
	    {
		static struct radiusd_module_radpkt_arg *ans;
		const char *typestr = "unknown";

		switch (imsg->hdr.type) {
		case IMSG_RADIUSD_MODULE_ACCSREQ_ANSWER:
			typestr = "ACCSREQ_ANSWER";
			break;
		case IMSG_RADIUSD_MODULE_REQDECO_DONE:
			typestr = "REQDECO_DONE";
			break;
		case IMSG_RADIUSD_MODULE_RESDECO_DONE:
			typestr = "RESDECO_DONE";
			break;
		}

		if (datalen <
		    (ssize_t)sizeof(struct radiusd_module_radpkt_arg)) {
			log_warnx("Received %s message, but length is wrong",
			    typestr);
			break;
		}
		q_id = ((struct radiusd_module_radpkt_arg *)imsg->data)->q_id;
		q = radiusd_find_query(module->radiusd, q_id);
		if (q == NULL) {
			log_warnx("Received %s from %s, but query id=%u "
			    "unknown", typestr, module->name, q_id);
			break;
		}
		if ((ans = radiusd_module_recv_radpkt(module, imsg,
		    imsg->hdr.type, typestr)) != NULL) {
			RADIUS_PACKET *radpkt = NULL;

			if (module->radpktoff > 0 &&
			    (radpkt = radius_convert_packet(
			    module->radpkt, module->radpktoff)) == NULL) {
				log_warn("q=%u radius_convert_packet() failed",
				    q->id);
				radiusd_access_request_aborted(q);
				break;
			}
			module->radpktoff = 0;
			switch (imsg->hdr.type) {
			case IMSG_RADIUSD_MODULE_REQDECO_DONE:
				if (radpkt != NULL) {
					radius_delete_packet(q->req);
					q->req = radpkt;
				}
				raidus_query_access_request(q);
				break;
			case IMSG_RADIUSD_MODULE_ACCSREQ_ANSWER:
				if (radpkt == NULL) {
					log_warn("q=%u wrong pkt from module",
					    q->id);
					radiusd_access_request_aborted(q);
				}
				q->res = radpkt;
				radiusd_access_request_answer(q);
				break;
			case IMSG_RADIUSD_MODULE_RESDECO_DONE:
				if (radpkt != NULL) {
					radius_delete_packet(q->res);
					radius_set_request_packet(radpkt,
					    q->req);
					q->res = radpkt;
				}
				radius_query_access_response(q);
				break;
			}
		}
		break;
	    }
	case IMSG_RADIUSD_MODULE_ACCSREQ_ABORTED:
	    {
		q_id = *((u_int *)imsg->data);
		q = radiusd_find_query(module->radiusd, q_id);
		if (q == NULL) {
			log_warnx("Received ACCSREQ_ABORT from %s, but query "
			    "id=%u unknown", module->name, q_id);
			break;
		}
		radiusd_access_request_aborted(q);
		break;
	    }
	default:
		RADIUSD_DBG(("Unhandled imsg type=%d from %s", imsg->hdr.type,
		    module->name));
	}
}

static struct radiusd_module_radpkt_arg *
radiusd_module_recv_radpkt(struct radiusd_module *module, struct imsg *imsg,
    uint32_t imsg_type, const char *type_str)
{
	struct radiusd_module_radpkt_arg	*ans;
	int					 datalen, chunklen;

	datalen = imsg->hdr.len - IMSG_HEADER_SIZE;
	ans = (struct radiusd_module_radpkt_arg *)imsg->data;
	if (module->radpktsiz < ans->pktlen) {
		u_char *nradpkt;
		if ((nradpkt = realloc(module->radpkt, ans->pktlen)) == NULL) {
			log_warn("Could not handle received %s message from "
			    "`%s'", type_str, module->name);
			goto on_fail;
		}
		module->radpkt = nradpkt;
		module->radpktsiz = ans->pktlen;
	}
	chunklen = datalen - sizeof(struct radiusd_module_radpkt_arg);
	if (chunklen > module->radpktsiz - module->radpktoff) {
		log_warnx("Could not handle received %s message from `%s': "
		    "received length is too big", type_str, module->name);
		goto on_fail;
	}
	if (chunklen > 0) {
		memcpy(module->radpkt + module->radpktoff,
		    (caddr_t)(ans + 1), chunklen);
		module->radpktoff += chunklen;
	}
	if (!ans->final)
		return (NULL);	/* again */
	if (module->radpktoff != ans->pktlen) {
		log_warnx("Could not handle received %s message from `%s': "
		    "length is mismatch", type_str, module->name);
		goto on_fail;
	}

	return (ans);
on_fail:
	module->radpktoff = 0;
	return (NULL);
}

int
radiusd_module_set(struct radiusd_module *module, const char *name,
    int argc, char * const * argv)
{
	struct radiusd_module_set_arg	 arg;
	struct radiusd_module_object	*val;
	int				 i, niov = 0;
	u_char				*buf = NULL, *buf0;
	ssize_t				 n;
	size_t				 bufsiz = 0, bufoff = 0, bufsiz0;
	size_t				 vallen, valsiz;
	struct iovec			 iov[2];
	struct imsg			 imsg;

	memset(&arg, 0, sizeof(arg));
	arg.nparamval = argc;
	strlcpy(arg.paramname, name, sizeof(arg.paramname));

	iov[niov].iov_base = &arg;
	iov[niov].iov_len = sizeof(struct radiusd_module_set_arg);
	niov++;

	for (i = 0; i < argc; i++) {
		vallen = strlen(argv[i]) + 1;
		valsiz = sizeof(struct radiusd_module_object) + vallen;
		if (bufsiz < bufoff + valsiz) {
			bufsiz0 = bufoff + valsiz + 128;
			if ((buf0 = realloc(buf, bufsiz0)) == NULL) {
				log_warn("Failed to set config parameter to "
				    "module `%s': realloc", module->name);
				goto on_error;
			}
			buf = buf0;
			bufsiz = bufsiz0;
			memset(buf + bufoff, 0, bufsiz - bufoff);
		}
		val = (struct radiusd_module_object *)(buf + bufoff);
		val->size = valsiz;
		memcpy(val + 1, argv[i], vallen);

		bufoff += valsiz;
	}
	iov[niov].iov_base = buf;
	iov[niov].iov_len = bufoff;
	niov++;

	if (imsg_composev(&module->ibuf, IMSG_RADIUSD_MODULE_SET_CONFIG, 0, 0,
	    -1, iov, niov) == -1) {
		log_warn("Failed to set config parameter to module `%s': "
		    "imsg_composev", module->name);
		goto on_error;
	}
	if (imsg_sync_flush(&module->ibuf, MODULE_IO_TIMEOUT) == -1) {
		log_warn("Failed to set config parameter to module `%s': "
		    "imsg_flush_timeout", module->name);
		goto on_error;
	}
	for (;;) {
		if (imsg_sync_read(&module->ibuf, MODULE_IO_TIMEOUT) <= 0) {
			log_warn("Failed to get reply from module `%s': "
			    "imsg_sync_read", module->name);
			goto on_error;
		}
		if ((n = imsg_get(&module->ibuf, &imsg)) > 0)
			break;
		if (n < 0) {
			log_warn("Failed to get reply from module `%s': "
			    "imsg_get", module->name);
			goto on_error;
		}
	}
	if (imsg.hdr.type == IMSG_NG) {
		log_warnx("Could not set `%s' for module `%s': %s", name,
		    module->name, (char *)imsg.data);
		goto on_error;
	} else if (imsg.hdr.type != IMSG_OK) {
		imsg_free(&imsg);
		log_warnx("Failed to get reply from module `%s': "
		    "unknown imsg type=%d", module->name, imsg.hdr.type);
		goto on_error;
	}
	imsg_free(&imsg);

	free(buf);
	return (0);

on_error:
	free(buf);
	return (-1);
}

static void
radiusd_module_userpass(struct radiusd_module *module, struct radius_query *q)
{
	struct radiusd_module_userpass_arg userpass;

	memset(&userpass, 0, sizeof(userpass));
	userpass.q_id = q->id;

	if (radius_get_user_password_attr(q->req, userpass.pass,
	    sizeof(userpass.pass), q->client->secret) == 0)
		userpass.has_pass = true;
	else
		userpass.has_pass = false;
	if (radius_get_string_attr(q->req, RADIUS_TYPE_USER_NAME,
	    userpass.user, sizeof(userpass.user)) != 0) {
		log_warnx("q=%u no User-Name attribute", q->id);
		goto on_error;
	}
	imsg_compose(&module->ibuf, IMSG_RADIUSD_MODULE_USERPASS, 0, 0, -1,
	    &userpass, sizeof(userpass));
	radiusd_module_reset_ev_handler(module);
	return;
on_error:
	radiusd_access_request_aborted(q);
}

static void
radiusd_module_access_request(struct radiusd_module *module,
    struct radius_query *q)
{
	RADIUS_PACKET				*radpkt;
	char					 pass[256];

	if ((radpkt = radius_convert_packet(radius_get_data(q->req),
	    radius_get_length(q->req))) == NULL) {
		log_warn("q=%u Could not send ACCSREQ to `%s'", q->id,
		    module->name);
		radiusd_access_request_aborted(q);
		return;
	}
	if (q->client->secret[0] != '\0' && module->secret != NULL &&
	    radius_get_user_password_attr(radpkt, pass, sizeof(pass),
		    q->client->secret) == 0) {
		radius_del_attr_all(radpkt, RADIUS_TYPE_USER_PASSWORD);
		(void)radius_put_raw_attr(radpkt, RADIUS_TYPE_USER_PASSWORD,
		    pass, strlen(pass));
	}
	if (imsg_compose_radius_packet(&module->ibuf,
	    IMSG_RADIUSD_MODULE_ACCSREQ, q->id, radpkt) == -1) {
		log_warn("q=%u Could not send ACCSREQ to `%s'", q->id,
		    module->name);
		radiusd_access_request_aborted(q);
	}
	radiusd_module_reset_ev_handler(module);
	radius_delete_packet(radpkt);
}

static void
radiusd_module_request_decoration(struct radiusd_module *module,
    struct radius_query *q)
{
	if (module->fd < 0) {
		log_warnx("q=%u Could not send REQDECO to `%s': module is "
		    "not running?", q->id, module->name);
		radiusd_access_request_aborted(q);
		return;
	}
	if (imsg_compose_radius_packet(&module->ibuf,
	    IMSG_RADIUSD_MODULE_REQDECO, q->id, q->req) == -1) {
		log_warn("q=%u Could not send REQDECO to `%s'", q->id,
		    module->name);
		radiusd_access_request_aborted(q);
		return;
	}
	radiusd_module_reset_ev_handler(module);
}

static void
radiusd_module_response_decoration(struct radiusd_module *module,
    struct radius_query *q)
{
	if (module->fd < 0) {
		log_warnx("q=%u Could not send RESDECO to `%s': module is "
		    "not running?", q->id, module->name);
		radiusd_access_request_aborted(q);
		return;
	}
	if (imsg_compose_radius_packet(&module->ibuf,
	    IMSG_RADIUSD_MODULE_RESDECO0_REQ, q->id, q->req) == -1) {
		log_warn("q=%u Could not send RESDECO0_REQ to `%s'", q->id,
		    module->name);
		radiusd_access_request_aborted(q);
		return;
	}
	if (imsg_compose_radius_packet(&module->ibuf,
	    IMSG_RADIUSD_MODULE_RESDECO, q->id, q->res) == -1) {
		log_warn("q=%u Could not send RESDECO to `%s'", q->id,
		    module->name);
		radiusd_access_request_aborted(q);
		return;
	}
	radiusd_module_reset_ev_handler(module);
}

static int
imsg_compose_radius_packet(struct imsgbuf *ibuf, uint32_t type, u_int q_id,
    RADIUS_PACKET *radpkt)
{
	struct radiusd_module_radpkt_arg	 arg;
	int					 off = 0, len, siz;
	struct iovec				 iov[2];
	const u_char				*pkt;

	pkt = radius_get_data(radpkt);
	len = radius_get_length(radpkt);
	memset(&arg, 0, sizeof(arg));
	arg.q_id = q_id;
	arg.pktlen = len;
	while (off < len) {
		siz = MAX_IMSGSIZE - sizeof(arg);
		if (len - off > siz)
			arg.final = false;
		else {
			arg.final = true;
			siz = len - off;
		}
		iov[0].iov_base = &arg;
		iov[0].iov_len = sizeof(arg);
		iov[1].iov_base = (caddr_t)pkt + off;
		iov[1].iov_len = siz;
		if (imsg_composev(ibuf, type, 0, 0, -1, iov, 2) == -1)
			return (-1);
		off += siz;
	}
	return (0);
}
