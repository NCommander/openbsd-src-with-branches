/* $OpenBSD: rebound.c,v 1.68 2016/09/01 10:54:36 tedu Exp $ */
/*
 * Copyright (c) 2015 Ted Unangst <tedu@openbsd.org>
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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/event.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <signal.h>
#include <syslog.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>

#define MINIMUM(a,b) (((a)<(b))?(a):(b))

uint16_t randomid(void);

union sockthing {
	struct sockaddr a;
	struct sockaddr_storage s;
	struct sockaddr_in i;
	struct sockaddr_in6 i6;
};

static struct timespec now;
static int debug;
static int daemonized;

struct dnspacket {
	uint16_t id;
	uint16_t flags;
	uint16_t qdcount;
	uint16_t ancount;
	uint16_t nscount;
	uint16_t arcount;
	/* ... */
};

/*
 * requests will point to cache entries until a response is received.
 * until then, the request owns the entry and must free it.
 * after it's on the list, the request must not free it.
 */
struct dnscache {
	TAILQ_ENTRY(dnscache) fifo;
	RB_ENTRY(dnscache) cachenode;
	struct dnspacket *req;
	size_t reqlen;
	struct dnspacket *resp;
	size_t resplen;
	struct timespec ts;
};
static TAILQ_HEAD(, dnscache) cachefifo;
static RB_HEAD(cachetree, dnscache) cachetree;
RB_PROTOTYPE_STATIC(cachetree, dnscache, cachenode, cachecmp)

static int cachecount;
static int cachemax;
static uint64_t cachehits;

/*
 * requests are kept on a fifo list, but only after socket s is set.
 */
struct request {
	int s;
	int client;
	int tcp;
	union sockthing from;
	socklen_t fromlen;
	struct timespec ts;
	TAILQ_ENTRY(request) fifo;
	uint16_t clientid;
	uint16_t reqid;
	struct dnscache *cacheent;
};
static TAILQ_HEAD(, request) reqfifo;

static int conncount;
static int connmax;
static uint64_t conntotal;
static int stopaccepting;

static void
logmsg(int prio, const char *msg, ...)
{
	va_list ap;

	if (debug || !daemonized) {
		va_start(ap, msg);
		vfprintf(stdout, msg, ap);
		fprintf(stdout, "\n");
		va_end(ap);
	}
	if (!debug) {
		va_start(ap, msg);
		vsyslog(LOG_DAEMON | prio, msg, ap);
		va_end(ap);
	}
}

static void __dead
logerr(const char *msg, ...)
{
	va_list ap;

	if (debug || !daemonized) {
		va_start(ap, msg);
		fprintf(stderr, "rebound: ");
		vfprintf(stderr, msg, ap);
		fprintf(stderr, "\n");
		va_end(ap);
	}
	if (!debug) {
		va_start(ap, msg);
		vsyslog(LOG_DAEMON | LOG_ERR, msg, ap);
		va_end(ap);
	}
	exit(1);
}

static int
cachecmp(struct dnscache *c1, struct dnscache *c2)
{
	if (c1->reqlen == c2->reqlen)
		return memcmp(c1->req, c2->req, c1->reqlen);
	return c1->reqlen < c2->reqlen ? -1 : 1;
}
RB_GENERATE_STATIC(cachetree, dnscache, cachenode, cachecmp)

static struct dnscache *
cachelookup(struct dnspacket *dnsreq, size_t reqlen)
{
	struct dnscache *hit, key;
	uint16_t origid;

	origid = dnsreq->id;
	dnsreq->id = 0;

	key.reqlen = reqlen;
	key.req = dnsreq;
	hit = RB_FIND(cachetree, &cachetree, &key);
	if (hit)
		cachehits += 1;

	dnsreq->id = origid;
	return hit;
}

static void
freerequest(struct request *req)
{
	struct dnscache *ent;

	if (req->tcp)
		conncount -= 2;
	else
		conncount -= 1;
	if (req->s != -1) {
		TAILQ_REMOVE(&reqfifo, req, fifo);
		close(req->s);
	}
	if (req->client != -1)
		close(req->client);
	if ((ent = req->cacheent) && !ent->resp) {
		free(ent->req);
		free(ent);
	}
	free(req);
}

static void
freecacheent(struct dnscache *ent)
{
	cachecount -= 1;
	RB_REMOVE(cachetree, &cachetree, ent);
	TAILQ_REMOVE(&cachefifo, ent, fifo);
	free(ent->req);
	free(ent->resp);
	free(ent);
}

static void
servfail(int ud, uint16_t id, struct sockaddr *fromaddr, socklen_t fromlen)
{
	struct dnspacket pkt;

	memset(&pkt, 0, sizeof(pkt));
	pkt.id = id;
	pkt.flags = htons(1 << 15 | 0x2);
	sendto(ud, &pkt, sizeof(pkt), 0, fromaddr, fromlen);
}

static struct request *
newrequest(int ud, struct sockaddr *remoteaddr)
{
	union sockthing from;
	socklen_t fromlen;
	struct request *req;
	uint8_t buf[65536];
	struct dnspacket *dnsreq;
	struct dnscache *hit;
	size_t r;

	dnsreq = (struct dnspacket *)buf;

	fromlen = sizeof(from);
	r = recvfrom(ud, buf, sizeof(buf), 0, &from.a, &fromlen);
	if (r == 0 || r == -1 || r < sizeof(struct dnspacket))
		return NULL;

	conntotal += 1;
	if ((hit = cachelookup(dnsreq, r))) {
		hit->resp->id = dnsreq->id;
		sendto(ud, hit->resp, hit->resplen, 0, &from.a, fromlen);
		return NULL;
	}

	if (!(req = calloc(1, sizeof(*req))))
		return NULL;

	conncount += 1;
	req->ts = now;
	req->ts.tv_sec += 30;
	req->s = -1;

	req->client = -1;
	memcpy(&req->from, &from, fromlen);
	req->fromlen = fromlen;

	req->clientid = dnsreq->id;
	req->reqid = randomid();
	dnsreq->id = req->reqid;

	hit = calloc(1, sizeof(*hit));
	if (hit) {
		hit->req = malloc(r);
		if (hit->req) {
			memcpy(hit->req, dnsreq, r);
			hit->reqlen = r;
			hit->req->id = 0;
		} else {
			free(hit);
			hit = NULL;

		}
	}
	req->cacheent = hit;

	req->s = socket(remoteaddr->sa_family, SOCK_DGRAM, 0);
	if (req->s == -1)
		goto fail;

	TAILQ_INSERT_TAIL(&reqfifo, req, fifo);

	if (connect(req->s, remoteaddr, remoteaddr->sa_len) == -1) {
		logmsg(LOG_NOTICE, "failed to connect (%d)", errno);
		if (errno == EADDRNOTAVAIL)
			servfail(ud, req->clientid, &from.a, fromlen);
		goto fail;
	}
	if (send(req->s, buf, r, 0) != r) {
		logmsg(LOG_NOTICE, "failed to send (%d)", errno);
		goto fail;
	}

	return req;

fail:
	freerequest(req);
	return NULL;
}

static uint32_t
minttl(struct dnspacket *resp, size_t rlen)
{
	uint32_t minttl = UINT_MAX, ttl, cnt, i;
	uint16_t len;
	char *p = (char *)resp;
	char *end = p + rlen;

	/* skip past packet header */
	p += sizeof(struct dnspacket);
	if (p >= end)
		return -1;
	if (ntohs(resp->qdcount) != 1)
		return -1;
	/* skip past query name, type, and class */
	p += strnlen(p, end - p);
	p += 2;
	p += 2;
	cnt = ntohs(resp->ancount);
	for (i = 0; i < cnt; i++) {
		if (p >= end)
			return -1;
		/* skip past answer name, type, and class */
		p += strnlen(p, end - p);
		p += 2;
		p += 2;
		if (p + 4 >= end)
			return -1;
		memcpy(&ttl, p, 4);
		p += 4;
		if (p + 2 >= end)
			return -1;
		ttl = ntohl(ttl);
		if (ttl < minttl)
			minttl = ttl;
		memcpy(&len, p, 2);
		p += 2;
		p += ntohs(len);
	}
	return minttl;
}



static void
sendreply(int ud, struct request *req)
{
	uint8_t buf[65536];
	struct dnspacket *resp;
	struct dnscache *ent;
	size_t r;
	uint32_t ttl;

	resp = (struct dnspacket *)buf;

	r = recv(req->s, buf, sizeof(buf), 0);
	if (r == 0 || r == -1 || r < sizeof(struct dnspacket))
		return;
	if (resp->id != req->reqid)
		return;
	resp->id = req->clientid;
	sendto(ud, buf, r, 0, &req->from.a, req->fromlen);
	if ((ent = req->cacheent)) {
		/*
		 * we do this first, because there's a potential race against
		 * other requests made at the same time. if we lose, abort.
		 * if anything else goes wrong, though, we need to reverse.
		 */
		if (RB_INSERT(cachetree, &cachetree, ent))
			return;
		ttl = minttl(resp, r);
		if (ttl == -1)
			ttl = 0;
		ent->ts = now;
		ent->ts.tv_sec += MINIMUM(ttl, 300);
		ent->resp = malloc(r);
		if (!ent->resp) {
			RB_REMOVE(cachetree, &cachetree, ent);
			return;
		}
		memcpy(ent->resp, buf, r);
		ent->resplen = r;
		cachecount += 1;
		TAILQ_INSERT_TAIL(&cachefifo, ent, fifo);
	}
}

static struct request *
tcpphasetwo(struct request *req)
{
	int error;
	socklen_t len = sizeof(error);

	req->tcp = 2;

	if (getsockopt(req->s, SOL_SOCKET, SO_ERROR, &error, &len) == -1 ||
	    error != 0)
		goto fail;
	if (setsockopt(req->client, SOL_SOCKET, SO_SPLICE, &req->s,
	    sizeof(req->s)) == -1)
		goto fail;
	if (setsockopt(req->s, SOL_SOCKET, SO_SPLICE, &req->client,
	    sizeof(req->client)) == -1)
		goto fail;

	return req;

fail:
	freerequest(req);
	return NULL;
}

static struct request *
newtcprequest(int ld, struct sockaddr *remoteaddr)
{
	struct request *req;
	int client;

	client = accept(ld, NULL, 0);
	if (client == -1) {
		if (errno == ENFILE || errno == EMFILE)
			stopaccepting = 1;
		return NULL;
	}

	if (!(req = calloc(1, sizeof(*req)))) {
		close(client);
		return NULL;
	}

	conntotal += 1;
	conncount += 2;
	req->ts = now;
	req->ts.tv_sec += 30;
	req->tcp = 1;
	req->client = client;

	req->s = socket(remoteaddr->sa_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (req->s == -1)
		goto fail;

	TAILQ_INSERT_TAIL(&reqfifo, req, fifo);

	if (connect(req->s, remoteaddr, remoteaddr->sa_len) == -1) {
		if (errno != EINPROGRESS)
			goto fail;
	} else {
		return tcpphasetwo(req);
	}

	return req;

fail:
	freerequest(req);
	return NULL;
}

static int
readconfig(FILE *conf, union sockthing *remoteaddr)
{
	char buf[1024];
	struct sockaddr_in *sin = &remoteaddr->i;
	struct sockaddr_in6 *sin6 = &remoteaddr->i6;

	if (fgets(buf, sizeof(buf), conf) == NULL)
		return -1;
	buf[strcspn(buf, "\n")] = '\0';

	memset(remoteaddr, 0, sizeof(*remoteaddr));
	if (inet_pton(AF_INET, buf, &sin->sin_addr) == 1) {
		sin->sin_len = sizeof(*sin);
		sin->sin_family = AF_INET;
		sin->sin_port = htons(53);
		return AF_INET;
	} else if (inet_pton(AF_INET6, buf, &sin6->sin6_addr) == 1) {
		sin6->sin6_len = sizeof(*sin6);
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = htons(53);
		return AF_INET6;
	} else {
		return -1;
	}
}

static int
launch(FILE *conf, int ud, int ld, int kq)
{
	union sockthing remoteaddr;
	struct kevent ch[2], kev[4];
	struct timespec ts, *timeout = NULL;
	struct request *req;
	struct dnscache *ent;
	struct passwd *pwd;
	int i, r, af;
	pid_t parent, child;

	parent = getpid();
	if (!debug) {
		if ((child = fork())) {
			fclose(conf);
			return child;
		}
		close(kq);
	}

	kq = kqueue();

	if (!(pwd = getpwnam("_rebound")))
		logerr("getpwnam failed");

	if (chroot(pwd->pw_dir) == -1)
		logerr("chroot failed (%d)", errno);
	if (chdir("/") == -1)
		logerr("chdir failed (%d)", errno);

	setproctitle("worker");
	if (setgroups(1, &pwd->pw_gid) ||
	    setresgid(pwd->pw_gid, pwd->pw_gid, pwd->pw_gid) ||
	    setresuid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid))
		logerr("failed to privdrop");

	/* would need pledge(proc) to do this below */
	EV_SET(&kev[0], parent, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
	if (kevent(kq, kev, 1, NULL, 0, NULL) == -1)
		logerr("kevent1: %d", errno);

	if (pledge("stdio inet", NULL) == -1)
		logerr("pledge failed");

	af = readconfig(conf, &remoteaddr);
	fclose(conf);
	if (af == -1)
		logerr("parse error in config file");

	EV_SET(&kev[0], ud, EVFILT_READ, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[1], ld, EVFILT_READ, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[2], SIGHUP, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	EV_SET(&kev[3], SIGUSR1, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	if (kevent(kq, kev, 4, NULL, 0, NULL) == -1)
		logerr("kevent4: %d", errno);
	logmsg(LOG_INFO, "worker process going to work");
	while (1) {
		r = kevent(kq, NULL, 0, kev, 4, timeout);
		if (r == -1)
			logerr("kevent failed (%d)", errno);

		clock_gettime(CLOCK_MONOTONIC, &now);

		if (stopaccepting) {
			EV_SET(&ch[0], ld, EVFILT_READ, EV_ADD, 0, 0, NULL);
			kevent(kq, ch, 1, NULL, 0, NULL);
			stopaccepting = 0;
		}

		for (i = 0; i < r; i++) {
			if (kev[i].filter == EVFILT_SIGNAL) {
				if (kev[i].ident == SIGHUP) {
					logmsg(LOG_INFO, "hupped, exiting");
					exit(0);
				} else {
					logmsg(LOG_INFO, "connection stats: "
					    "%d active, %llu total",
					    conncount, conntotal);
					logmsg(LOG_INFO, "cache stats: "
					    "%d active, %llu hits",
					    cachecount, cachehits);
				}
			} else if (kev[i].filter == EVFILT_PROC) {
				logmsg(LOG_INFO, "parent died");
				exit(0);
			} else if (kev[i].filter == EVFILT_WRITE) {
				req = kev[i].udata;
				req = tcpphasetwo(req);
				if (req) {
					EV_SET(&ch[0], req->s, EVFILT_WRITE,
					    EV_DELETE, 0, 0, NULL);
					EV_SET(&ch[1], req->s, EVFILT_READ,
					    EV_ADD, 0, 0, req);
					kevent(kq, ch, 2, NULL, 0, NULL);
				}
			} else if (kev[i].filter != EVFILT_READ) {
				logerr("don't know what happened");
			} else if (kev[i].ident == ud) {
				if ((req = newrequest(ud, &remoteaddr.a))) {
					EV_SET(&ch[0], req->s, EVFILT_READ,
					    EV_ADD, 0, 0, req);
					kevent(kq, ch, 1, NULL, 0, NULL);
				}
			} else if (kev[i].ident == ld) {
				if ((req = newtcprequest(ld, &remoteaddr.a))) {
					EV_SET(&ch[0], req->s,
					    req->tcp == 1 ? EVFILT_WRITE :
					    EVFILT_READ, EV_ADD, 0, 0, req);
					kevent(kq, ch, 1, NULL, 0, NULL);
				}
			} else {
				req = kev[i].udata;
				if (req->tcp == 0)
					sendreply(ud, req);
				freerequest(req);
			}
		}

		timeout = NULL;

		if (stopaccepting) {
			EV_SET(&ch[0], ld, EVFILT_READ, EV_DELETE, 0, 0, NULL);
			kevent(kq, ch, 1, NULL, 0, NULL);
			memset(&ts, 0, sizeof(ts));
			/* one second added below */
			timeout = &ts;
		}

		while (conncount > connmax)
			freerequest(TAILQ_FIRST(&reqfifo));
		while (cachecount > cachemax)
			freecacheent(TAILQ_FIRST(&cachefifo));

		/* burn old cache entries */
		while ((ent = TAILQ_FIRST(&cachefifo))) {
			if (timespeccmp(&ent->ts, &now, <=))
				freecacheent(ent);
			else
				break;
		}
		if (ent) {
			timespecsub(&ent->ts, &now, &ts);
			timeout = &ts;
		}

		/* burn stalled requests */
		while ((req = TAILQ_FIRST(&reqfifo))) {
			if (timespeccmp(&req->ts, &now, <=))
				freerequest(req);
			else
				break;
		}
		if (req && (!ent || timespeccmp(&req->ts, &ent->ts, <=))) {
			timespecsub(&req->ts, &now, &ts);
			timeout = &ts;
		}
		/* one second grace to avoid spinning */
		if (timeout)
			timeout->tv_sec += 1;

	}
	/* not reached */
	exit(1);
}

static void __dead
usage(void)
{
	fprintf(stderr, "usage: rebound [-d] [-c config]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	union sockthing bindaddr;
	int r, kq, ld, ud, ch;
	int one;
	int childdead, hupped;
	pid_t child;
	struct kevent kev;
	struct rlimit rlim;
	struct timespec ts, *timeout = NULL;
	const char *confname = "/etc/rebound.conf";
	FILE *conf;

	while ((ch = getopt(argc, argv, "c:d")) != -1) {
		switch (ch) {
		case 'c':
			confname = optarg;
			break;
		case 'd':
			debug = 1;
			break;
		default:
			usage();
			break;
		}
	}
	argv += optind;
	argc -= optind;

	if (argc)
		usage();

	if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
		logerr("getrlimit: %s", strerror(errno));
	rlim.rlim_cur = rlim.rlim_max;
	if (setrlimit(RLIMIT_NOFILE, &rlim) == -1)
		logerr("setrlimit: %s", strerror(errno));
	connmax = rlim.rlim_cur - 10;
	if (connmax > 512)
		connmax = 512;

	cachemax = 10000; /* something big, but not huge */

	tzset();
	openlog("rebound", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	TAILQ_INIT(&reqfifo);
	TAILQ_INIT(&cachefifo);
	RB_INIT(&cachetree);

	memset(&bindaddr, 0, sizeof(bindaddr));
	bindaddr.i.sin_len = sizeof(bindaddr.i);
	bindaddr.i.sin_family = AF_INET;
	bindaddr.i.sin_port = htons(53);
	inet_aton("127.0.0.1", &bindaddr.i.sin_addr);

	ud = socket(AF_INET, SOCK_DGRAM, 0);
	if (ud == -1)
		logerr("socket: %s", strerror(errno));
	if (bind(ud, &bindaddr.a, bindaddr.a.sa_len) == -1)
		logerr("bind: %s", strerror(errno));

	ld = socket(AF_INET, SOCK_STREAM, 0);
	if (ld == -1)
		logerr("socket: %s", strerror(errno));
	one = 1;
	setsockopt(ld, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
	if (bind(ld, &bindaddr.a, bindaddr.a.sa_len) == -1)
		logerr("bind: %s", strerror(errno));
	if (listen(ld, 10) == -1)
		logerr("listen: %s", strerror(errno));

	conf = fopen(confname, "r");
	if (!conf)
		logerr("failed to open config %s", confname);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	if (debug)
		return launch(conf, ud, ld, -1);

	if (daemon(0, 0) == -1)
		logerr("daemon: %s", strerror(errno));
	daemonized = 1;

	kq = kqueue();

	EV_SET(&kev, SIGHUP, EVFILT_SIGNAL, EV_ADD, 0, 0, NULL);
	kevent(kq, &kev, 1, NULL, 0, NULL);
	while (1) {
		hupped = 0;
		childdead = 0;
		child = launch(conf, ud, ld, kq);
		if (child == -1)
			logerr("failed to launch");

		/* monitor child */
		EV_SET(&kev, child, EVFILT_PROC, EV_ADD, NOTE_EXIT, 0, NULL);
		kevent(kq, &kev, 1, NULL, 0, NULL);

		/* wait for something to happen: HUP or child exiting */
		timeout = NULL;
		while (1) {
			r = kevent(kq, NULL, 0, &kev, 1, timeout);
			if (r == -1)
				logerr("kevent failed (%d)", errno);

			if (r == 0) {
				/* timeout expired */
				logerr("child died without HUP");
			} else if (kev.filter == EVFILT_SIGNAL) {
				/* signaled. kill child. */
				logmsg(LOG_INFO, "received HUP, restarting");
				hupped = 1;
				if (childdead)
					break;
				kill(child, SIGHUP);
				conf = fopen(confname, "r");
				if (!conf)
					logerr("failed to open config %s",
					    confname);
			} else if (kev.filter == EVFILT_PROC) {
				/* child died. wait for our own HUP. */
				logmsg(LOG_INFO, "observed child exit");
				childdead = 1;
				if (hupped)
					break;
				memset(&ts, 0, sizeof(ts));
				ts.tv_sec = 1;
				timeout = &ts;
			} else {
				logerr("don't know what happened");
			}
		}
		wait(NULL);
	}
	return 1;
}
