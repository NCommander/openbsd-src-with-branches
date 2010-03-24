/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* $Id: pap.c 35695 2009-04-13 14:52:44Z yasuoka $ */
/**@file
 * Password Authentication Protocol (PAP) �μ���
 * @author Yasuoka Masahiko
 */
/*
 *   Windows 2000 �� PAP ��Ԥ��ȡ�8�ô֤�10�Ĥ� AuthReq ���Ϥ������ॢ����
 *   �����ͤ� CHAP �ξ���Ⱦʬ�ʲ��ʤΤǡ�Radius �׵�Υ����ॢ���Ȥ��ͤϡ�
 *   CHAP �� PAP ���̡�������Ǥ����ۤ����ɤ����⤷��ʤ���
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if_dl.h>
#include <netinet/in.h>

#include <event.h>
#include <md5.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "slist.h"
#include "npppd.h"
#include "ppp.h"

#ifdef USE_NPPPD_RADIUS
#include "radius_chap_const.h"
#endif
#include "debugutil.h"

#define	AUTHREQ				0x01
#define	AUTHACK				0x02
#define	AUTHNAK				0x03

#define	PAP_STATE_INITIAL		0
#define	PAP_STATE_STARTING		1
#define	PAP_STATE_AUTHENTICATING	2
#define	PAP_STATE_SENT_RESPONSE		3
#define	PAP_STATE_STOPPED		4
#define	PAP_STATE_PROXY_AUTHENTICATION	5

#define	DEFAULT_SUCCESS_MESSAGE		"OK"
#define	DEFAULT_FAILURE_MESSAGE		"Unknown username or password"
#define	DEFAULT_ERROR_MESSAGE		"Unknown failure"

#ifdef	PAP_DEBUG
#define	PAP_DBG(x)	pap_log x
#define	PAP_ASSERT(cond)					\
	if (!(cond)) {						\
	    fprintf(stderr,					\
		"\nASSERT(" #cond ") failed on %s() at %s:%d.\n"\
		, __func__, __FILE__, __LINE__);		\
	    abort(); 						\
	}
#else
#define	PAP_ASSERT(cond)			
#define	PAP_DBG(x)	
#endif

static void  pap_log (pap *, uint32_t, const char *, ...) __printflike(3,4);
static void  pap_response (pap *, int, const char *);
static void  pap_authenticate(pap *, const char *);
static void  pap_local_authenticate (pap *, const char *, const char *);
#ifdef USE_NPPPD_RADIUS
static void  pap_radius_authenticate (pap *, const char *, const char *);
static void  pap_radius_response (void *, RADIUS_PACKET *, int);
#endif

#ifdef __cplusplus
extern "C" {
#endif

void  pap_init (pap *, npppd_ppp *);
int   pap_start (pap *);
int   pap_stop (pap *);
int   pap_input (pap *, u_char *, int);

#ifdef __cplusplus
}
#endif

void
pap_init(pap *_this, npppd_ppp *ppp)
{
	_this->ppp = ppp;
	_this->state = PAP_STATE_INITIAL;
	_this->auth_id = -1;
}

int
pap_start(pap *_this)
{
	pap_log(_this, LOG_DEBUG, "%s", __func__);

	if (_this->state == PAP_STATE_PROXY_AUTHENTICATION) {
		_this->state = PAP_STATE_AUTHENTICATING;
		pap_authenticate(_this, _this->ppp->proxy_authen_resp);
		return 0;
	}

	_this->state = PAP_STATE_STARTING;
	return 0;
}

int
pap_stop(pap *_this)
{
	_this->state = PAP_STATE_STOPPED;
	_this->auth_id = -1;

#ifdef USE_NPPPD_RADIUS
	if (_this->radctx != NULL) {
		radius_cancel_request(_this->radctx);
		_this->radctx = NULL;
	}
#endif
	return 0;
}

/** PAP �Υѥ��åȼ��� */
int
pap_input(pap *_this, u_char *pktp, int lpktp)
{
	int code, id, length, len;
	u_char *pktp1;
	char name[MAX_USERNAME_LENGTH], password[MAX_PASSWORD_LENGTH];

	if (_this->state == PAP_STATE_STOPPED ||
	    _this->state == PAP_STATE_INITIAL) {
		pap_log(_this, LOG_ERR, "Received pap packet.  But pap is "
		    "not started.");
		return -1;
	}
	pktp1 = pktp;

	GETCHAR(code, pktp1);
	GETCHAR(id, pktp1);
	GETSHORT(length, pktp1);

	if (code != AUTHREQ) {
		pap_log(_this, LOG_ERR, "%s: Received unknown code=%d",
		    __func__, code);
		return -1;
	}
	if (lpktp < length) {
		pap_log(_this, LOG_ERR, "%s: Received broken packet.",
		    __func__);
		return -1;
	}

	/* �桼��̾����Ф� */
#define	remlen		(lpktp - (pktp1 - pktp))
	if (remlen < 1)
		goto reigai;
	GETCHAR(len, pktp1);
	if (len <= 0)
		goto reigai;
	if (remlen < len)
		goto reigai;
	if (len > 0)
		memcpy(name, pktp1, len);
	name[len] = '\0';
	pktp1 += len;

	if (_this->state != PAP_STATE_STARTING) {
		/*
		 * �ޤä���Ʊ���׵���ټ�����ä����ϡ������κ����ˤ��
		 * ��Ρ�UserName ��Ʊ���ʤ��³�Ԥ��롣
		 */
		if ((_this->state == PAP_STATE_AUTHENTICATING ||
		    _this->state == PAP_STATE_SENT_RESPONSE) &&
		    strcmp(_this->name, name) == 0) {
			/* ³�� */
		} else {
			pap_log(_this, LOG_ERR,
			    "Received AuthReq is not same as before.  "
			    "(%d,%s) != (%d,%s)", id, name, _this->auth_id,
			    _this->name);
			_this->auth_id = id;
			goto reigai;
		}
	}
	if (_this->state == PAP_STATE_AUTHENTICATING)
		return 0;
	_this->auth_id = id;
	strlcpy(_this->name, name, sizeof(_this->name));

	_this->state = PAP_STATE_AUTHENTICATING;

	/* �ѥ���ɤ���Ф� */
	if (remlen < 1)
		goto reigai;
	GETCHAR(len, pktp1);
	if (remlen < len)
		goto reigai;
	if (len > 0)
		memcpy(password, pktp1, len);

	password[len] = '\0';
	pap_authenticate(_this, password);

	return 0;
reigai:
	pap_response(_this, 0, DEFAULT_FAILURE_MESSAGE);
	return -1;
}

static void
pap_authenticate(pap *_this, const char *password)
{
	if (npppd_ppp_bind_realm(_this->ppp->pppd, _this->ppp, _this->name, 0)
	    == 0) {
		if (!npppd_ppp_is_realm_ready(_this->ppp->pppd, _this->ppp)) {
			pap_log(_this, LOG_INFO,
			    "username=\"%s\" realm is not ready.", _this->name);
			goto reigai;
			/* NOTREACHED */
		}
#if USE_NPPPD_RADIUS
		if (npppd_ppp_is_realm_radius(_this->ppp->pppd, _this->ppp)) {
			pap_radius_authenticate(_this, _this->name, password);
			return;
			/* NOTREACHED */
		} else
#endif
		if (npppd_ppp_is_realm_local(_this->ppp->pppd, _this->ppp)) {
			pap_local_authenticate(_this, _this->name, password);
			return;
			/* NOTREACHED */
		}
	}
reigai:
	pap_response(_this, 0, DEFAULT_FAILURE_MESSAGE);
}

static void
pap_log(pap *_this, uint32_t prio, const char *fmt, ...)
{
	char logbuf[BUFSIZ];
	va_list ap;

	va_start(ap, fmt);
	snprintf(logbuf, sizeof(logbuf), "ppp id=%u layer=pap %s",
	    _this->ppp->id, fmt);
	vlog_printf(prio, logbuf, ap);
	va_end(ap);
}

static void
pap_response(pap *_this, int authok, const char *mes)
{
	int lpktp, lmes;
	u_char *pktp, *pktp1;
	const char *realm;

	pktp = ppp_packetbuf(_this->ppp, PPP_PROTO_PAP) + HEADERLEN;
	lpktp = _this->ppp->mru - HEADERLEN;
	realm = npppd_ppp_get_realm_name(_this->ppp->pppd, _this->ppp);

	pktp1 = pktp;
	if (mes == NULL)
		lmes = 0;
	else
		lmes = strlen(mes);
	lmes = MIN(lmes, lpktp - 1);

	PUTCHAR(lmes, pktp1);
	if (lmes > 0)
		memcpy(pktp1, mes, lmes);
	lpktp = lmes + 1;

	if (authok)
		ppp_output(_this->ppp, PPP_PROTO_PAP, AUTHACK, _this->auth_id,
		    pktp, lpktp);
	else
		ppp_output(_this->ppp, PPP_PROTO_PAP, AUTHNAK, _this->auth_id,
		    pktp, lpktp);

	if (!authok) {
		pap_log(_this, LOG_ALERT,
		    "logtype=Failure username=\"%s\" realm=%s", _this->name,
		    realm);
		pap_stop(_this);
		/* ���Ԥ����� ppp ��λ */
		ppp_stop_ex(_this->ppp, "Authentication Required",
		    PPP_DISCON_AUTH_FAILED, PPP_PROTO_PAP, 1 /* peer */, NULL);
	} else {
		strlcpy(_this->ppp->username, _this->name,
		    sizeof(_this->ppp->username));
		pap_log(_this, LOG_INFO,
		    "logtype=Success username=\"%s\" realm=%s", _this->name,
		    realm);
		pap_stop(_this);
		ppp_auth_ok(_this->ppp);
		// �����׵�������뤿��� pap_stop �ǤΥ��åȤ��񤭤��ޤ���
		_this->state = PAP_STATE_SENT_RESPONSE;
	}
}

/** PAPǧ�� */
static void
pap_local_authenticate(pap *_this, const char *username, const char *password)
{
	int lpassword0;
	char password0[MAX_PASSWORD_LENGTH];

	lpassword0 = sizeof(password0);

	if (npppd_get_user_password(_this->ppp->pppd, _this->ppp, username,
	    password0, &lpassword0) == 0) {
		if (!strcmp(password0, password)) {
			pap_response(_this, 1, DEFAULT_SUCCESS_MESSAGE);
			return;
		}
	}
	pap_response(_this, 0, DEFAULT_FAILURE_MESSAGE);
}

/***********************************************************************
 * Proxy Authentication
 ***********************************************************************/
int
pap_proxy_authen_prepare(pap *_this, dialin_proxy_info *dpi)
{

	PAP_ASSERT(dpi->auth_type == PPP_AUTH_PAP);
	PAP_ASSERT(_this->state == PAP_STATE_INITIAL);

	_this->auth_id = dpi->auth_id;
	if (strlen(dpi->username) >= sizeof(_this->name)) {
		pap_log(_this, LOG_NOTICE,
		    "\"Proxy Authen Name\" is too long.");
		return -1;
	}

	/* copy the authenticaiton properties */
	PAP_ASSERT(_this->ppp->proxy_authen_resp == NULL);
	if ((_this->ppp->proxy_authen_resp = malloc(dpi->lauth_resp + 1)) ==
	    NULL) {
		pap_log(_this, LOG_ERR, "malloc() failed in %s(): %m",
		    __func__);
		return -1;
	}
	memcpy(_this->ppp->proxy_authen_resp, dpi->auth_resp,
	    dpi->lauth_resp);
	_this->ppp->proxy_authen_resp[dpi->lauth_resp] = '\0';
	strlcpy(_this->name, dpi->username, sizeof(_this->name));

	_this->state = PAP_STATE_PROXY_AUTHENTICATION;

	return 0;
}

#ifdef USE_NPPPD_RADIUS
static void
pap_radius_authenticate(pap *_this, const char *username, const char *password)
{
	void *radctx;
	RADIUS_PACKET *radpkt;
	MD5_CTX md5ctx;
	int i, j, s_len, passlen;
	u_char ra[16], digest[16], pass[128];
	const char *s;
	radius_req_setting *rad_setting = NULL;
	char buf0[MAX_USERNAME_LENGTH];

	if ((rad_setting = npppd_get_radius_req_setting(_this->ppp->pppd,
	    _this->ppp)) == NULL)
		goto reigai;

	if ((radpkt = radius_new_request_packet(RADIUS_CODE_ACCESS_REQUEST))
	    == NULL)
		goto reigai;

	if (radius_prepare(rad_setting, _this, &radctx, 
	    pap_radius_response, _this->ppp->auth_timeout) != 0) {
		radius_delete_packet(radpkt);
		goto reigai;
	}

	if (ppp_set_radius_attrs_for_authreq(_this->ppp, rad_setting, radpkt)
	    != 0)
		goto reigai;

	if (radius_put_string_attr(radpkt, RADIUS_TYPE_USER_NAME,
	    npppd_ppp_get_username_for_auth(_this->ppp->pppd, _this->ppp, 
	    username, buf0)) != 0)
		goto reigai;

	if (_this->radctx != NULL)
		radius_cancel_request(_this->radctx);

	_this->radctx = radctx;

	/*
	 * RADIUS User-Password �����ȥ�ӥ塼�Ȥκ���
	 * (RFC 2865, "5.2.  User-Password")
	 */
	s = radius_get_server_secret(_this->radctx);
	s_len = strlen(s);

	memset(pass, 0, sizeof(pass));			// null-padding
	passlen = MIN(strlen(password), sizeof(pass));
	memcpy(pass, password, passlen);
	if ((passlen % 16) != 0)
		passlen += 16 - (passlen % 16);

	radius_get_authenticator(radpkt, ra);

	MD5Init(&md5ctx);
	MD5Update(&md5ctx, s, s_len);
	MD5Update(&md5ctx, ra, 16);
	MD5Final(digest, &md5ctx);

	for (i = 0; i < 16; i++)
		pass[i] ^= digest[i];

	while (i < passlen) {
		MD5Init(&md5ctx);
		MD5Update(&md5ctx, s, s_len);
		MD5Update(&md5ctx, &pass[i - 16], 16);
		MD5Final(digest, &md5ctx);

		for (j = 0; j < 16; j++, i++)
			pass[i] ^= digest[j];
	}

	if (radius_put_raw_attr(radpkt, RADIUS_TYPE_USER_PASSWORD, pass,
	    passlen) != 0)
		goto reigai;

	radius_request(_this->radctx, radpkt);

	return;
reigai:
	if (_this->radctx != NULL)
		radius_cancel_request(_this->radctx);
	pap_log(_this, LOG_ERR, "%s() failed: %m", __func__);
	pap_response(_this, 0, DEFAULT_ERROR_MESSAGE);

	return;
}

static void
pap_radius_response(void *context, RADIUS_PACKET *pkt, int flags)
{
	int code = -1;
	const char *reason = NULL;
	RADIUS_REQUEST_CTX radctx;
	pap *_this;

	_this = context;
	radctx = _this->radctx;
	_this->radctx = NULL;	/* ��� */

	if (pkt == NULL) {
		if (flags & RADIUS_REQUST_TIMEOUT) {
			reason = "timeout";
			npppd_radius_server_failure_notify(_this->ppp->pppd,
			    _this->ppp, radctx, "request timeout");
		} else {
			reason = "error";
			npppd_radius_server_failure_notify(_this->ppp->pppd,
			    _this->ppp, radctx, "unknown error");
		}
		goto auth_failed;
	}
	code = radius_get_code(pkt);
	if (code == RADIUS_CODE_ACCESS_REJECT) {
		reason="reject";
		goto auth_failed;
	} else if (code != RADIUS_CODE_ACCESS_ACCEPT) {
		reason="error";
		goto auth_failed;
	}
	if ((flags & RADIUS_REQUST_CHECK_AUTHENTICTOR_OK) == 0 &&
	    (flags & RADIUS_REQUST_CHECK_AUTHENTICTOR_NO_CHECK) == 0) {
		reason="bad_authenticator";
		npppd_radius_server_failure_notify(_this->ppp->pppd, _this->ppp,
		    radctx, "bad authenticator");
		goto auth_failed;
	}
	// ǧ�� OK
	pap_response(_this, 1, DEFAULT_SUCCESS_MESSAGE);
	ppp_proccess_radius_framed_ip(_this->ppp, pkt);
	radius_delete_packet(pkt);

	return;
auth_failed:
	// ǧ�� NG
	pap_log(_this, LOG_WARNING, "Radius authentication request failed: %s",
	    reason);
	if (pkt != NULL)
		radius_delete_packet(pkt);

	pap_response(_this, 0, DEFAULT_FAILURE_MESSAGE);
}
#endif
