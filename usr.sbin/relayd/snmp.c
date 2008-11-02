/*	$OpenBSD: snmp.c,v 1.1 2008/02/11 10:42:50 reyk Exp $	*/

/*
 * Copyright (c) 2008 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/un.h>
#include <sys/uio.h>

#include <net/if.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <event.h>

#include <openssl/ssl.h>

#include "relayd.h"
#include "snmp.h"

#define RELAYD_MIB	"1.3.6.1.4.1.30155.3"
#define SNMP_ELEMENT(x...)	do {				\
	if (snmp_element(RELAYD_MIB x) == -1)			\
		goto done;					\
} while (0)

static struct imsgbuf	*ibuf_snmp = NULL;
static struct imsgbuf	*ibuf_main = NULL;
static struct relayd	*env = NULL;

void	 snmp_sock(int, short, void *);
int	 snmp_getsock(struct imsgbuf *);
int	 snmp_element(const char *, enum snmp_type, void *, int64_t);

void
snmp_init(struct relayd *x_env, struct imsgbuf *ibuf)
{
	env = x_env;
	ibuf_main = ibuf;

	if (event_initialized(&env->sc_snmpev))
		event_del(&env->sc_snmpev);
	if (event_initialized(&env->sc_snmpto))
		event_del(&env->sc_snmpto);
	if (env->sc_snmp != -1) {
		close(env->sc_snmp);
		env->sc_snmp = -1;
	}

	if ((env->sc_flags & F_TRAP) == 0) {
		ibuf_main = NULL;
		return;
	}

	snmp_sock(-1, -1, ibuf);
}

int
snmp_sendsock(struct imsgbuf *ibuf)
{
	struct imsgbuf		 tmpibuf;
	struct sockaddr_un	 sun;
	int			 s = -1;

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		goto fail;

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, SNMP_SOCKET, sizeof(sun.sun_path));
	if (connect(s, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		goto fail;

	/* enable restricted snmp socket mode */
	bzero(&tmpibuf, sizeof(tmpibuf));
	imsg_init(&tmpibuf, s, NULL);
	imsg_compose(&tmpibuf, IMSG_SNMP_LOCK, 0, 0, -1, NULL, 0);

	imsg_compose(ibuf, IMSG_SNMPSOCK, 0, 0, s, NULL, 0);
	imsg_flush(ibuf);	/* need to send the socket now */
	close(s);
	return (0);

 fail:
	if (s != -1)
		close(s);
	imsg_compose(ibuf, IMSG_NONE, 0, 0, -1, NULL, 0);
	return (-1);
}

int
snmp_getsock(struct imsgbuf *ibuf)
{
	struct imsg	 imsg;
	int		 n, s = -1, done = 0;

	imsg_compose(ibuf, IMSG_SNMPSOCK, 0, 0, -1, NULL, 0);
	imsg_flush(ibuf);

	while (!done) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("snmp_getsock: failed to read imsg");
		if (n == 0)
			fatal("snmp_getsock: pipe closed");
		while (!done) {
			if ((n = imsg_get(ibuf, &imsg)) == -1)
				fatal("snmp_getsock: failed to get imsg");
			if (n == 0)
				break;
			done = 1;
			switch (imsg.hdr.type) {
			case IMSG_SNMPSOCK:
				if ((s = imsg_get_fd(ibuf)) <= 0)
					s = -1;
				break;
			default:
				break;
			}
			imsg_free(&imsg);
		}
	}

	if (s != -1) {
		log_debug("snmp_getsock: got new snmp socket %d", s);
		if (ibuf_snmp == NULL && (ibuf_snmp = (struct imsgbuf *)
		    calloc(1, sizeof(struct imsgbuf))) == NULL)
			fatal("snmp_getsock: calloc");
		imsg_init(ibuf_snmp, s, NULL);
	}

	return (s);
}

void
snmp_sock(int fd, short event, void *arg)
{
	struct timeval	tv = SNMP_RECONNECT_TIMEOUT;

	switch (event) {
	case -1:
		bzero(&tv, sizeof(tv));
		goto retry;
	case EV_READ:
		log_debug("snmp_sock: snmp socket closed %d", env->sc_snmp);
		(void)close(env->sc_snmp);
		break;
	}

	if ((env->sc_snmp = snmp_getsock(ibuf_main)) == -1) {
		DPRINTF("snmp_sock: failed to open snmp socket");
		goto retry;
	}

	event_set(&env->sc_snmpev, env->sc_snmp,
	    EV_READ|EV_TIMEOUT, snmp_sock, arg);
	event_add(&env->sc_snmpev, NULL);
	return;
 retry:
	evtimer_set(&env->sc_snmpto, snmp_sock, env);
	evtimer_add(&env->sc_snmpto, &tv);
}

int
snmp_element(const char *oid, enum snmp_type type, void *buf, int64_t val)
{
	struct iovec		 iov[2];
	int			 iovcnt = 2;
	u_int32_t		 d;
	u_int64_t		 l;
	struct snmp_imsg	 sm;

	DPRINTF("snmp_element: oid %s type %d buf %p val %lld",
	    oid, type, buf, val);

	bzero(&iov, sizeof(iov));

	switch (type) {
	case SNMP_COUNTER32:
	case SNMP_GAUGE32:
	case SNMP_TIMETICKS:
	case SNMP_OPAQUE:
	case SNMP_UINTEGER32:
	case SNMP_INTEGER32:
		d = (u_int32_t)val;
		iov[1].iov_base = &d;
		iov[1].iov_len = sizeof(d);
		break;
	case SNMP_COUNTER64:
		l = (u_int64_t)val;
		iov[1].iov_base = &l;
		iov[1].iov_len = sizeof(l);
		break;
	case SNMP_NSAPADDR:
	case SNMP_BITSTRING:
	case SNMP_OCTETSTRING:
	case SNMP_IPADDR:
	case SNMP_OBJECT:
		iov[1].iov_base = buf;
		if (val == 0)
			iov[1].iov_len = strlen((char *)buf);
		else
			iov[1].iov_len = val;
		break;
	case SNMP_NULL:
		iovcnt--;
		break;
	}

	bzero(&sm, sizeof(sm));
	if (strlcpy(sm.snmp_oid, oid, sizeof(sm.snmp_oid)) >=
	    sizeof(sm.snmp_oid))
		return (-1);
	sm.snmp_type = type;
	sm.snmp_len = iov[1].iov_len;
	iov[0].iov_base = &sm;
	iov[0].iov_len = sizeof(sm);

	if (imsg_composev(ibuf_snmp, IMSG_SNMP_ELEMENT, 0, 0, -1,
	    iov, iovcnt) == -1)
		return (-1);

	return (0);
}

/*
 * SNMP traps for relayd
 */

void
snmp_hosttrap(struct table *table, struct host *host)
{
	if (ibuf_snmp == NULL || env->sc_snmp == -1)
		return;

	/*
	 * OPENBSD-RELAYD-MIB host status trap
	 * XXX The trap format needs some tweaks and other OIDs
	 */

	imsg_compose(ibuf_snmp, IMSG_SNMP_TRAP, 0, 0, -1, NULL, 0);

	SNMP_ELEMENT(".1", SNMP_NULL, NULL, 0);
	SNMP_ELEMENT(".1.1", SNMP_OCTETSTRING, host->conf.name, 0);
	SNMP_ELEMENT(".1.2", SNMP_INTEGER32, NULL, host->up);
	SNMP_ELEMENT(".1.3", SNMP_INTEGER32, NULL, host->last_up);
	SNMP_ELEMENT(".1.4", SNMP_INTEGER32, NULL, host->up_cnt);
	SNMP_ELEMENT(".1.5", SNMP_INTEGER32, NULL, host->check_cnt);
	SNMP_ELEMENT(".1.6", SNMP_OCTETSTRING, table->conf.name, 0);
	SNMP_ELEMENT(".1.7", SNMP_INTEGER32, NULL, table->up);
	if (!host->conf.retry)
		goto done;
	SNMP_ELEMENT(".1.8", SNMP_INTEGER32, NULL, host->conf.retry);
	SNMP_ELEMENT(".1.9", SNMP_INTEGER32, NULL, host->retry_cnt);

 done:
	imsg_compose(ibuf_snmp, IMSG_SNMP_END, 0, 0, -1, NULL, 0);
}
