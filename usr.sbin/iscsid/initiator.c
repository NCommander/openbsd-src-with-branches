/*	$OpenBSD: initiator.c,v 1.3 2010/09/25 16:20:06 sobrado Exp $ */

/*
 * Copyright (c) 2009 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/uio.h>

#include <scsi/iscsi.h>

#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "iscsid.h"
#include "log.h"

struct initiator *initiator;

struct kvp	*initiator_login_kvp(struct session *);
char		*default_initiator_name(void);

struct initiator *
initiator_init(void)
{
	if (!(initiator = calloc(1, sizeof(*initiator))))
		fatal("initiator_init");

	initiator->config.isid_base =
	    arc4random_uniform(0xffffff) | ISCSI_ISID_RAND;
	initiator->config.isid_qual = arc4random_uniform(0xffff);
	TAILQ_INIT(&initiator->sessions);
	return (initiator);
}

void
initiator_cleanup(struct initiator *i)
{
	struct session *s;

	while ((s = TAILQ_FIRST(&i->sessions)) != NULL) {
		TAILQ_REMOVE(&i->sessions, s, entry);
		session_close(s);
	}
	free(initiator);
}

struct session *
initiator_t2s(u_int target)
{
	struct session *s;

	TAILQ_FOREACH(s, &initiator->sessions, entry) {
		if (s->target == target)
			return s;
	}
	return NULL;
}

struct session *
session_find(struct initiator *i, char *name)
{
	struct session *s;

	TAILQ_FOREACH(s, &initiator->sessions, entry) {
		if (strcmp(s->config.SessionName, name) == 0)
			return s;
	}
	return NULL;
}

struct session *
session_new(struct initiator *i, u_int8_t st)
{
	struct session *s;

	if (!(s = calloc(1, sizeof(*s))))
		return NULL;

	/* use the same qualifier unless there is a conflict */
	s->isid_base = i->config.isid_base;
	s->isid_qual = i->config.isid_qual;
	s->cmdseqnum = arc4random();
	s->itt = arc4random();
	s->initiator = i;
	s->state = SESS_FREE;

	if (st == SESSION_TYPE_DISCOVERY)
		s->target = 0;
	else
		s->target = s->initiator->target++;

	TAILQ_INSERT_HEAD(&i->sessions, s, entry);
	TAILQ_INIT(&s->connections);
	TAILQ_INIT(&s->tasks);

	return s;
}

void
session_close(struct session *s)
{
	struct connection *c;

	while ((c = TAILQ_FIRST(&s->connections)) != NULL)
		conn_free(c);

	free(s->config.TargetName);
	free(s->config.InitiatorName);
	free(s);
}

void
session_config(struct session *s, struct session_config *sc)
{
	if (s->config.TargetName)
		free(s->config.TargetName);
	s->config.TargetName = NULL;
	if (s->config.InitiatorName)
		free(s->config.InitiatorName);
	s->config.InitiatorName = NULL;

	s->config = *sc;

	if (sc->TargetName) {
		s->config.TargetName = strdup(sc->TargetName);
		if (s->config.TargetName == NULL)
			fatal("strdup");
	}
	if (sc->InitiatorName) {
		s->config.InitiatorName = strdup(sc->InitiatorName);
		if (s->config.InitiatorName == NULL)
			fatal("strdup");
	} else
		s->config.InitiatorName = default_initiator_name();
}

void
session_task_issue(struct session *s, struct task *t)
{
	TAILQ_INSERT_TAIL(&s->tasks, t, entry);
	session_schedule(s);
}

void
session_schedule(struct session *s)
{
	struct task *t = TAILQ_FIRST(&s->tasks);
	struct connection *c;

	if (!t)
		return;

	/* XXX IMMEDIATE TASK NEED SPECIAL HANDLING !!!! */

	/* wake up a idle connection or a not busy one */
	/* XXX this needs more work as it makes the daemon go wrooOOOMM */
	TAILQ_REMOVE(&s->tasks, t, entry);
	TAILQ_FOREACH(c, &s->connections, entry)
		if (conn_task_issue(c, t))
			return;
	/* all connections are busy readd task to the head */
	TAILQ_INSERT_HEAD(&s->tasks, t, entry);
}

struct task_login {
	struct task		 task;
	struct connection	*c;
	u_int16_t		 tsih;
	u_int8_t		 stage;
};

struct pdu *initiator_login_build(struct task_login *, struct kvp *);
void	initiator_login_cb(struct connection *, void *, struct pdu *);

void	initiator_discovery_cb(struct connection *, void *, struct pdu *);
struct pdu *initiator_text_build(struct task *, struct session *, struct kvp *);

struct kvp *
initiator_login_kvp(struct session *s)
{
	struct kvp *kvp;

	if (!(kvp = calloc(4, sizeof(*kvp))))
		return NULL;
	kvp[0].key = "AuthMethod";
	kvp[0].value = "None";
	kvp[1].key = "InitiatorName";
	kvp[1].value = s->config.InitiatorName;

	if (s->config.SessionType == SESSION_TYPE_DISCOVERY) {
		kvp[2].key = "SessionType";
		kvp[2].value = "Discovery";
	} else {
		kvp[2].key = "TargetName";
		kvp[2].value = s->config.TargetName;
	}

	return kvp;
}

void
initiator_login(struct connection *c)
{
	struct task_login *tl;
	struct pdu *p;
	struct kvp *kvp;

	if (!(tl = calloc(1, sizeof(*tl)))) {
		log_warn("initiator_login");
		conn_fail(c);
		return;
	}
	tl->c = c;
	tl->stage = ISCSI_LOGIN_STG_SECNEG;

	if (!(kvp = initiator_login_kvp(c->session))) {
		log_warnx("initiator_login_kvp failed");
		free(tl);
		conn_fail(c);
		return;
	}

	if (!(p = initiator_login_build(tl, kvp))) {
		log_warnx("initiator_login_build failed");
		free(tl);
		conn_fail(c);
		return;
	}

	free(kvp);

	task_init(&tl->task, c->session, 1, tl, initiator_login_cb);
	task_pdu_add(&tl->task, p);
	/* XXX this is wrong, login needs to run on a specific connection */
	session_task_issue(c->session, &tl->task);
}

struct pdu *
initiator_login_build(struct task_login *tl, struct kvp *kvp)
{
	struct pdu *p;
	struct iscsi_pdu_login_request *lreq;
	int n;

	if (!(p = pdu_new()))
		return NULL;
	if (!(lreq = pdu_gethdr(p)))
		return NULL;

	lreq->opcode = ISCSI_OP_LOGIN_REQUEST | ISCSI_OP_F_IMMEDIATE;
	if (tl->stage == ISCSI_LOGIN_STG_SECNEG)
		lreq->flags = ISCSI_LOGIN_F_T |
		    ISCSI_LOGIN_F_CSG(ISCSI_LOGIN_STG_OPNEG) |
		    ISCSI_LOGIN_F_NSG(ISCSI_LOGIN_STG_FULL);
	else if (tl->stage == ISCSI_LOGIN_STG_OPNEG)
		lreq->flags = ISCSI_LOGIN_F_T |
		    ISCSI_LOGIN_F_CSG(ISCSI_LOGIN_STG_OPNEG) |
		    ISCSI_LOGIN_F_NSG(ISCSI_LOGIN_STG_FULL);

	lreq->isid_base = htonl(tl->c->session->isid_base);
	lreq->isid_qual = htons(tl->c->session->isid_qual);
	lreq->tsih = tl->tsih;
	lreq->cid = htons(tl->c->cid);
	lreq->expstatsn = htonl(tl->c->expstatsn);

	if ((n = text_to_pdu(kvp, p)) == -1)
		return NULL;
	n = htonl(n);
	bcopy(&n, &lreq->ahslen, sizeof(n));

	return p;
}

void
initiator_login_cb(struct connection *c, void *arg, struct pdu *p)
{
	struct task_login *tl = arg;
	struct iscsi_pdu_login_response *lresp;

	lresp = pdu_getbuf(p, NULL, PDU_HEADER);
	/* XXX handle packet would be great */
	log_pdu(p, 1);
	if (ISCSI_PDU_OPCODE(lresp->opcode) != ISCSI_OP_LOGIN_RESPONSE) {
		log_debug("Unknown crap");
	}

	task_cleanup(&tl->task, c);
	conn_loggedin(c);
	free(tl);
	pdu_free(p);
}

void
initiator_discovery(struct session *s)
{
	struct task *t;
	struct pdu *p;
	struct kvp kvp[] = {
		{ "SendTargets", "All" },
		{ NULL, NULL }
	};

	if (!(t = calloc(1, sizeof(*t)))) {
		log_warn("initiator_discovery");
		return;
	}

	if (!(p = initiator_text_build(t, s, kvp))) {
		log_warnx("initiator_text_build failed");
		free(t);
		return;
	}

	task_init(t, s, 0, t, initiator_discovery_cb);
	task_pdu_add(t, p);
	session_task_issue(s, t);
}

struct pdu *
initiator_text_build(struct task *t, struct session *s, struct kvp *kvp)
{
	struct pdu *p;
	struct iscsi_pdu_text_request *lreq;
	int n;

	if (!(p = pdu_new()))
		return NULL;
	if (!(lreq = pdu_gethdr(p)))
		return NULL;

	lreq->opcode = ISCSI_OP_TEXT_REQUEST;
	lreq->flags = ISCSI_TEXT_F_F;
	lreq->ttt = 0xffffffff;

	if ((n = text_to_pdu(kvp, p)) == -1)
		return NULL;
	n = htonl(n);
	bcopy(&n, &lreq->ahslen, sizeof(n));

	return p;
}

void
initiator_discovery_cb(struct connection *c, void *arg, struct pdu *p)
{
	struct iscsi_pdu_text_response *lresp;
	u_char *buf = NULL;
	struct kvp *kvp, *k;
	size_t n, size;

	lresp = pdu_getbuf(p, NULL, PDU_HEADER);
	switch (ISCSI_PDU_OPCODE(lresp->opcode)) {
	case ISCSI_OP_TEXT_RESPONSE:
		buf = pdu_getbuf(p, &n, PDU_DATA);
		if (buf == NULL)
			goto fail;
		size = lresp->datalen[0] << 16 | lresp->datalen[1] << 8 |
		    lresp->datalen[2];
		if (size > n)
			goto fail;
		kvp = pdu_to_text(buf, size);
		if (kvp == NULL)
			goto fail;
		log_debug("ISCSI_OP_TEXT_RESPONSE");
		for (k = kvp; k->key; k++) {
			log_debug("%s\t=>\t%s", k->key, k->value);
		}
		free(kvp);
		free(arg);
		conn_close(c);
		break;
	default:
fail:
		conn_fail(c);
	}
	pdu_free(p);
}

void
initiator_nop_in_imm(struct connection *c, struct pdu *p)
{
	struct iscsi_pdu_nop_in *nopin;
	struct task *t;

	/* fixup NOP-IN to make it a NOP-OUT */
	nopin = pdu_getbuf(p, NULL, PDU_HEADER);
	nopin->maxcmdsn = 0;
	nopin->opcode = ISCSI_OP_I_NOP | ISCSI_OP_F_IMMEDIATE;

	/* and schedule an immediate task */
	if (!(t = calloc(1, sizeof(*t)))) {
		log_warn("initiator_nop_in_imm");
		pdu_free(p);
		return;
	}

	task_init(t, c->session, 1, NULL, NULL);
	t->itt = 0xffffffff; /* change ITT because it is just a ping reply */
	task_pdu_add(t, p);
	conn_task_issue(c, t);
}

char *
default_initiator_name(void)
{
	char *s, hostname[MAXHOSTNAMELEN];

	if (gethostname(hostname, sizeof(hostname)))
		strlcpy(hostname, "initiator", sizeof(hostname));
	if ((s = strchr(hostname, '.')))
		*s = '\0';
	if (asprintf(&s, "%s:%s", ISCSID_BASE_NAME, hostname) == -1)
		return ISCSID_BASE_NAME ":initiator";
	return s;
}
