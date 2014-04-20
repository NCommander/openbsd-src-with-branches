/*	$OpenBSD: iscsictl.c,v 1.4 2012/05/02 18:02:45 gsoares Exp $ */

/*
 * Copyright (c) 2010 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <event.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "iscsid.h"
#include "iscsictl.h"

__dead void	 usage(void);
void		 run_command(int, struct pdu *);
struct pdu	*ctl_getpdu(char *, size_t);
int		 ctl_sendpdu(int, struct pdu *);
void		 show_vscsi_stats(struct ctrlmsghdr *, struct pdu *);

char		cbuf[CONTROL_READ_SIZE];

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,"usage: %s [-s socket] command [argument ...]\n",
	    __progname);
	exit(1);
}

int
main (int argc, char* argv[])
{
	struct sockaddr_un sun;
	struct parse_result *res;
	char *confname = ISCSID_CONFIG;
	char *sockname = ISCSID_CONTROL;
	struct pdu *pdu;
	struct ctrlmsghdr *cmh;
	struct session_config *sc;
	struct initiator_config *ic;
	struct session_ctlcfg *s;
	struct iscsi_config *cf;
	char *tname, *iname;
	int ch, csock;
	int *vp, val = 0;

	/* check flags */
	while ((ch = getopt(argc, argv, "f:s:")) != -1) {
		switch (ch) {
		case 'f':
			confname = optarg;
			break;
		case 's':
			sockname = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/* parse options */
	if ((res = parse(argc, argv)) == NULL)
		exit(1);

	/* connect to iscsid control socket */
	if ((csock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, sockname, sizeof(sun.sun_path));

	if (connect(csock, (struct sockaddr *)&sun, sizeof(sun)) == -1)
		err(1, "connect: %s", sockname);

	switch (res->action) {
	case NONE:
	case LOG_VERBOSE:
		val = 1;
		/* FALLTHROUGH */
	case LOG_BRIEF:
		if ((pdu = pdu_new()) == NULL)
			err(1, "pdu_new");
		if ((cmh = pdu_alloc(sizeof(*cmh))) == NULL)
			err(1, "pdu_alloc");
		bzero(cmh, sizeof(*cmh));
		cmh->type = CTRL_LOG_VERBOSE;
		cmh->len[0] = sizeof(int);
		if ((vp = pdu_dup(&val, sizeof(int))) == NULL)
			err(1, "pdu_dup");
		pdu_addbuf(pdu, cmh, sizeof(*cmh), 0);
		pdu_addbuf(pdu, vp, sizeof(int), 1);
		run_command(csock, pdu);
		break;
	case SHOW:
	case SHOW_SUM:
		usage();
		/* NOTREACHED */
	case SHOW_VSCSI_STATS:
		if ((pdu = pdu_new()) == NULL)
			err(1, "pdu_new");
		if ((cmh = pdu_alloc(sizeof(*cmh))) == NULL)
			err(1, "pdu_alloc");
		bzero(cmh, sizeof(*cmh));
		cmh->type = CTRL_VSCSI_STATS;
		pdu_addbuf(pdu, cmh, sizeof(*cmh), 0);
		run_command(csock, pdu);
		break;
	case RELOAD:
		if ((cf = parse_config(confname)) == NULL)
			errx(1, "errors while loading configuration file.");
		if (cf->initiator.isid_base != 0) {
			if ((pdu = pdu_new()) == NULL)
				err(1, "pdu_new");
			if ((cmh = pdu_alloc(sizeof(*cmh))) == NULL)
				err(1, "pdu_alloc");
			bzero(cmh, sizeof(*cmh));
			cmh->type = CTRL_INITIATOR_CONFIG;
			cmh->len[0] = sizeof(*ic);
			if ((ic = pdu_dup(&cf->initiator,
			    sizeof(cf->initiator))) == NULL)
				err(1, "pdu_dup");
			pdu_addbuf(pdu, cmh, sizeof(*cmh), 0);
			pdu_addbuf(pdu, ic, sizeof(*ic), 1);
			run_command(csock, pdu);
		}
		SIMPLEQ_FOREACH(s, &cf->sessions, entry) {
			if ((pdu = pdu_new()) == NULL)
				err(1, "pdu_new");
			if ((cmh = pdu_alloc(sizeof(*cmh))) == NULL)
				err(1, "pdu_alloc");
			bzero(cmh, sizeof(*cmh));
			cmh->type = CTRL_SESSION_CONFIG;
			cmh->len[0] = sizeof(*sc);
			if ((sc = pdu_dup(&s->session, sizeof(s->session))) ==
			    NULL)
				err(1, "pdu_dup");
			if (s->session.TargetName) {
				if ((tname = pdu_dup(s->session.TargetName,
				    strlen(s->session.TargetName) + 1)) ==
				    NULL)
					err(1, "pdu_dup");
				cmh->len[1] = strlen(s->session.TargetName) + 1;
			} else
				tname = NULL;
			if (s->session.InitiatorName) {
				if ((iname = pdu_dup(s->session.InitiatorName,
				    strlen(s->session.InitiatorName) + 1)) ==
				    NULL)
					err(1, "pdu_dup");
				cmh->len[2] = strlen(s->session.InitiatorName)
				    + 1;
			} else
				iname = NULL;
			pdu_addbuf(pdu, cmh, sizeof(*cmh), 0);
			pdu_addbuf(pdu, sc, sizeof(*sc), 1);
			if (tname)
				pdu_addbuf(pdu, tname, strlen(tname) + 1, 2);
			if (iname)
				pdu_addbuf(pdu, iname, strlen(iname) + 1, 3);

			run_command(csock, pdu);
		}
		break;
	case DISCOVERY:
		printf("discover %s\n", log_sockaddr(&res->addr));
		if ((pdu = pdu_new()) == NULL)
			err(1, "pdu_new");
		if ((cmh = pdu_alloc(sizeof(*cmh))) == NULL)
			err(1, "pdu_alloc");
		if ((sc = pdu_alloc(sizeof(*sc))) == NULL)
			err(1, "pdu_alloc");
		bzero(cmh, sizeof(*cmh));
		bzero(sc, sizeof(*sc));
		snprintf(sc->SessionName, sizeof(sc->SessionName),
		    "discovery.%d", (int)getpid());
		bcopy(&res->addr, &sc->connection.TargetAddr, res->addr.ss_len);
		sc->SessionType = SESSION_TYPE_DISCOVERY;
		cmh->type = CTRL_SESSION_CONFIG;
		cmh->len[0] = sizeof(*sc);
		pdu_addbuf(pdu, cmh, sizeof(*cmh), 0);
		pdu_addbuf(pdu, sc, sizeof(*sc), 1);

		run_command(csock, pdu);
	}

	close(csock);

	return (0);
}

void
run_command(int csock, struct pdu *pdu)
{
	struct ctrlmsghdr *cmh;
	int done = 0;
	ssize_t n;

	if (ctl_sendpdu(csock, pdu) == -1)
		err(1, "send");
	while (!done) {
		if ((n = recv(csock, cbuf, sizeof(cbuf), 0)) == -1 &&
		    !(errno == EAGAIN || errno == EINTR))
			err(1, "recv");

		if (n == 0)
			errx(1, "connection to iscsid closed");

		pdu = ctl_getpdu(cbuf, n);
		cmh = pdu_getbuf(pdu, NULL, 0);
		if (cmh == NULL)
			break;
		switch (cmh->type) {
		case CTRL_SUCCESS:
			printf("command successful\n");
			done = 1;
			break;
		case CTRL_FAILURE:
			printf("command failed\n");
			done = 1;
			break;
		case CTRL_INPROGRESS:
			printf("command in progress...\n");
			break;
		case CTRL_VSCSI_STATS:
			show_vscsi_stats(cmh, pdu);
			done = 1;
			break;
		}
	}
}

struct pdu *
ctl_getpdu(char *buf, size_t len)
{
	struct pdu *p;
	struct ctrlmsghdr *cmh;
	void *data;
	size_t n;
	int i;

	if (len < sizeof(*cmh))
		return NULL;

	if (!(p = pdu_new()))
		return NULL;

	n = sizeof(*cmh);
	cmh = pdu_alloc(n);
	bcopy(buf, cmh, n);
	buf += n;
	len -= n;

	if (pdu_addbuf(p, cmh, n, 0)) {
		free(cmh);
fail:
		pdu_free(p);
		return NULL;
	}

	for (i = 0; i < 3; i++) {
		n = cmh->len[i];
		if (n == 0)
			continue;
		if (PDU_LEN(n) > len)
			goto fail;
		if (!(data = pdu_alloc(n)))
			goto fail;
		bcopy(buf, data, n);
		if (pdu_addbuf(p, data, n, i + 1)) {
			free(data);
			goto fail;
		}
		buf += PDU_LEN(n);
		len -= PDU_LEN(n);
	}

	return p;
}

int
ctl_sendpdu(int fd, struct pdu *pdu)
{
	struct iovec iov[PDU_MAXIOV];
	struct msghdr msg;
	unsigned int niov = 0;

	for (niov = 0; niov < PDU_MAXIOV; niov++) {
		iov[niov].iov_base = pdu->iov[niov].iov_base;
		iov[niov].iov_len = pdu->iov[niov].iov_len;
	}
	bzero(&msg, sizeof(msg));
	msg.msg_iov = iov;
	msg.msg_iovlen = niov;
	if (sendmsg(fd, &msg, 0) == -1)
		return -1;
	return 0;
}

void
show_vscsi_stats(struct ctrlmsghdr *cmh, struct pdu *pdu)
{
	struct vscsi_stats *vs;

	if (cmh->len[0] != sizeof(struct vscsi_stats))
		errx(1, "bad size of response");
	vs = pdu_getbuf(pdu, NULL, 1);
		if (vs == NULL)
			return;

	printf("VSCSI ioctl statistics:\n");
	printf("%u probe calls and %u detach calls\n",
	    vs->cnt_probe, vs->cnt_detach);
	printf("%llu I2T calls (%llu read, %llu writes)\n",
	    vs->cnt_i2t,
	    vs->cnt_i2t_dir[1], 
	    vs->cnt_i2t_dir[2]);

	printf("%llu data reads (%llu bytes read)\n",
	    vs->cnt_read, vs->bytes_rd);
	printf("%llu data writes (%llu bytes written)\n",
	    vs->cnt_write, vs->bytes_wr);

	printf("%llu T2I calls (%llu done, %llu sense errors, %llu errors)\n",
	    vs->cnt_t2i,
	    vs->cnt_t2i_status[0], 
	    vs->cnt_t2i_status[1], 
	    vs->cnt_t2i_status[2]);
}
