/*	$OpenBSD: application_agentx.c,v 1.1 2022/08/23 08:56:20 martijn Exp $ */
/*
 * Copyright (c) 2022 Martijn van Duren <martijn@openbsd.org>
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

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

#include <errno.h>
#include <event.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "application.h"
#include "ax.h"
#include "log.h"
#include "smi.h"
#include "snmp.h"
#include "snmpd.h"

#define AGENTX_DEFAULTTIMEOUT 5

struct appl_agentx_connection {
	uint32_t conn_id;
	struct ax *conn_ax;
	struct event conn_rev;
	struct event conn_wev;

	TAILQ_HEAD(, appl_agentx_session) conn_sessions;
	RB_ENTRY(appl_agentx_connection) conn_entry;
};

struct appl_agentx_session {
	uint32_t sess_id;
	struct appl_agentx_connection *sess_conn;
	/*
	 * RFC 2741 section 7.1.1:
	 * All subsequent AgentX protocol operations initiated by the master
	 * agent for this session must use this byte ordering and set this bit
	 * accordingly.
	 */
	enum ax_byte_order sess_byteorder;
	uint8_t sess_timeout;
	struct ax_oid sess_oid;
	struct ax_ostring sess_descr;
	struct appl_backend sess_backend;

	RB_ENTRY(appl_agentx_session) sess_entry;
	TAILQ_ENTRY(appl_agentx_session) sess_conn_entry;
};

void appl_agentx_listen(struct agentx_master *);
void appl_agentx_accept(int, short, void *);
void appl_agentx_free(struct appl_agentx_connection *);
void appl_agentx_recv(int, short, void *);
void appl_agentx_open(struct appl_agentx_connection *, struct ax_pdu *);
void appl_agentx_close(struct appl_agentx_session *, struct ax_pdu *);
void appl_agentx_forceclose(struct appl_backend *, enum appl_close_reason);
void appl_agentx_session_free(struct appl_agentx_session *);
void appl_agentx_register(struct appl_agentx_session *, struct ax_pdu *);
void appl_agentx_unregister(struct appl_agentx_session *, struct ax_pdu *);
void appl_agentx_get(struct appl_backend *, int32_t, int32_t, const char *,
    struct appl_varbind *);
void appl_agentx_getnext(struct appl_backend *, int32_t, int32_t, const char *,
    struct appl_varbind *);
void appl_agentx_response(struct appl_agentx_session *, struct ax_pdu *);
void appl_agentx_send(int, short, void *);
struct ber_oid *appl_agentx_oid2ber_oid(struct ax_oid *, struct ber_oid *);
struct ber_element *appl_agentx_value2ber_element(struct ax_varbind *);
struct ax_ostring *appl_agentx_string2ostring(const char *,
    struct ax_ostring *);
int appl_agentx_cmp(struct appl_agentx_connection *,
    struct appl_agentx_connection *);
int appl_agentx_session_cmp(struct appl_agentx_session *,
    struct appl_agentx_session *);

struct appl_backend_functions appl_agentx_functions = {
	.ab_close = appl_agentx_forceclose,
	.ab_get = appl_agentx_get,
	.ab_getnext = appl_agentx_getnext,
	.ab_getbulk = NULL, /* not properly supported in application.c and libagentx */
};

RB_HEAD(appl_agentx_conns, appl_agentx_connection) appl_agentx_conns =
    RB_INITIALIZER(&appl_agentx_conns);
RB_HEAD(appl_agentx_sessions, appl_agentx_session) appl_agentx_sessions =
    RB_INITIALIZER(&appl_agentx_sessions);

RB_PROTOTYPE_STATIC(appl_agentx_conns, appl_agentx_connection, conn_entry,
    appl_agentx_cmp);
RB_PROTOTYPE_STATIC(appl_agentx_sessions, appl_agentx_session, sess_entry,
    appl_agentx_session_cmp);

void
appl_agentx(void)
{
	struct agentx_master *master;

	TAILQ_FOREACH(master, &(snmpd_env->sc_agentx_masters), axm_entry)
		appl_agentx_listen(master);
}

void
appl_agentx_init(void)
{
	struct agentx_master *master;

	TAILQ_FOREACH(master, &(snmpd_env->sc_agentx_masters), axm_entry) {
		if (master->axm_fd == -1)
			continue;
		event_set(&(master->axm_ev), master->axm_fd,
		    EV_READ | EV_PERSIST, appl_agentx_accept, master);
		event_add(&(master->axm_ev), NULL);
		log_info("AgentX: listening on %s", master->axm_sun.sun_path);
	}
}
void
appl_agentx_listen(struct agentx_master *master)
{
	mode_t mask;

	unlink(master->axm_sun.sun_path);

	mask = umask(0777);
	if ((master->axm_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1 ||
	    bind(master->axm_fd, (struct sockaddr *)&(master->axm_sun),
	    sizeof(master->axm_sun)) == -1 ||
	    listen(master->axm_fd, 5)) {
		log_warn("AgentX: listen %s", master->axm_sun.sun_path);
		umask(mask);
		return;
	}
	umask(mask);
	if (chown(master->axm_sun.sun_path, master->axm_owner,
	    master->axm_group) == -1) {
		log_warn("AgentX: chown %s", master->axm_sun.sun_path);
		goto fail;
	}
	if (chmod(master->axm_sun.sun_path, master->axm_mode) == -1) {
		log_warn("AgentX: chmod %s", master->axm_sun.sun_path);
		goto fail;
	}
	return;
 fail:
	close(master->axm_fd);
	master->axm_fd = -1;
}

void
appl_agentx_shutdown(void)
{
	struct appl_agentx_connection *conn, *tconn;

	RB_FOREACH_SAFE(conn, appl_agentx_conns, &appl_agentx_conns, tconn)
		appl_agentx_free(conn);
}

void
appl_agentx_accept(int masterfd, short event, void *cookie)
{
	int fd;
	struct agentx_master *master = cookie;
	struct sockaddr_un sun;
	socklen_t sunlen = sizeof(sun);
	struct appl_agentx_connection *conn = NULL;

	if ((fd = accept(masterfd, (struct sockaddr *)&sun, &sunlen)) == -1) {
		log_warn("AgentX: accept %s", master->axm_sun.sun_path);
		return;
	}

	if ((conn = malloc(sizeof(*conn))) == NULL) {
		log_warn(NULL);
		goto fail;
	}

	TAILQ_INIT(&(conn->conn_sessions));
	if ((conn->conn_ax = ax_new(fd)) == NULL) {
		log_warn(NULL);
		goto fail;
	}

	do {
		conn->conn_id = arc4random();
	} while (RB_INSERT(appl_agentx_conns,
	    &appl_agentx_conns, conn) != NULL);

	event_set(&(conn->conn_rev), fd, EV_READ | EV_PERSIST,
	    appl_agentx_recv, conn);
	event_add(&(conn->conn_rev), NULL);
	event_set(&(conn->conn_wev), fd, EV_WRITE, appl_agentx_send, conn);
	log_info("AgentX(%"PRIu32"): new connection", conn->conn_id);

	return;
 fail:
	close(fd);
	free(conn);
}

void
appl_agentx_free(struct appl_agentx_connection *conn)
{
	struct appl_agentx_session *session;

	event_del(&(conn->conn_rev));
	event_del(&(conn->conn_wev));

	while ((session = TAILQ_FIRST(&(conn->conn_sessions))) != NULL) {
		if (conn->conn_ax == NULL)
			appl_agentx_session_free(session);
		else
			appl_agentx_forceclose(&(session->sess_backend),
			    APPL_CLOSE_REASONSHUTDOWN);
	}

	RB_REMOVE(appl_agentx_conns, &appl_agentx_conns, conn);
	ax_free(conn->conn_ax);
	free(conn);
}

void
appl_agentx_recv(int fd, short event, void *cookie)
{
	struct appl_agentx_connection *conn = cookie;
	struct appl_agentx_session *session;
	struct ax_pdu *pdu;

	if ((pdu = ax_recv(conn->conn_ax)) == NULL) {
		if (errno == EAGAIN)
			return;
		log_warn("AgentX(%"PRIu32")", conn->conn_id);
		/*
		 * Either the connection is dead, or we had garbage on the line.
		 * Both make sure we can't continue on this stream.
		 */
		if (errno == ECONNRESET) {
			ax_free(conn->conn_ax);
			conn->conn_ax = NULL;
		}
		appl_agentx_free(conn);
		return;
	}

	conn->conn_ax->ax_byteorder = pdu->ap_header.aph_flags &
	    AX_PDU_FLAG_NETWORK_BYTE_ORDER ?
	    AX_BYTE_ORDER_BE : AX_BYTE_ORDER_LE;
	if (pdu->ap_header.aph_type != AX_PDU_TYPE_OPEN) {
		/* Make sure we only look for connection-local sessions */
		TAILQ_FOREACH(session, &(conn->conn_sessions),
		    sess_conn_entry) {
			if (session->sess_id == pdu->ap_header.aph_sessionid)
				break;
		}
		if (session == NULL) {
			log_warnx("AgentX(%"PRIu32"): Session %"PRIu32" not "
			    "found for request", conn->conn_id,
			    pdu->ap_header.aph_sessionid);
			ax_response(conn->conn_ax, pdu->ap_header.aph_sessionid,
			    pdu->ap_header.aph_transactionid,
			    pdu->ap_header.aph_packetid,
			    &(pdu->ap_context), smi_getticks(),
			    APPL_ERROR_NOTOPEN, 0, NULL, 0);
			appl_agentx_send(-1, EV_WRITE, conn);
			goto fail;
		}
		/*
		 * RFC2741 section 7.1.1 bullet 4 is unclear on what byte order
		 * the response should be. My best guess is that it makes more
		 * sense that replies are in the same byte-order as what was
		 * requested.
		 * In practice we always have the same byte order as when we
		 * opened the session, so it's likely a non-issue, however, we
		 * can change to session byte order here.
		 */
	}

	switch (pdu->ap_header.aph_type) {
	case AX_PDU_TYPE_OPEN:
		appl_agentx_open(conn, pdu);
		break;
	case AX_PDU_TYPE_CLOSE:
		appl_agentx_close(session, pdu);
		break;
	case AX_PDU_TYPE_REGISTER:
		appl_agentx_register(session, pdu);
		break;
	case AX_PDU_TYPE_UNREGISTER:
		appl_agentx_unregister(session, pdu);
		break;
	case AX_PDU_TYPE_GET:
	case AX_PDU_TYPE_GETNEXT:
	case AX_PDU_TYPE_GETBULK:
	case AX_PDU_TYPE_TESTSET:
	case AX_PDU_TYPE_COMMITSET:
	case AX_PDU_TYPE_UNDOSET:
	case AX_PDU_TYPE_CLEANUPSET:
		appl_agentx_forceclose(&(session->sess_backend),
		    APPL_CLOSE_REASONPROTOCOLERROR);
		if (!TAILQ_EMPTY(&(conn->conn_sessions)))
			goto fail;

		ax_pdu_free(pdu);
		appl_agentx_free(conn);
		return;
	case AX_PDU_TYPE_NOTIFY:
		log_warnx("%s: not supported",
		    ax_pdutype2string(pdu->ap_header.aph_type));
		/*
		 * RFC 2741 section 7.1.10:
		 * Note that the master agent's successful response indicates
		 * the agentx-Notify-PDU was received and validated.  It does
		 * not indicate that any particular notifications were actually
		 * generated or received by notification targets
		 */
		/* XXX Not yet - FALLTHROUGH */
	case AX_PDU_TYPE_PING:
		ax_response(conn->conn_ax, pdu->ap_header.aph_sessionid,
		    pdu->ap_header.aph_transactionid,
		    pdu->ap_header.aph_packetid, &(pdu->ap_context),
		    smi_getticks(), APPL_ERROR_NOERROR, 0, NULL, 0);
		appl_agentx_send(-1, EV_WRITE, conn);
		break;
	case AX_PDU_TYPE_INDEXALLOCATE:
	case AX_PDU_TYPE_INDEXDEALLOCATE:
		log_warnx("%s: not supported",
		    ax_pdutype2string(pdu->ap_header.aph_type));
		ax_response(conn->conn_ax, pdu->ap_header.aph_sessionid,
		    pdu->ap_header.aph_transactionid,
		    pdu->ap_header.aph_packetid, &(pdu->ap_context),
		    smi_getticks(), APPL_ERROR_PROCESSINGERROR, 1,
		    pdu->ap_payload.ap_vbl.ap_varbind,
		    pdu->ap_payload.ap_vbl.ap_nvarbind);
		appl_agentx_send(-1, EV_WRITE, conn);
		break;
	case AX_PDU_TYPE_ADDAGENTCAPS:
	case AX_PDU_TYPE_REMOVEAGENTCAPS:
		log_warnx("%s: not supported",
		    ax_pdutype2string(pdu->ap_header.aph_type));
		ax_response(conn->conn_ax, pdu->ap_header.aph_sessionid,
		    pdu->ap_header.aph_transactionid,
		    pdu->ap_header.aph_packetid, &(pdu->ap_context),
		    smi_getticks(), APPL_ERROR_PROCESSINGERROR, 1,
		    NULL, 0);
		appl_agentx_send(-1, EV_WRITE, conn);
		break;
	case AX_PDU_TYPE_RESPONSE:
		appl_agentx_response(session, pdu);
		break;
	}

 fail:
	ax_pdu_free(pdu);
}

void
appl_agentx_open(struct appl_agentx_connection *conn, struct ax_pdu *pdu)
{
	struct appl_agentx_session *session;
	struct ber_oid oid;
	char oidbuf[1024];

	if ((session = malloc(sizeof(*session))) == NULL) {
		log_warn(NULL);
		goto fail;
	}
	session->sess_descr.aos_string = NULL;

	session->sess_conn = conn;
	if (pdu->ap_header.aph_flags & AX_PDU_FLAG_NETWORK_BYTE_ORDER)
		session->sess_byteorder = AX_BYTE_ORDER_BE;
	else
		session->sess_byteorder = AX_BYTE_ORDER_LE;

	/* RFC 2742 agentxSessionObjectID */
	if (pdu->ap_payload.ap_open.ap_oid.aoi_idlen == 0) {
		pdu->ap_payload.ap_open.ap_oid.aoi_id[0] = 0;
		pdu->ap_payload.ap_open.ap_oid.aoi_id[1] = 0;
		pdu->ap_payload.ap_open.ap_oid.aoi_idlen = 2;
	} else if (pdu->ap_payload.ap_open.ap_oid.aoi_idlen == 1) {
		log_warnx("AgentX(%"PRIu32"): Invalid oid: Open Failed",
		    conn->conn_id);
		goto fail;
	}
	/* RFC 2742 agentxSessionDescr */
	if (pdu->ap_payload.ap_open.ap_descr.aos_slen > 255) {
		log_warnx("AgentX(%"PRIu32"): Invalid descr (too long): Open "
		    "Failed", conn->conn_id);
		goto fail;
	}
	/*
	 * ax_ostring is always NUL-terminated, but doesn't scan for internal
	 * NUL-bytes. However, mbstowcs stops at NUL, which might be in the
	 * middle of the string.
	 */
	if (strlen(pdu->ap_payload.ap_open.ap_descr.aos_string) !=
	    pdu->ap_payload.ap_open.ap_descr.aos_slen ||
	    mbstowcs(NULL,
	    pdu->ap_payload.ap_open.ap_descr.aos_string, 0) == (size_t)-1) {
		log_warnx("AgentX(%"PRIu32"): Invalid descr (not UTF-8): "
		    "Open Failed", conn->conn_id);
		goto fail;
	}

	session->sess_timeout = pdu->ap_payload.ap_open.ap_timeout;
	session->sess_oid = pdu->ap_payload.ap_open.ap_oid;
	session->sess_descr.aos_slen = pdu->ap_payload.ap_open.ap_descr.aos_slen;
	if (pdu->ap_payload.ap_open.ap_descr.aos_string != NULL) {
		session->sess_descr.aos_string =
		    strdup(pdu->ap_payload.ap_open.ap_descr.aos_string);
		if (session->sess_descr.aos_string == NULL) {
			log_warn("AgentX(%"PRIu32"): strdup: Open Failed",
			    conn->conn_id);
			goto fail;
		}
	}
	    
	/* RFC 2742 agentxSessionIndex: chances of reuse, slim to none */
	do {
		session->sess_id = arc4random();
	} while (RB_INSERT(appl_agentx_sessions,
	    &appl_agentx_sessions, session) != NULL);

	if (asprintf(&(session->sess_backend.ab_name),
	    "AgentX(%"PRIu32"/%"PRIu32")",
	    conn->conn_id, session->sess_id) == -1) {
		log_warn("AgentX(%"PRIu32"): asprintf: Open Failed",
		    conn->conn_id);
		goto fail;
	}
	session->sess_backend.ab_cookie = session;
	session->sess_backend.ab_retries = 0;
	session->sess_backend.ab_fn = &appl_agentx_functions;
	RB_INIT(&(session->sess_backend.ab_requests));
	TAILQ_INSERT_TAIL(&(conn->conn_sessions), session, sess_conn_entry);

	appl_agentx_oid2ber_oid(&(session->sess_oid), &oid);
	smi_oid2string(&oid, oidbuf, sizeof(oidbuf), 0);
	log_info("%s: %s %s: Open", session->sess_backend.ab_name, oidbuf,
	    session->sess_descr.aos_string);

	ax_response(conn->conn_ax, session->sess_id, pdu->ap_header.aph_transactionid,
	    pdu->ap_header.aph_packetid, NULL, smi_getticks(), APPL_ERROR_NOERROR, 0,
	    NULL, 0);
	appl_agentx_send(-1, EV_WRITE, conn);

	return;
 fail:
	ax_response(conn->conn_ax, 0, pdu->ap_header.aph_transactionid,
	    pdu->ap_header.aph_packetid, NULL, 0, APPL_ERROR_OPENFAILED, 0,
	    NULL, 0);
	appl_agentx_send(-1, EV_WRITE, conn);
	if (session != NULL)
		free(session->sess_descr.aos_string);
	free(session);
}

void
appl_agentx_close(struct appl_agentx_session *session, struct ax_pdu *pdu)
{
	struct appl_agentx_connection *conn = session->sess_conn;
	char name[100];

	strlcpy(name, session->sess_backend.ab_name, sizeof(name));
	appl_agentx_session_free(session);
	log_info("%s: Closed by subagent (%s)", name,
	    ax_closereason2string(pdu->ap_payload.ap_close.ap_reason));

	ax_response(conn->conn_ax, pdu->ap_header.aph_sessionid,
	    pdu->ap_header.aph_transactionid, pdu->ap_header.aph_packetid,
	    &(pdu->ap_context), smi_getticks(), APPL_ERROR_NOERROR, 0, NULL, 0);
	appl_agentx_send(-1, EV_WRITE, conn);
}

void
appl_agentx_forceclose(struct appl_backend *backend,
    enum appl_close_reason reason)
{
	struct appl_agentx_session *session = backend->ab_cookie;
	char name[100];

	session->sess_conn->conn_ax->ax_byteorder = session->sess_byteorder;
	ax_close(session->sess_conn->conn_ax, session->sess_id,
	    (enum ax_close_reason) reason);
	appl_agentx_send(-1, EV_WRITE, session->sess_conn);

	strlcpy(name, session->sess_backend.ab_name, sizeof(name));
	appl_agentx_session_free(session);
	log_info("%s: Closed by snmpd (%s)", name,
	    ax_closereason2string((enum ax_close_reason)reason));
}

void
appl_agentx_session_free(struct appl_agentx_session *session)
{
	struct appl_agentx_connection *conn = session->sess_conn;

	appl_close(&(session->sess_backend));

	RB_REMOVE(appl_agentx_sessions, &appl_agentx_sessions, session);
	TAILQ_REMOVE(&(conn->conn_sessions), session, sess_conn_entry);

	free(session->sess_backend.ab_name);
	free(session->sess_descr.aos_string);
	free(session);
}

void
appl_agentx_register(struct appl_agentx_session *session, struct ax_pdu *pdu)
{
	uint32_t timeout;
	struct ber_oid oid;
	enum appl_error error;

	timeout = pdu->ap_payload.ap_register.ap_timeout;
	timeout = timeout != 0 ? timeout : session->sess_timeout != 0 ?
	    session->sess_timeout : AGENTX_DEFAULTTIMEOUT;
	timeout *= 100;

	if (appl_agentx_oid2ber_oid(
	    &(pdu->ap_payload.ap_register.ap_subtree), &oid) == NULL) {
		log_warnx("%s: Failed to register: oid too small",
		    session->sess_backend.ab_name);
		error = APPL_ERROR_PROCESSINGERROR;
		goto fail;
	}

	error = appl_register(pdu->ap_context.aos_string, timeout,
	    pdu->ap_payload.ap_register.ap_priority, &oid, 
	    pdu->ap_header.aph_flags & AX_PDU_FLAG_INSTANCE_REGISTRATION, 0,
	    pdu->ap_payload.ap_register.ap_range_subid,
	    pdu->ap_payload.ap_register.ap_upper_bound,
	    &(session->sess_backend));

 fail:
	ax_response(session->sess_conn->conn_ax, session->sess_id,
	    pdu->ap_header.aph_transactionid, pdu->ap_header.aph_packetid,
	    &(pdu->ap_context), smi_getticks(), error, 0, NULL, 0);
	appl_agentx_send(-1, EV_WRITE, session->sess_conn);
}

void
appl_agentx_unregister(struct appl_agentx_session *session, struct ax_pdu *pdu)
{
	struct ber_oid oid;
	enum appl_error error;

	if (appl_agentx_oid2ber_oid(
	    &(pdu->ap_payload.ap_unregister.ap_subtree), &oid) == NULL) {
		log_warnx("%s: Failed to unregister: oid too small",
		    session->sess_backend.ab_name);
		error = APPL_ERROR_PROCESSINGERROR;
		goto fail;
	}

	error = appl_unregister(pdu->ap_context.aos_string, 
	    pdu->ap_payload.ap_unregister.ap_priority, &oid, 
	    pdu->ap_payload.ap_unregister.ap_range_subid,
	    pdu->ap_payload.ap_unregister.ap_upper_bound,
	    &(session->sess_backend));

 fail:
	ax_response(session->sess_conn->conn_ax, session->sess_id,
	    pdu->ap_header.aph_transactionid, pdu->ap_header.aph_packetid,
	    &(pdu->ap_context), smi_getticks(), error, 0, NULL, 0);
	appl_agentx_send(-1, EV_WRITE, session->sess_conn);
}

#define AX_PDU_FLAG_INDEX (AX_PDU_FLAG_NEW_INDEX | AX_PDU_FLAG_ANY_INDEX)

void
appl_agentx_get(struct appl_backend *backend, int32_t transactionid,
    int32_t requestid, const char *ctx, struct appl_varbind *vblist)
{
	struct appl_agentx_session *session = backend->ab_cookie;
	struct ax_ostring *context, string;
	struct appl_varbind *vb;
	struct ax_searchrange *srl;
	size_t i, j, nsr;

	for (nsr = 0, vb = vblist; vb != NULL; vb = vb->av_next)
		nsr++;

	if ((srl = calloc(nsr, sizeof(*srl))) == NULL) {
		log_warn(NULL);
		appl_response(backend, requestid, APPL_ERROR_GENERR, 1, vblist);
		return;
	}

	for (i = 0, vb = vblist; i < nsr; i++, vb = vb->av_next) {
		srl[i].asr_start.aoi_include = vb->av_include;
		srl[i].asr_start.aoi_idlen = vb->av_oid.bo_n;
		for (j = 0; j < vb->av_oid.bo_n; j++)
			srl[i].asr_start.aoi_id[j] = vb->av_oid.bo_id[j];
		srl[i].asr_stop.aoi_include = 0;
		srl[i].asr_stop.aoi_idlen = 0;
	}
	if ((context = appl_agentx_string2ostring(ctx, &string)) == NULL) {
		if (errno != 0) {
			log_warn("Failed to convert context");
			appl_response(backend, requestid,
			    APPL_ERROR_GENERR, 1, vblist);
			free(srl);
			return;
		}
	}

	session->sess_conn->conn_ax->ax_byteorder = session->sess_byteorder;
	if (ax_get(session->sess_conn->conn_ax, session->sess_id, transactionid,
	    requestid, context, srl, nsr) == -1)
		appl_response(backend, requestid, APPL_ERROR_GENERR, 1, vblist);
	else
		appl_agentx_send(-1, EV_WRITE, session->sess_conn);
	free(srl);
	if (context != NULL)
		free(context->aos_string);
}

void
appl_agentx_getnext(struct appl_backend *backend, int32_t transactionid,
    int32_t requestid, const char *ctx, struct appl_varbind *vblist)
{
	struct appl_agentx_session *session = backend->ab_cookie;
	struct ax_ostring *context, string;
	struct appl_varbind *vb;
	struct ax_searchrange *srl;
	size_t i, j, nsr;

	for (nsr = 0, vb = vblist; vb != NULL; vb = vb->av_next)
		nsr++;

	if ((srl = calloc(nsr, sizeof(*srl))) == NULL) {
		log_warn(NULL);
		appl_response(backend, requestid, APPL_ERROR_GENERR, 1, vblist);
		return;
	}

	for (i = 0, vb = vblist; i < nsr; i++, vb = vb->av_next) {
		srl[i].asr_start.aoi_include = vb->av_include;
		srl[i].asr_start.aoi_idlen = vb->av_oid.bo_n;
		for (j = 0; j < vb->av_oid.bo_n; j++)
			srl[i].asr_start.aoi_id[j] = vb->av_oid.bo_id[j];
		srl[i].asr_stop.aoi_include = 0;
		srl[i].asr_stop.aoi_idlen = vb->av_oid_end.bo_n;
		for (j = 0; j < vb->av_oid.bo_n; j++)
			srl[i].asr_stop.aoi_id[j] = vb->av_oid_end.bo_id[j];
	}
	if ((context = appl_agentx_string2ostring(ctx, &string)) == NULL) {
		if (errno != 0) {
			log_warn("Failed to convert context");
			appl_response(backend, requestid,
			    APPL_ERROR_GENERR, 1, vblist);
			free(srl);
			return;
		}
	}

	session->sess_conn->conn_ax->ax_byteorder = session->sess_byteorder;
	if (ax_getnext(session->sess_conn->conn_ax, session->sess_id, transactionid,
	    requestid, context, srl, nsr) == -1)
		appl_response(backend, requestid, APPL_ERROR_GENERR, 1, vblist);
	else
		appl_agentx_send(-1, EV_WRITE, session->sess_conn);
	free(srl);
	if (context != NULL)
		free(context->aos_string);
}

void
appl_agentx_response(struct appl_agentx_session *session, struct ax_pdu *pdu)
{
	struct appl_varbind *response = NULL;
	struct ax_varbind *vb;
	enum appl_error error;
	uint16_t index;
	size_t i, nvarbind;

	nvarbind = pdu->ap_payload.ap_response.ap_nvarbind;
	if ((response = calloc(nvarbind, sizeof(*response))) == NULL) {
		log_warn(NULL);
		appl_response(&(session->sess_backend),
		    pdu->ap_header.aph_packetid,
		    APPL_ERROR_GENERR, 1, NULL);
		return;
	}

	error = (enum appl_error)pdu->ap_payload.ap_response.ap_error;
	index = pdu->ap_payload.ap_response.ap_index;
	for (i = 0; i < nvarbind; i++) {
		response[i].av_next = i + 1 == nvarbind ?
		    NULL : &(response[i + 1]);
		vb = &(pdu->ap_payload.ap_response.ap_varbindlist[i]);

		if (appl_agentx_oid2ber_oid(&(vb->avb_oid),
		    &(response[i].av_oid)) == NULL) {
			log_warnx("%s: invalid oid",
			    session->sess_backend.ab_name);
			if (error != APPL_ERROR_NOERROR) {
				error = APPL_ERROR_GENERR;
				index = i + 1;
			}
			continue;
		}
		response[i].av_value = appl_agentx_value2ber_element(vb);
		if (response[i].av_value == NULL) {
			log_warn("%s: Failed to parse response value",
			    session->sess_backend.ab_name);
			if (error != APPL_ERROR_NOERROR) {
				error = APPL_ERROR_GENERR;
				index = i + 1;
			}
		}
	}
	appl_response(&(session->sess_backend), pdu->ap_header.aph_packetid,
	    error, index, response);
	free(response);
}

void
appl_agentx_send(int fd, short event, void *cookie)
{
	struct appl_agentx_connection *conn = cookie;

	switch (ax_send(conn->conn_ax)) {
	case -1:
		if (errno == EAGAIN)
			break;
		log_warn("AgentX(%"PRIu32")", conn->conn_id);
		ax_free(conn->conn_ax);
		conn->conn_ax = NULL;
		appl_agentx_free(conn);
		return;
	case 0:
		return;
	default:
		break;
	}
	event_add(&(conn->conn_wev), NULL);
}

struct ber_oid *
appl_agentx_oid2ber_oid(struct ax_oid *aoid, struct ber_oid *boid)
{
	size_t i;

	if (aoid->aoi_idlen < BER_MIN_OID_LEN ||
	    aoid->aoi_idlen > BER_MAX_OID_LEN) {
		errno = EINVAL;
		return NULL;
	}
	

	boid->bo_n = aoid->aoi_idlen;
	for (i = 0; i < boid->bo_n; i++)
		boid->bo_id[i] = aoid->aoi_id[i];
	return boid;
}

struct ber_element *
appl_agentx_value2ber_element(struct ax_varbind *vb)
{
	struct ber_oid oid;
	struct ber_element *elm;

	switch (vb->avb_type) {
	case AX_DATA_TYPE_INTEGER:
		return ober_add_integer(NULL, vb->avb_data.avb_int32);
	case AX_DATA_TYPE_OCTETSTRING:
		return ober_add_nstring(NULL,
		    vb->avb_data.avb_ostring.aos_string,
		    vb->avb_data.avb_ostring.aos_slen);
	case AX_DATA_TYPE_NULL:
		return ober_add_null(NULL);
	case AX_DATA_TYPE_OID:
		if (appl_agentx_oid2ber_oid(
		    &(vb->avb_data.avb_oid), &oid) == NULL)
			return NULL;
		return ober_add_oid(NULL, &oid);
	case AX_DATA_TYPE_IPADDRESS:
		if ((elm = ober_add_nstring(NULL,
		    vb->avb_data.avb_ostring.aos_string,
		    vb->avb_data.avb_ostring.aos_slen)) == NULL)
			return NULL;
		ober_set_header(elm, BER_CLASS_APPLICATION, SNMP_T_IPADDR);
		return elm;
	case AX_DATA_TYPE_COUNTER32:
		elm = ober_add_integer(NULL, vb->avb_data.avb_uint32);
		if (elm == NULL)
			return NULL;
		ober_set_header(elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER32);
		return elm;
	case AX_DATA_TYPE_GAUGE32:
		elm = ober_add_integer(NULL, vb->avb_data.avb_uint32);
		if (elm == NULL)
			return NULL;
		ober_set_header(elm, BER_CLASS_APPLICATION, SNMP_T_GAUGE32);
		return elm;
	case AX_DATA_TYPE_TIMETICKS:
		elm = ober_add_integer(NULL, vb->avb_data.avb_uint32);
		if (elm == NULL)
			return NULL;
		ober_set_header(elm, BER_CLASS_APPLICATION, SNMP_T_TIMETICKS);
		return elm;
	case AX_DATA_TYPE_OPAQUE:
		if ((elm = ober_add_nstring(NULL,
		    vb->avb_data.avb_ostring.aos_string,
		    vb->avb_data.avb_ostring.aos_slen)) == NULL)
			return NULL;
		ober_set_header(elm, BER_CLASS_APPLICATION, SNMP_T_OPAQUE);
		return elm;
	case AX_DATA_TYPE_COUNTER64:
		elm = ober_add_integer(NULL, vb->avb_data.avb_uint64);
		if (elm == NULL)
			return NULL;
		ober_set_header(elm, BER_CLASS_APPLICATION, SNMP_T_COUNTER64);
		return elm;
	case AX_DATA_TYPE_NOSUCHOBJECT:
		return appl_exception(APPL_EXC_NOSUCHOBJECT);
	case AX_DATA_TYPE_NOSUCHINSTANCE:
		return appl_exception(APPL_EXC_NOSUCHINSTANCE);
	case AX_DATA_TYPE_ENDOFMIBVIEW:
		return appl_exception(APPL_EXC_ENDOFMIBVIEW);
	default:
		errno = EINVAL;
		return NULL;
	}
}

struct ax_ostring *
appl_agentx_string2ostring(const char *str, struct ax_ostring *ostring)
{
	if (str == NULL) {
		errno = 0;
		return NULL;
	}

	ostring->aos_slen = strlen(str);
	if ((ostring->aos_string = strdup(str)) == NULL)
		return NULL;
	return ostring;
}

int
appl_agentx_cmp(struct appl_agentx_connection *conn1,
    struct appl_agentx_connection *conn2)
{
	return conn1->conn_id < conn2->conn_id ? -1 :
	    conn1->conn_id > conn2->conn_id;
}

int
appl_agentx_session_cmp(struct appl_agentx_session *sess1,
    struct appl_agentx_session *sess2)
{
	return sess1->sess_id < sess2->sess_id ? -1 : sess1->sess_id > sess2->sess_id;
}

RB_GENERATE_STATIC(appl_agentx_conns, appl_agentx_connection, conn_entry,
    appl_agentx_cmp);
RB_GENERATE_STATIC(appl_agentx_sessions, appl_agentx_session, sess_entry,
    appl_agentx_session_cmp);
