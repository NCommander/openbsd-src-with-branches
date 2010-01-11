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
/* $Id: radius_req.c 36323 2009-09-24 13:38:13Z ymatsui $ */
/**@file
 * radius+ �饤�֥��� event(3) ���Ȥ߹�碌����RADIUS �׵�Τ���Υ桼�ƥ�
 * ��ƥ���
 * @author	Yasuoka Masahiko
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <radius+.h>
#include <radiusconst.h>
#include <debugutil.h>
#include <time.h>
#include <event.h>
#include <string.h>

#include "radius_req.h"

struct overlapped {
	struct event ev_sock;
	int socket;
	int ntry;
	int timeout;
	struct sockaddr_storage ss;
	void *context;
	radius_response *response_fn;
	char secret[MAX_RADIUS_SECRET];
	RADIUS_PACKET *pkt;
};

static int   radius_request0 (struct overlapped *);
static void  radius_request_io_event (int, short, void *);
static int select_srcaddr(struct sockaddr const *, struct sockaddr *, socklen_t *);

#ifdef	RADIUS_REQ_DEBUG
#define	RADIUS_REQ_ASSERT(cond)					\
	if (!(cond)) {						\
	    fprintf(stderr,					\
		"\nASSERT(" #cond ") failed on %s() at %s:%d.\n"\
		, __func__, __FILE__, __LINE__);		\
	    abort(); 						\
	}
#else
#define	RADIUS_REQ_ASSERT(cond)			
#endif

/**
 * RADIUS �ꥯ�����Ȥ��������ޤ������ꤷ�� pkt(RADIUS �ѥ��å�) �ϡ����μ���
 * �����ǲ�������ޤ���
 * @param pkt		�׵᤹�� RADIUS �ѥ��åȡ�
 */
void
radius_request(RADIUS_REQUEST_CTX ctx, RADIUS_PACKET *pkt)
{
	struct overlapped *lap;

	RADIUS_REQ_ASSERT(pkt != NULL);
	RADIUS_REQ_ASSERT(ctx != NULL);
	lap = ctx;
	lap->pkt = pkt;
	if (radius_request0(lap) != 0) {
		if (lap->response_fn != NULL)
			lap->response_fn(lap->context, NULL,
			    RADIUS_REQUST_ERROR);
	}
}

/**
 * NAS-IP-Address �ޤ��� NAS-IPv6-Address ��������ޤ���
 * setting->server[setting->curr_server].sock �˥����륢�ɥ쥹������
 * �Ȥ���Ƥ��ʤ����ˤϡ������ФΥ��ɥ쥹���鼫ư�ǥ����륢�ɥ쥹��
 * ���åȤ��ޤ���
 */
int
radius_prepare_nas_address(radius_req_setting *setting,
    RADIUS_PACKET *pkt)
{
	int af;
	struct sockaddr_in *sin4;
	struct sockaddr_in6 *sin6;
	socklen_t socklen;

	/* See RFC 2765, 3162 */
	RADIUS_REQ_ASSERT(setting != NULL);

	af = setting->server[setting->curr_server].peer.sin6.sin6_family;
	RADIUS_REQ_ASSERT(af == AF_INET6 || af == AF_INET);

	sin4 = &setting->server[setting->curr_server].sock.sin4;
	sin6 = &setting->server[setting->curr_server].sock.sin6;

	switch (af) {
	case AF_INET:
		socklen = sizeof(*sin4);
		if (sin4->sin_addr.s_addr == INADDR_ANY) {
			if (select_srcaddr((struct sockaddr const *)
			    &setting->server[setting->curr_server].peer,
			    (struct sockaddr *)sin4, &socklen) != 0) {
				RADIUS_REQ_ASSERT("NOTREACHED" == NULL);
				goto reigai;
			}
		}
		if (radius_put_ipv4_attr(pkt, RADIUS_TYPE_NAS_IP_ADDRESS,
		    sin4->sin_addr) != 0)
			goto reigai;
		break;
	case AF_INET6:
		socklen = sizeof(*sin6);
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			if (select_srcaddr((struct sockaddr const *)
			    &setting->server[setting->curr_server].peer,
			    (struct sockaddr *)sin4, &socklen) != 0) {
				RADIUS_REQ_ASSERT("NOTREACHED" == NULL);
				goto reigai;
			}
		}
		if (radius_put_raw_attr(pkt, RADIUS_TYPE_NAS_IPV6_ADDRESS,
		    sin6->sin6_addr.s6_addr, sizeof(sin6->sin6_addr.s6_addr))
		    != 0)
			goto reigai;
		break;
	}

	return 0;
reigai:
	return 1;
}

/**
 * RADIUS �ꥯ�����Ȥ��������������Ԥ��ޤ���
 * <p>
 * ���������ä����䥿���ॢ���ȤʤɤΥ��顼�ϡ����ꤷ���ؿ���Ƥӽ�(������
 * �Хå�)�������Τ��ޤ���</p>
 * <p>
 * pctx �˽񤭽Ф���륳��ƥ����Ȥȱ���������Хå��ؿ��ǻ��ꤵ���
 * �����ѥ��åȤϡ����μ��������ǲ�������ޤ���</p>
 * @param setting	RADIUS �����Ф��䤤��碌������
 * @param context	�ƤӽФ�¦����ƥ�����
 * @param pctx		RADIUS �ꥯ�����ȥ���ƥ�����(RADIUS_REQUEST_CTX)
 *			��񤭽Ф��ΰ����ꤷ�ޤ�������󥻥뤹����ʤɤˡ�
 *			���Ѥ��ޤ���NULL ����ꤷ�Ƥ⹽���ޤ���
 * @param response_fn	������������뤤�ϥ����ॢ���Ȥ������˸ƤӽФ��ؿ���
 *			�ؿ��ݥ��󥿤ǻ��ꤷ�ޤ���
 * @param timeout	���������ॢ���ȤޤǤ��ÿ�
 */
int
radius_prepare(radius_req_setting *setting, void *context,
    RADIUS_REQUEST_CTX *pctx, radius_response response_fn, int timeout)
{
	int sock;
	struct overlapped *lap;
	struct sockaddr_in6 *sin6;

	RADIUS_REQ_ASSERT(setting != NULL);
	lap = NULL;

	if (setting->server[setting->curr_server].enabled == 0)
		return 1;
	if ((lap = malloc(sizeof(struct overlapped))) == NULL) {
		log_printf(LOG_ERR, "malloc() failed in %s: %m", __func__);
		goto reigai;
	}
	sin6 = &setting->server[setting->curr_server].peer.sin6;
	if ((sock = socket(sin6->sin6_family, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		log_printf(LOG_ERR, "socket() failed in %s: %m", __func__);
		goto reigai;
	}
	memset(lap, 0, sizeof(struct overlapped));
	memcpy(&lap->ss, &setting->server[setting->curr_server].peer,
	    setting->server[setting->curr_server].peer.sin6.sin6_len);

	lap->socket = sock;
	lap->timeout = MIN(setting->timeout, timeout);
	lap->ntry = timeout / lap->timeout;
	lap->context = context;
	lap->response_fn = response_fn;
	memcpy(lap->secret, setting->server[setting->curr_server].secret,
	    sizeof(lap->secret));
	if (pctx != NULL)
		*pctx = lap;

	return 0;
reigai:
	if (lap != NULL)
		free(lap);

	return 1;
}

/**
 * RADIUS �׵�򥭥�󥻥뤷�ޤ���
 * @param	{@link radius_request()} �Ǽ�����ä�����ƥ�����
 */
void
radius_cancel_request(RADIUS_REQUEST_CTX ctx)
{
	struct overlapped *lap;

	lap = ctx;
	if (lap->socket >= 0) {
		event_del(&lap->ev_sock);
		close(lap->socket);
		lap->socket = -1;
	}
	if (lap->pkt != NULL) {
		radius_delete_packet(lap->pkt);
		lap->pkt = NULL;
	}
	/* ������ no comment��*/
	memset(lap->secret, 0x41, sizeof(lap->secret));

	free(lap);
}

/** ���Υ���ƥ����Ȥ����Ѥ��Ƥ��� RADIUS �����Фζ�ͭ��̩�����֤��ޤ���*/
const char *
radius_get_server_secret(RADIUS_REQUEST_CTX ctx)
{
	struct overlapped *lap;

	lap = ctx;
	RADIUS_REQ_ASSERT(lap != NULL);

	return lap->secret;
}

/** ���Υ���ƥ����Ȥ����Ѥ��Ƥ��� RADIUS �����ФΥ��ɥ쥹���֤��ޤ���*/
struct sockaddr *
radius_get_server_address(RADIUS_REQUEST_CTX ctx)
{
	struct overlapped *lap;

	lap = ctx;
	RADIUS_REQ_ASSERT(lap != NULL);

	return (struct sockaddr *)&lap->ss;
}

static int
radius_request0(struct overlapped *lap)
{
	struct timeval tv0;

	RADIUS_REQ_ASSERT(lap->ntry > 0);

	lap->ntry--;
	if (radius_sendto(lap->socket, lap->pkt, 0, (struct sockaddr *)
	    &lap->ss, lap->ss.ss_len) != 0)
		return 1;
	tv0.tv_usec = 0;
	tv0.tv_sec = lap->timeout;

	event_set(&lap->ev_sock, lap->socket, EV_READ,
	    radius_request_io_event, lap);
	event_add(&lap->ev_sock, &tv0);

	return 0;
}

static void
radius_request_io_event(int fd, short evmask, void *context)
{
	struct overlapped *lap;
	struct sockaddr_storage ss;
	int flags;
	socklen_t len;
	RADIUS_PACKET *respkt;

	RADIUS_REQ_ASSERT(context != NULL);

	if ((evmask & EV_READ) != 0) {
		lap = context;
		flags = 0;

		RADIUS_REQ_ASSERT(lap->socket >= 0);
		if (lap->socket < 0)
			return;
		RADIUS_REQ_ASSERT(lap->pkt != NULL);

		memset(&ss, 0, sizeof(ss));
		len = sizeof(ss);
		if ((respkt = radius_recvfrom(lap->socket, 0,
		    (struct sockaddr *)&ss, &len)) == NULL) {
			log_printf(LOG_ERR, "recvfrom() failed in %s: %m",
			    __func__);
			flags |= RADIUS_REQUST_ERROR;
		} else if (lap->secret[0] == '\0') {
			flags |= RADIUS_REQUST_CHECK_AUTHENTICTOR_NO_CHECK;
		} else {
			radius_set_request_packet(respkt, lap->pkt);
			if (!radius_check_response_authenticator(respkt,
			    lap->secret))
				flags |= RADIUS_REQUST_CHECK_AUTHENTICTOR_OK;
		}

		if (lap->response_fn != NULL)
			lap->response_fn(lap->context, respkt, flags);

		if (respkt != NULL)
			radius_delete_packet(respkt);
		radius_cancel_request(lap);
	} else if ((evmask & EV_TIMEOUT) != 0) {
		lap = context;
		if (lap->ntry > 0) {
			if (radius_request0(lap) != 0) { 
				if (lap->response_fn != NULL) 
					lap->response_fn(lap->context, NULL,
					    RADIUS_REQUST_ERROR);
				radius_cancel_request(lap);
			}
			return;
		}
		if (lap->response_fn != NULL)
			lap->response_fn(lap->context, NULL,
			    RADIUS_REQUST_TIMEOUT);
		radius_cancel_request(lap);
	}
}

static int
select_srcaddr(struct sockaddr const *dst, struct sockaddr *src,
    socklen_t *srclen)
{
	int sock;

	sock = -1;
	if ((sock = socket(dst->sa_family, SOCK_DGRAM, IPPROTO_UDP)) < 0)
		goto reigai;
	if (connect(sock, dst, dst->sa_len) != 0)
		goto reigai;
	if (getsockname(sock, src, srclen) != 0)
		goto reigai;

	close(sock);

	return 0;
reigai:
	if (sock >= 0)
		close(sock);

	return 1;
}
