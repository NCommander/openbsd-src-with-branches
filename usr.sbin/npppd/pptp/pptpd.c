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
/* $Id: pptpd.c 39106 2010-01-10 21:01:39Z yasuoka $ */
/**@file
 * PPTP�ǡ����μ��������ߤ� PAC(PPTP Access Concentrator) �Ȥ��Ƥμ���
 * �ΤߤǤ���
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdarg.h>
#include <signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <string.h>
#include <event.h>
#include <ifaddrs.h>

#ifdef USE_LIBSOCKUTIL
#include <seil/sockfromto.h>
#endif

#include "net_utils.h"
#include "bytebuf.h"
#include "debugutil.h"
#include "hash.h"
#include "slist.h"
#include "addr_range.h"
#include "properties.h"
#include "config_helper.h"
#ifdef _SEIL_EXT_
#include "rtev.h"
#endif

#include "pptp.h"
#include "pptp_local.h"

static int pptpd_seqno = 0;

#ifdef	PPTPD_DEBUG
#define	PPTPD_ASSERT(x)	ASSERT(x)
#define	PPTPD_DBG(x)	pptpd_log x
#else
#define	PPTPD_ASSERT(x)
#define	PPTPD_DBG(x)
#endif

static void      pptpd_log (pptpd *, int, const char *, ...) __printflike(3,4);
static void      pptpd_close_gre (pptpd *);
static void      pptpd_close_1723 (pptpd *);
static void      pptpd_io_event (int, short, void *);
static void      pptpd_gre_io_event (int, short, void *);
static void      pptpd_gre_input (pptpd_listener *, struct sockaddr *, u_char *, int);
static void      pptp_ctrl_start_by_pptpd (pptpd *, int, int, struct sockaddr *);
static int       pptp_call_cmp (const void *, const void *);
static uint32_t  pptp_call_hash (const void *, int);
static void      pptp_gre_header_string (struct pptp_gre_header *, char *, int);

#define	PPTPD_SHUFFLE_MARK	-1

/** PPTP�ǡ������������ޤ� */
int
pptpd_init(pptpd *_this)
{
	int i, m;
	struct sockaddr_in sin0;
	uint16_t call0, call[UINT16_MAX - 1];

	memset(_this, 0, sizeof(pptpd));
	_this->id = pptpd_seqno++;

	slist_init(&_this->listener);
	memset(&sin0, 0, sizeof(sin0));
	sin0.sin_len = sizeof(sin0);
	sin0.sin_family = AF_INET;
	if (pptpd_add_listener(_this, 0, PPTPD_DEFAULT_LAYER2_LABEL, 
	    (struct sockaddr *)&sin0) != 0) {
		return 1;
	}

	_this->ip4_allow = NULL;

	slist_init(&_this->ctrl_list);
	slist_init(&_this->call_free_list);

	/* Call-ID ����åե� */
	for (i = 0; i < countof(call) ; i++)
		call[i] = i + 1;
	for (i = countof(call); i > 1; i--) {
		m = random() % i;
		call0 = call[m];
		call[m] = call[i - 1];
		call[i - 1] = call0;
	}
	/* ɬ�׸Ĥ����� slist �� */
	for (i = 0; i < MIN(PPTP_MAX_CALL, countof(call)); i++)
		slist_add(&_this->call_free_list, (void *)(uintptr_t)call[i]);
	/* ������ SHUFFLE_MARK������� slist_shuffle �� shuflle ����� */
	slist_add(&_this->call_free_list, (void *)PPTPD_SHUFFLE_MARK);

	if (_this->call_id_map == NULL)
		_this->call_id_map = hash_create(pptp_call_cmp, pptp_call_hash,
		    0);

	return 0;
}

/**
 * {@link ::pptpd PPTP�ǡ����}��{@link ::pptpd_listener �ꥹ��}���ɲä��ޤ���
 * @param	_this	{@link ::pptpd PPTP�ǡ����}
 * @param	idx	�ꥹ�ʤΥ���ǥå���
 * @param	label	ʪ���ؤȤ��ƤΥ�٥롣"PPTP" �ʤ�
 * @param	bindaddr	�Ԥ������륢�ɥ쥹
 */
int
pptpd_add_listener(pptpd *_this, int idx, const char *label,
    struct sockaddr *bindaddr)
{
	int inaddr_any;
	pptpd_listener *plistener, *plstn;

	plistener = NULL;
	if (idx == 0 && slist_length(&_this->listener) > 0) {
		slist_itr_first(&_this->listener);
		while (slist_itr_has_next(&_this->listener)) {
			slist_itr_next(&_this->listener);
			plstn = slist_itr_remove(&_this->listener);
			PPTPD_ASSERT(plstn != NULL);
			PPTPD_ASSERT(plstn->sock == -1);
			PPTPD_ASSERT(plstn->sock_gre == -1);
			free(plstn);
		}
	}
	PPTPD_ASSERT(slist_length(&_this->listener) == idx);
	if (slist_length(&_this->listener) != idx) {
		pptpd_log(_this, LOG_ERR,
		    "Invalid argument error on %s(): idx must be %d but %d",
		    __func__, slist_length(&_this->listener), idx);
		goto reigai;
	}
	if ((plistener = malloc(sizeof(pptpd_listener))) == NULL) {
		pptpd_log(_this, LOG_ERR, "malloc() failed in %s: %m",
		    __func__);
		goto reigai;
	}
	memset(plistener, 0, sizeof(pptpd_listener));

	PPTPD_ASSERT(sizeof(plistener->bind_sin) >= bindaddr->sa_len);
	memcpy(&plistener->bind_sin, bindaddr, bindaddr->sa_len);
	memcpy(&plistener->bind_sin_gre, bindaddr, bindaddr->sa_len);

	/* �ݡ����ֹ椬��ά���줿���ϡ��ǥե���� (1723/tcp)��Ȥ� */
	if (plistener->bind_sin.sin_port == 0)
		plistener->bind_sin.sin_port = htons(PPTPD_DEFAULT_TCP_PORT);

	/*
	 * raw �����åȤǡ�INADDR_ANY ������Ū�� IP ���ɥ쥹���ꤷ�������å�ξ
	 * ���� bind ������硢�ѥ��åȤ�ξ���Υ����åȤǼ�������롣���ξ��֤�
	 * ȯ������ȡ��ѥ��åȤ���ʣ���Ƽ��������褦�˸����Ƥ��ޤ����ᡢ���Τ�
	 * ��������ϵ����ʤ����ȤȤ�����
	 */
	inaddr_any = 0;
	slist_itr_first(&_this->listener);
	while (slist_itr_has_next(&_this->listener)) {
		plstn = slist_itr_next(&_this->listener);
		if (plstn->bind_sin_gre.sin_addr.s_addr == INADDR_ANY)
			inaddr_any++;
	}
	if (plistener->bind_sin_gre.sin_addr.s_addr == INADDR_ANY)
		inaddr_any++;
	if (inaddr_any > 0 && idx > 0) {
		log_printf(LOG_ERR, "configuration error at pptpd.listener_in: "
		    "combination 0.0.0.0 and other address is not allowed.");
		goto reigai;
	}

	plistener->bind_sin_gre.sin_port = 0;
	plistener->sock = -1;
	plistener->sock_gre = -1;
	plistener->self = _this;
	plistener->index = idx;
	strlcpy(plistener->phy_label, label, sizeof(plistener->phy_label));

	if (slist_add(&_this->listener, plistener) == NULL) {
		pptpd_log(_this, LOG_ERR, "slist_add() failed in %s: %m",
		    __func__);
		goto reigai;
	}
	return 0;
reigai:
	if (plistener != NULL)
		free(plistener);
	return 1;
}

void
pptpd_uninit(pptpd *_this)
{
	pptpd_listener *plstn;

	slist_fini(&_this->ctrl_list);
	slist_fini(&_this->call_free_list);

	slist_itr_first(&_this->listener);
	while (slist_itr_has_next(&_this->listener)) {
		plstn = slist_itr_next(&_this->listener);
		PPTPD_ASSERT(plstn != NULL);
		PPTPD_ASSERT(plstn->sock == -1);
		PPTPD_ASSERT(plstn->sock_gre == -1);
		free(plstn);
	}
	slist_fini(&_this->listener);
	if (_this->call_id_map != NULL) {
		// �����ƥ�κ��?
		hash_free(_this->call_id_map);
	}
	if (_this->ip4_allow != NULL)
		in_addr_range_list_remove_all(&_this->ip4_allow);
	_this->call_id_map = NULL;
	_this->config = NULL;
}

#define	CALL_MAP_KEY(call)	\
	(void *)(call->id | (call->ctrl->listener_index << 16))
#define	CALL_ID(item)	((uint32_t)item & 0xffff)

/** PPTP�������Ƥޤ� */
int
pptpd_assign_call(pptpd *_this, pptp_call *call)
{
	int shuffle_cnt = 0, call_id;

	shuffle_cnt = 0;
	slist_itr_first(&_this->call_free_list);
	while (slist_length(&_this->call_free_list) > 1 &&
	    slist_itr_has_next(&_this->call_free_list)) {
		call_id = (int)slist_itr_next(&_this->call_free_list);
		if (call_id == 0)
			break;
		slist_itr_remove(&_this->call_free_list);
		if (call_id == PPTPD_SHUFFLE_MARK) {
			if (shuffle_cnt++ > 0)
				break;
			slist_shuffle(&_this->call_free_list);
			slist_add(&_this->call_free_list,
			    (void *)PPTPD_SHUFFLE_MARK);
			slist_itr_first(&_this->call_free_list);
			continue;
		}
		call->id = call_id;
		hash_insert(_this->call_id_map, CALL_MAP_KEY(call), call);

		return 0;
	}
	errno = EBUSY;
	pptpd_log(_this, LOG_ERR, "call request reached limit=%d",
	    PPTP_MAX_CALL);
	return -1;
}

/** PPTP��������ޤ���*/
void
pptpd_release_call(pptpd *_this, pptp_call *call)
{
	if (call->id != 0)
		slist_add(&_this->call_free_list, (void *)call->id);
	hash_delete(_this->call_id_map, CALL_MAP_KEY(call), 0);
	call->id = 0;
}

static int
pptpd_listener_start(pptpd_listener *_this)
{
	int sock, ival, sock_gre;
	struct sockaddr_in bind_sin, bind_sin_gre;
	int wildcardbinding;
#ifdef NPPPD_FAKEBIND
	extern void set_faith(int, int);
#endif

	wildcardbinding =
	    (_this->bind_sin.sin_addr.s_addr == INADDR_ANY)?  1 : 0;
	sock = -1;
	sock_gre = -1;
	memcpy(&bind_sin, &_this->bind_sin, sizeof(bind_sin));
	memcpy(&bind_sin_gre, &_this->bind_sin_gre, sizeof(bind_sin_gre));

	if (_this->phy_label[0] == '\0')
		strlcpy(_this->phy_label, PPTPD_DEFAULT_LAYER2_LABEL,
		    sizeof(_this->phy_label));
	// 1723/tcp
	if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		pptpd_log(_this->self, LOG_ERR, "socket() failed at %s(): %m",
		    __func__);
		goto reigai;
	}
	ival = 1;
	if(setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &ival, sizeof(ival)) < 0){
		pptpd_log(_this->self, LOG_WARNING,
		    "setsockopt(SO_REUSEPORT) failed at %s(): %m", __func__);
	}
#ifdef NPPPD_FAKEBIND
	if (!wildcardbinding)
		set_faith(sock, 1);
#endif
#if defined(IP_STRICT_RCVIF) && defined(USE_STRICT_RCVIF)
	ival = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_STRICT_RCVIF, &ival, sizeof(ival))
	    != 0)
		pptpd_log(_this->self, LOG_WARNING,
		    "%s(): setsockopt(IP_STRICT_RCVIF) failed: %m", __func__);
#endif
	if ((ival = fcntl(sock, F_GETFL, 0)) < 0) {
		pptpd_log(_this->self, LOG_ERR,
		    "fcntl(F_GET_FL) failed at %s(): %m", __func__);
		goto reigai;
	} else if (fcntl(sock, F_SETFL, ival | O_NONBLOCK) < 0) {
		pptpd_log(_this->self, LOG_ERR,
		    "fcntl(F_SET_FL) failed at %s(): %m", __func__);
		goto reigai;
	}
	if (bind(sock, (struct sockaddr *)&_this->bind_sin,
	    _this->bind_sin.sin_len) != 0) {
		pptpd_log(_this->self, LOG_ERR,
		    "bind(%s:%u) failed at %s(): %m",
		    inet_ntoa(_this->bind_sin.sin_addr),
		    ntohs(_this->bind_sin.sin_port), __func__);
		goto reigai;
	}
	if (listen(sock, PPTP_BACKLOG) != 0) {
		pptpd_log(_this->self, LOG_ERR,
		    "listen(%s:%u) failed at %s(): %m",
		    inet_ntoa(_this->bind_sin.sin_addr),
		    ntohs(_this->bind_sin.sin_port), __func__);
		goto reigai;
	}
#ifdef NPPPD_FAKEBIND
	if (!wildcardbinding)
		set_faith(sock, 0);
#endif
	pptpd_log(_this->self, LOG_INFO, "Listening %s:%u/tcp (PPTP PAC) [%s]",
	    inet_ntoa(_this->bind_sin.sin_addr),
	    ntohs(_this->bind_sin.sin_port), _this->phy_label);

	/* GRE */
	bind_sin_gre.sin_port = 0;
	if ((sock_gre = socket(AF_INET, SOCK_RAW, IPPROTO_GRE)) < 0) {
		pptpd_log(_this->self, LOG_ERR, "socket() failed at %s(): %m",
		    __func__);
		goto reigai;
	}
#ifdef NPPPD_FAKEBIND
	if (!wildcardbinding)
		set_faith(sock_gre, 1);
#endif
#if defined(IP_STRICT_RCVIF) && defined(USE_STRICT_RCVIF)
	ival = 1;
	if (setsockopt(sock_gre, IPPROTO_IP, IP_STRICT_RCVIF, &ival,
	    sizeof(ival)) != 0)
		pptpd_log(_this->self, LOG_WARNING,
		    "%s(): setsockopt(IP_STRICT_RCVIF) failed: %m", __func__);
#endif
	if ((ival = fcntl(sock_gre, F_GETFL, 0)) < 0) {
		pptpd_log(_this->self, LOG_ERR,
		    "fcntl(F_GET_FL) failed at %s(): %m", __func__);
		goto reigai;
	} else if (fcntl(sock_gre, F_SETFL, ival | O_NONBLOCK) < 0) {
		pptpd_log(_this->self, LOG_ERR,
		    "fcntl(F_SET_FL) failed at %s(): %m", __func__);
		goto reigai;
	}
	if (bind(sock_gre, (struct sockaddr *)&bind_sin_gre,
	    bind_sin_gre.sin_len) != 0) {
		pptpd_log(_this->self, LOG_ERR,
		    "bind(%s:%u) failed at %s(): %m",
		    inet_ntoa(bind_sin_gre.sin_addr),
		    ntohs(bind_sin_gre.sin_port), __func__);
		goto reigai;
	}
#ifdef NPPPD_FAKEBIND
	if (!wildcardbinding)
		set_faith(sock_gre, 0);
#endif
	if (wildcardbinding) {
#ifdef USE_LIBSOCKUTIL
		if (setsockoptfromto(sock) != 0) {
			pptpd_log(_this->self, LOG_ERR,
			    "setsockoptfromto() failed in %s(): %m", __func__);
			goto reigai;
		}
#else
		/* nothing to do */
#endif
	}
	pptpd_log(_this->self, LOG_INFO, "Listening %s:gre (PPTP PAC)",
	    inet_ntoa(bind_sin_gre.sin_addr));

	_this->sock = sock;
	_this->sock_gre = sock_gre;

	event_set(&_this->ev_sock, _this->sock, EV_READ | EV_PERSIST,
	    pptpd_io_event, _this);
	event_add(&_this->ev_sock, NULL);

	event_set(&_this->ev_sock_gre, _this->sock_gre, EV_READ | EV_PERSIST,
	    pptpd_gre_io_event, _this);
	event_add(&_this->ev_sock_gre, NULL);

	return 0;
reigai:
	if (sock >= 0)
		close(sock);
	if (sock_gre >= 0)
		close(sock_gre);

	_this->sock = -1;
	_this->sock_gre = -1;

	return 1;
}
/** PPTP�ǡ����򳫻Ϥ��ޤ� */
int
pptpd_start(pptpd *_this)
{
	int rval = 0;
	pptpd_listener *plistener;

	slist_itr_first(&_this->listener);
	while (slist_itr_has_next(&_this->listener)) {
		plistener = slist_itr_next(&_this->listener);
		PPTPD_ASSERT(plistener != NULL);
		rval |= pptpd_listener_start(plistener);
	}
	if (rval == 0)
		_this->state = PPTPD_STATE_RUNNING;

	return rval;
}

static void
pptpd_listener_close_gre(pptpd_listener *_this)
{
	if (_this->sock_gre >= 0) {
		event_del(&_this->ev_sock_gre);
		close(_this->sock_gre);
		pptpd_log(_this->self, LOG_INFO, "Shutdown %s/gre",
		    inet_ntoa(_this->bind_sin_gre.sin_addr));
	}
	_this->sock_gre = -1;
}

/** GRE���Ԥ����������åȤ� close ���ޤ� */
static void
pptpd_close_gre(pptpd *_this)
{
	pptpd_listener *plistener;

	slist_itr_first(&_this->listener);
	while (slist_itr_has_next(&_this->listener)) {
		plistener = slist_itr_next(&_this->listener);
		pptpd_listener_close_gre(plistener);
	}
}

/** 1723/tcp ���Ԥ����������åȤ� close ���ޤ� */
static void
pptpd_listener_close_1723(pptpd_listener *_this)
{
	if (_this->sock >= 0) {
		event_del(&_this->ev_sock);
		close(_this->sock);
		pptpd_log(_this->self, LOG_INFO, "Shutdown %s:%u/tcp",
		    inet_ntoa(_this->bind_sin.sin_addr),
		    ntohs(_this->bind_sin.sin_port));
	}
	_this->sock = -1;
}
/** 1723/tcp ���Ԥ����������åȤ� close ���ޤ� */
static void
pptpd_close_1723(pptpd *_this)
{
	pptpd_listener *plistener;

	slist_itr_first(&_this->listener);
	while (slist_itr_has_next(&_this->listener)) {
		plistener = slist_itr_next(&_this->listener);
		pptpd_listener_close_1723(plistener);
	}
}

/** PPTP�ǡ����������˽�λ���ޤ���**/
void
pptpd_stop_immediatly(pptpd *_this)
{
	pptp_ctrl *ctrl;

	if (event_initialized(&_this->ev_timer))
		evtimer_del(&_this->ev_timer);
	if (_this->state != PPTPD_STATE_STOPPED) {
		/*
		 * pptp_ctrl_stop ��ƤӽФ��ȡ����δؿ������ٸƤФ���ǽ
		 * �������롣���Τ��ᡢ����state �ѹ��Ͻ��ס�
		 */
		_this->state = PPTPD_STATE_STOPPED;

		pptpd_close_1723(_this);
		for (slist_itr_first(&_this->ctrl_list);
		    (ctrl = slist_itr_next(&_this->ctrl_list)) != NULL;) {
			pptp_ctrl_stop(ctrl, 0);
		}
		pptpd_close_gre(_this);
		slist_fini(&_this->ctrl_list);
		slist_fini(&_this->call_free_list);
		pptpd_log(_this, LOG_NOTICE, "Stopped");
	} else {
		PPTPD_DBG((_this, LOG_DEBUG, "(Already) Stopped"));
	}
}

static void
pptpd_stop_timeout(int fd, short event, void *ctx)
{
	pptpd *_this;

	_this = ctx;
	pptpd_stop_immediatly(_this);
}

/** PPTP�ǡ�����λ���ޤ� */
void
pptpd_stop(pptpd *_this)
{
	int nctrl;
	pptp_ctrl *ctrl;
	struct timeval tv;

	if (event_initialized(&_this->ev_timer))
		evtimer_del(&_this->ev_timer);
	pptpd_close_1723(_this);
	/* ���Τ������ư��� l2tpd_stop �Ȥ��碌��٤� */

	if (pptpd_is_stopped(_this))
		return;
	if (pptpd_is_shutting_down(_this)) {
		pptpd_stop_immediatly(_this);
		return;
	}
	_this->state = PPTPD_STATE_SHUTTING_DOWN;
	nctrl = 0;
	for (slist_itr_first(&_this->ctrl_list);
	    (ctrl = slist_itr_next(&_this->ctrl_list)) != NULL;) {
		pptp_ctrl_stop(ctrl, PPTP_CDN_RESULT_ADMIN_SHUTDOWN);
		nctrl++;
	}
	if (nctrl > 0) {
		// �����ޡ����å�
		tv.tv_sec = PPTPD_SHUTDOWN_TIMEOUT;
		tv.tv_usec = 0;

		evtimer_set(&_this->ev_timer, pptpd_stop_timeout, _this);
		evtimer_add(&_this->ev_timer, &tv);

		return;
	}
	pptpd_stop_immediatly(_this);
}

/***********************************************************************
 * �����Ϣ
 ***********************************************************************/
#define	CFG_KEY(p, s)	config_key_prefix((p), (s))
#define	VAL_SEP		" \t\r\n"

CONFIG_FUNCTIONS(pptpd_config, pptpd, config);
PREFIXED_CONFIG_FUNCTIONS(pptp_ctrl_config, pptp_ctrl, pptpd->config,
    phy_label);
int
pptpd_reload(pptpd *_this, struct properties *config, const char *name,
    int default_enabled)
{
	int i, do_start, aierr;
	const char *val;
	char *tok, *cp, buf[PPTPD_CONFIG_BUFSIZ], *label;
	struct addrinfo *ai;

	ASSERT(_this != NULL);
	ASSERT(config != NULL);

	_this->config = config;	/* ���ߤ� copy ���ʤ�������� */
	do_start = 0;
	if (pptpd_config_str_equal(_this, CFG_KEY(name, "enabled"), "true", 
	    default_enabled)) {
		// false �ˤ���ľ��� true �ˤ���뤫�⤷��ʤ���
		if (pptpd_is_shutting_down(_this)) 
			pptpd_stop_immediatly(_this);
		if (pptpd_is_stopped(_this))
			do_start = 1;
	} else {
		if (!pptpd_is_stopped(_this))
			pptpd_stop(_this);
		return 0;
	}
	if (do_start && pptpd_init(_this) != 0)
		return 1;
	// pptpd_init �ǥꥻ�åȤ���Ƥ��ޤ��Τǡ�
	_this->config = config;

	_this->ctrl_in_pktdump = pptpd_config_str_equal(_this,
	    "log.pptp.ctrl.in.pktdump", "true", 0);
	_this->data_in_pktdump = pptpd_config_str_equal(_this,
	    "log.pptp.data.in.pktdump", "true", 0);
	_this->ctrl_out_pktdump = pptpd_config_str_equal(_this,
	    "log.pptp.ctrl.out.pktdump", "true", 0);
	_this->data_out_pktdump = pptpd_config_str_equal(_this,
	    "log.pptp.data.out.pktdump", "true", 0);
	_this->phy_label_with_ifname = pptpd_config_str_equal(_this,
	    CFG_KEY(name, "label_with_ifname"), "true", 0);

	// ip4_allow ��ѡ���
	in_addr_range_list_remove_all(&_this->ip4_allow);
	val = pptpd_config_str(_this, CFG_KEY(name, "ip4_allow"));
	if (val != NULL) {
		if (strlen(val) >= sizeof(buf)) {
			log_printf(LOG_ERR, "configuration error at "
			    "%s: too long", CFG_KEY(name, "ip4_allow"));
			return 1;
		}
		strlcpy(buf, val, sizeof(buf));
		for (cp = buf; (tok = strsep(&cp, VAL_SEP)) != NULL;) {
			if (*tok == '\0')
				continue;
			if (in_addr_range_list_add(&_this->ip4_allow, tok)
			    != 0) {
				pptpd_log(_this, LOG_ERR,
				    "configuration error at %s: %s",
				    CFG_KEY(name, "ip4_allow"), tok);
				return 1;
			}
		}
	}

	if (do_start) {
		/*
		 * ��ưľ��ȡ�pptpd.enable �� false -> true ���ѹ����줿
		 * ���ˡ�do_start�����٤ƤΥꥹ�ʡ�������������줿���֤�
		 * ����Ǥ���
		 */
		// pptpd.listener_in ���ɤ߹���
		val = pptpd_config_str(_this, CFG_KEY(name, "listener_in"));
		if (val != NULL) {
			if (strlen(val) >= sizeof(buf)) {
				pptpd_log(_this, LOG_ERR,
				    "configuration error at "
				    "%s: too long", CFG_KEY(name, "listener"));
				return 1;
			}
			strlcpy(buf, val, sizeof(buf));

			label = NULL;
			// ���֡����ڡ������ڤ�ǡ�ʣ�������ǽ
			for (i = 0, cp = buf;
			    (tok = strsep(&cp, VAL_SEP)) != NULL;) {
				if (*tok == '\0')
					continue;
				if (label == NULL) {
					label = tok;
					continue;
				}
				if ((aierr = addrport_parse(tok, IPPROTO_TCP,
				    &ai)) != 0) {
					pptpd_log(_this, LOG_ERR,
					    "configuration error at "
					    "%s: %s: %s",
					    CFG_KEY(name, "listener_in"), tok, 
					    gai_strerror(aierr));
					return 1;
				}
				PPTPD_ASSERT(ai != NULL &&
				    ai->ai_family == AF_INET);
				if (pptpd_add_listener(_this, i, label,
				    ai->ai_addr) != 0) {
					freeaddrinfo(ai);
					label = NULL;
					break;
				}
				freeaddrinfo(ai);
				label = NULL;
				i++;
			}
			if (label != NULL) {
				pptpd_log(_this, LOG_ERR,
				    "configuration error at %s: %s",
				    CFG_KEY(name, "listner_in"), label);
				return 1;
			}
		}
		if (pptpd_start(_this) != 0)
			return 1;
	}

	return 0;
}

/***********************************************************************
 * I/O��Ϣ
 ***********************************************************************/
static void
pptpd_log_access_deny(pptpd *_this, const char *reason, struct sockaddr *peer)
{
	char hostbuf[NI_MAXHOST], servbuf[NI_MAXSERV];

	if (getnameinfo(peer, peer->sa_len, hostbuf, sizeof(hostbuf),
	    servbuf, sizeof(servbuf), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
		pptpd_log(_this, LOG_ERR, "getnameinfo() failed at %s(): %m",
		    __func__);
		return;
	}
	pptpd_log(_this, LOG_ALERT, "denied a connection from %s:%s/tcp: %s",
	    hostbuf, servbuf, reason);
}

/** 1723/tcp �� IO���٥�ȥϥ�ɥ� */
static void
pptpd_io_event(int fd, short evmask, void *ctx)
{
	int newsock;
	const char *reason;
	socklen_t peerlen;
	struct sockaddr_storage peer;
	pptpd *_this;
	pptpd_listener *listener;

	listener = ctx;
	PPTPD_ASSERT(listener != NULL);
	_this = listener->self;
	PPTPD_ASSERT(_this != NULL);

	if ((evmask & EV_READ) != 0) {
		for (;;) {	// EAGAIN �ޤ� Ϣ³���� accept
			peerlen = sizeof(peer);
			if ((newsock = accept(listener->sock,
			    (struct sockaddr *)&peer, &peerlen)) < 0) {
				switch (errno) {
				case EAGAIN:
				case EINTR:
					break;
				case ECONNABORTED:
					pptpd_log(_this, LOG_WARNING,
					    "accept() failed at %s(): %m",
					    __func__);
					break;
				default:
					pptpd_log(_this, LOG_ERR,
					    "accept() failed at %s(): %m",
						__func__);
					pptpd_listener_close_1723(listener);
					pptpd_stop(_this);
				}
				break;
			}
		// �����������å�
			switch (peer.ss_family) {
			case AF_INET:
				if (!in_addr_range_list_includes(
				    &_this->ip4_allow,
				    &((struct sockaddr_in *)&peer)->sin_addr)) {
					reason = "not allowed by acl.";
					break;
				}
				goto accept;
			default:
				reason = "address family is not supported.";
				break;
			}
		// ���Ĥ���Ƥ��ʤ�
			pptpd_log_access_deny(_this, reason,
			    (struct sockaddr *)&peer);
			close(newsock);
			continue;
			// NOTREACHED 
accept:
		// ����
			pptp_ctrl_start_by_pptpd(_this, newsock,
			    listener->index, (struct sockaddr *)&peer);
		}
	}
}

/** GRE �� IO���٥�ȥϥ�ɥ顼 */
static void
pptpd_gre_io_event(int fd, short evmask, void *ctx)
{
	int sz;
	u_char pkt[65535];
	socklen_t peerlen;
	struct sockaddr_storage peer;
	pptpd *_this;
	pptpd_listener *listener;

	listener = ctx;
	PPTPD_ASSERT(listener != NULL);
	_this = listener->self;
	PPTPD_ASSERT(_this != NULL);

	if (evmask & EV_READ) {
		for (;;) {
			// Block ����ޤ��ɤ�
			peerlen = sizeof(peer);
			if ((sz = recvfrom(listener->sock_gre, pkt, sizeof(pkt),
			    0, (struct sockaddr *)&peer, &peerlen)) <= 0) {
				if (sz < 0 &&
				    (errno == EAGAIN || errno == EINTR))
					break;
				pptpd_log(_this, LOG_INFO,
				    "read(GRE) failed: %m");
				pptpd_stop(_this);
				return;
			}
			pptpd_gre_input(listener, (struct sockaddr *)&peer, pkt,
			    sz);
		}
	}
}

/** GRE�μ��� �� pptp_call ������ */
static void
pptpd_gre_input(pptpd_listener *listener, struct sockaddr *peer, u_char *pkt,
    int lpkt)
{
	int hlen, input_flags;
	uint32_t seq, ack, call_id;
	struct ip *iphdr;
	struct pptp_gre_header *grehdr;
	char hbuf0[NI_MAXHOST], logbuf[512];
	const char *reason;
	pptp_call *call;
	hash_link *hl;
	pptpd *_this;

	seq = 0;
	ack = 0;
	input_flags = 0;
	reason = "No error";
	_this = listener->self;

	PPTPD_ASSERT(peer->sa_family == AF_INET);

	strlcpy(hbuf0, "<unknown>", sizeof(hbuf0));
	if (getnameinfo(peer, peer->sa_len, hbuf0, sizeof(hbuf0), NULL, 0,
	    NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
		pptpd_log(_this, LOG_ERR,
		    "getnameinfo() failed at %s(): %m", __func__);
		goto reigai;
	}
	if (_this->data_in_pktdump != 0) {
		pptpd_log(_this, LOG_DEBUG, "PPTP Data input packet dump");
		show_hd(debug_get_debugfp(), pkt, lpkt);
	}
	if (peer->sa_family != AF_INET) {
		pptpd_log(_this, LOG_ERR,
		    "Received malformed GRE packet: address family is not "
		    "supported: peer=%s af=%d", hbuf0, peer->sa_family);
		goto reigai;
	}

	if (lpkt < sizeof(struct ip)) {
		pptpd_log(_this, LOG_ERR,
		    "Received a short length packet length=%d, from %s", lpkt,
			hbuf0);
		goto reigai;
	}
	iphdr = (struct ip *)pkt;

	// IP�إå��� ntohs �Ѥ� NetBSD �ξ��
#if !defined(__NetBSD__)
	iphdr->ip_len = ntohs(iphdr->ip_len);
#endif
	hlen = iphdr->ip_hl * 4;

	if (iphdr->ip_len > lpkt ||
	    iphdr->ip_len < sizeof(struct pptp_gre_header)) {
		pptpd_log(_this, LOG_ERR,
		    "Received a broken packet: ip_hl=%d iplen=%d lpkt=%d", hlen,
			iphdr->ip_len, lpkt);
		show_hd(debug_get_debugfp(), pkt, lpkt);
		goto reigai;
	}
	pkt += hlen;
	lpkt -= hlen;
	grehdr = (struct pptp_gre_header *)pkt;
	pkt += sizeof(struct pptp_gre_header);
	lpkt -= sizeof(struct pptp_gre_header);

	grehdr->protocol_type = htons(grehdr->protocol_type);
	grehdr->payload_length = htons(grehdr->payload_length);
	grehdr->call_id = htons(grehdr->call_id);

	if (!(grehdr->protocol_type == PPTP_GRE_PROTOCOL_TYPE &&
	    grehdr->C == 0 && grehdr->R == 0 && grehdr->K != 0 &&
	    grehdr->recur == 0 && grehdr->s == 0 && grehdr->flags == 0 &&
	    grehdr->ver == PPTP_GRE_VERSION)) {
		reason = "GRE header is broken";
		goto bad_gre;
	}
	if (grehdr->S != 0) {
		if (lpkt < 2) {
			reason = "No enough space for seq number";
			goto bad_gre;
		}
		input_flags |= PPTP_GRE_PKT_SEQ_PRESENT;
		seq = ntohl(*(uint32_t *)pkt);
		pkt += 4;
		lpkt -= 4;
	}

	if (grehdr->A != 0) {
		if (lpkt < 2) {
			reason = "No enough space for ack number";
			goto bad_gre;
		}
		input_flags |= PPTP_GRE_PKT_ACK_PRESENT;
		ack = ntohl(*(uint32_t *)pkt);
		pkt += 4;
		lpkt -= 4;
	}

	if (grehdr->payload_length > lpkt) {
		reason = "'Payload Length' is mismatch from actual length";
		goto bad_gre;
	}


	// pptp_call ������ 
	call_id = grehdr->call_id;

	hl = hash_lookup(_this->call_id_map,
	    (void *)(call_id | (listener->index << 16)));
	if (hl == NULL) {
		reason = "Received GRE packet has unknown call_id";
		goto bad_gre;
	}
	call = hl->item;
	pptp_call_gre_input(call, seq, ack, input_flags, pkt, lpkt);

	return;
bad_gre:
	pptp_gre_header_string(grehdr, logbuf, sizeof(logbuf));
	pptpd_log(_this, LOG_INFO,
	    "Received malformed GRE packet: %s: peer=%s sock=%s %s seq=%u: "
	    "ack=%u ifidx=%d", reason, hbuf0, inet_ntoa(iphdr->ip_dst), logbuf,
	    seq, ack, listener->index);
reigai:
	return;
}

/** PPTP����ȥ���򳫻Ϥ��ޤ���(��������³������иƤӽФ���롣) */
static void
pptp_ctrl_start_by_pptpd(pptpd *_this, int sock, int listener_index,
    struct sockaddr *peer)
{
	int ival;
	pptp_ctrl *ctrl;
	socklen_t sslen;
	char ifname[IF_NAMESIZE], msgbuf[128];

	ctrl = NULL;
	if ((ctrl = pptp_ctrl_create()) == NULL)
		goto reigai;
	if (pptp_ctrl_init(ctrl) != 0)
		goto reigai;

	memset(&ctrl->peer, 0, sizeof(ctrl->peer));
	memcpy(&ctrl->peer, peer, peer->sa_len);
	ctrl->pptpd = _this;
	ctrl->sock = sock;
	ctrl->listener_index = listener_index;

	sslen = sizeof(ctrl->our);
	if (getsockname(ctrl->sock, (struct sockaddr *)&ctrl->our,
	    &sslen) != 0) {
		pptpd_log(_this, LOG_WARNING,
		    "getsockname() failed at %s(): %m", __func__);
		goto reigai;
	}
	/* "L2TP%em0.mru" �ʤɤȡ����󥿥ե�������������ѹ������� */
	if (_this->phy_label_with_ifname != 0) {
		if (get_ifname_by_sockaddr((struct sockaddr *)&ctrl->our,
		    ifname) == NULL) {
			pptpd_log_access_deny(_this,
			    "could not get interface informations", peer);
			goto reigai;
		}
		if (pptpd_config_str_equal(_this, 
		    config_key_prefix("pptpd.interface", ifname), "accept", 0)){
			snprintf(ctrl->phy_label, sizeof(ctrl->phy_label),
			    "%s%%%s", PPTP_CTRL_LISTENER_LABEL(ctrl), ifname);
		} else if (pptpd_config_str_equal(_this, 
		    config_key_prefix("pptpd.interface", "any"), "accept", 0)){
			snprintf(ctrl->phy_label, sizeof(ctrl->phy_label),
			    "%s", PPTP_CTRL_LISTENER_LABEL(ctrl));
		} else {
			/* ���Υ��󥿥ե������ϵ��Ĥ���Ƥ��ʤ���*/
			snprintf(msgbuf, sizeof(msgbuf),
			    "'%s' is not allowed by config.", ifname);
			pptpd_log_access_deny(_this, msgbuf, peer);
			goto reigai;
		}
#if defined(_SEIL_EXT_) && !defined(USE_LIBSOCKUTIL)
		if (!rtev_ifa_is_primary(ifname,
		    (struct sockaddr *)&ctrl->our)) {
			char hostbuf[NI_MAXHOST];

			getnameinfo((struct sockaddr *)&ctrl->our,
			    ctrl->our.ss_len, hostbuf,
			    sizeof(hostbuf), NULL, 0, NI_NUMERICHOST);
			snprintf(msgbuf, sizeof(msgbuf),
			    "connecting to %s (an alias address of %s)"
			    " is not allowed by this version.",
			    hostbuf, ifname);
			pptpd_log_access_deny(_this, msgbuf, peer);

			goto reigai;
		}
#endif
	} else 
		strlcpy(ctrl->phy_label, PPTP_CTRL_LISTENER_LABEL(ctrl),
		    sizeof(ctrl->phy_label));

	if ((ival = pptp_ctrl_config_int(ctrl, "pptp.echo_interval", 0)) != 0)
		ctrl->echo_interval = ival;

	if ((ival = pptp_ctrl_config_int(ctrl, "pptp.echo_timeout", 0)) != 0)
		ctrl->echo_timeout = ival;

	if (pptp_ctrl_start(ctrl) != 0)
		goto reigai;

	slist_add(&_this->ctrl_list, ctrl);

	return;
reigai:
	close(sock);
	pptp_ctrl_destroy(ctrl);
	return;
}

/** PPTP����ȥ��뤬��λ������Τ��Ƥ��ޤ���*/
void
pptpd_ctrl_finished_notify(pptpd *_this, pptp_ctrl *ctrl)
{
	pptp_ctrl *ctrl1;
	int i, nctrl;

	PPTPD_ASSERT(_this != NULL);
	PPTPD_ASSERT(ctrl != NULL);

	nctrl = 0;
	for (i = 0; i < slist_length(&_this->ctrl_list); i++) {
		ctrl1 = slist_get(&_this->ctrl_list, i);
		if (ctrl1 == ctrl) {
			slist_remove(&_this->ctrl_list, i);
			break;
		}
	}
	pptp_ctrl_destroy(ctrl);

	PPTPD_DBG((_this, LOG_DEBUG, "Remains %d ctrls", nctrl));
	if (pptpd_is_shutting_down(_this) && nctrl == 0)
	// ����åȥ�������Ǹ�ΰ��
		pptpd_stop_immediatly(_this);
}

/***********************************************************************
 * ����¾���桼�ƥ���ƥ��ؿ�
 ***********************************************************************/
/** ���Υ��󥹥��󥹤˴�Ť�����٥뤫��Ϥޤ����Ͽ���ޤ��� */
static void
pptpd_log(pptpd *_this, int prio, const char *fmt, ...)
{
	char logbuf[BUFSIZ];
	va_list ap;

	PPTPD_ASSERT(_this != NULL);
	va_start(ap, fmt);
#ifdef	PPTPD_MULITPLE
	snprintf(logbuf, sizeof(logbuf), "pptpd id=%u %s", _this->id, fmt);
#else
	snprintf(logbuf, sizeof(logbuf), "pptpd %s", fmt);
#endif
	vlog_printf(prio, logbuf, ap);
	va_end(ap);
}

static int
pptp_call_cmp(const void *a0, const void *b0)
{
	return ((uint32_t)a0 - (uint32_t)b0);
}

static uint32_t
pptp_call_hash(const void *ctx, int size)
{
	return (uint32_t)ctx % size;
}

/** GRE�ѥ��åȥإå���ʸ����Ȥ��� */
static void
pptp_gre_header_string(struct pptp_gre_header *grehdr, char *buf, int lbuf)
{
	snprintf(buf, lbuf,
	    "[%s%s%s%s%s%s] ver=%d "
	    "protocol_type=%04x payload_length=%d call_id=%d",
	    (grehdr->C != 0)? "C" : "", (grehdr->R != 0)? "R" : "",
	    (grehdr->K != 0)? "K" : "", (grehdr->S != 0)? "S" : "",
	    (grehdr->s != 0)? "s" : "", (grehdr->A != 0)? "A" : "", grehdr->ver,
	    grehdr->protocol_type, grehdr->payload_length, grehdr->call_id);
}
