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
/**@file
 * L2TP(Layer Two Tunneling Protocol "L2TP") �μ���
 */
/*
 * RFC 2661
 */
// $Id: l2tpd.c 39106 2010-01-10 21:01:39Z yasuoka $
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#if 0
#include <netinet6/ipsec.h>
#endif
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <event.h>

#ifdef USE_LIBSOCKUTIL
#include <seil/sockfromto.h>
#else
#include "recvfromto.h"
#endif

#include "bytebuf.h"
#include "hash.h"
#include "slist.h"
#include "debugutil.h"
#include "l2tp.h"
#include "l2tp_subr.h"
#include "l2tp_local.h"
#include "addr_range.h"
#include "properties.h"
#include "config_helper.h"
#include "net_utils.h"

#ifdef	L2TPD_DEBUG
#define	L2TPD_ASSERT(x)	ASSERT(x)
#define	L2TPD_DBG(x)	l2tpd_log x
#else
#define	L2TPD_ASSERT(x)
#endif
#define L2TPD_IPSEC_POLICY_IN	"in ipsec esp/transport//require"
#define L2TPD_IPSEC_POLICY_OUT	"out ipsec esp/transport//require"

static void             l2tpd_io_event (int, short, void *);
static inline int       short_cmp (const void *, const void *);
static inline uint32_t  short_hash (const void *, int);
/*
 * static �ѿ�
 */

/** l2tpd �� ID�ֹ�Υ��������ֹ� */
static unsigned l2tpd_id_seq = 0;

#ifndef USE_LIBSOCKUTIL
struct in_ipsec_sa_cookie	{	};
#endif


/***********************************************************************
 * L2TP �ǡ���󥤥󥹥������
 ***********************************************************************/

/**
 * L2TP�ǡ���󥤥󥹥��󥹤��������ޤ���
 * <p>
 * {@link _l2tpd#bind_sin} �ϡ�.sin_family = AF_INET��.sin_port = 1701��
 * .sin_len �����ꤵ�줿���֤��֤�ޤ��� </p>
 */
int
l2tpd_init(l2tpd *_this)
{
	struct sockaddr_in sin0;

	L2TPD_ASSERT(_this != NULL);
	memset(_this, 0, sizeof(l2tpd));

	slist_init(&_this->listener);
	memset(&sin0, 0, sizeof(sin0));
	sin0.sin_len = sizeof(sin0);
	sin0.sin_family = AF_INET;
	if (l2tpd_add_listener(_this, 0, L2TPD_DEFAULT_LAYER2_LABEL, 
	    (struct sockaddr *)&sin0) != 0) {
		return 1;
	}

	_this->id = l2tpd_id_seq++;

	if ((_this->ctrl_map = hash_create(short_cmp, short_hash,
	    L2TPD_TUNNEL_HASH_SIZ)) == NULL) {
		log_printf(LOG_ERR, "hash_create() failed in %s(): %m",
		    __func__);
		return 1;
	}
	_this->ip4_allow = NULL;

	_this->require_ipsec = 1;
	_this->purge_ipsec_sa = 1;
	_this->state = L2TPD_STATE_INIT;

	return 0;
}

/**
 * {@link ::l2tpd L2TP�ǡ����}��{@link ::l2tpd_listener �ꥹ��}���ɲä��ޤ���
 * @param	_this	{@link ::l2tpd L2TP�ǡ����}
 * @param	idx	�ꥹ�ʤΥ���ǥå���
 * @param	label	ʪ���ؤȤ��ƤΥ�٥롣"L2TP" �ʤ�
 * @param	bindaddr	�Ԥ������륢�ɥ쥹
 */
int
l2tpd_add_listener(l2tpd *_this, int idx, const char *label,
    struct sockaddr *bindaddr)
{
	l2tpd_listener *plistener, *plsnr;

	plistener = NULL;
	if (idx == 0 && slist_length(&_this->listener) > 0) {
		slist_itr_first(&_this->listener);
		while (slist_itr_has_next(&_this->listener)) {
			slist_itr_next(&_this->listener);
			plsnr = slist_itr_remove(&_this->listener);
			L2TPD_ASSERT(plsnr != NULL);
			L2TPD_ASSERT(plsnr->sock == -1);
			free(plsnr);
		}
	}
	L2TPD_ASSERT(slist_length(&_this->listener) == idx);
	if (slist_length(&_this->listener) != idx) {
		l2tpd_log(_this, LOG_ERR,
		    "Invalid argument error on %s(): idx must be %d but %d",
		    __func__, slist_length(&_this->listener), idx);
		goto reigai;
	}
	if ((plistener = malloc(sizeof(l2tpd_listener))) == NULL) {
		l2tpd_log(_this, LOG_ERR, "malloc() failed in %s: %m",
		    __func__);
		goto reigai;
	}
	memset(plistener, 0, sizeof(l2tpd_listener));
	L2TPD_ASSERT(sizeof(plistener->bind_sin) >= bindaddr->sa_len);
	memcpy(&plistener->bind_sin, bindaddr, bindaddr->sa_len);

	/* �ݡ����ֹ椬��ά���줿���ϡ��ǥե���� (1701/udp)��Ȥ� */
	if (plistener->bind_sin.sin_port == 0)
		plistener->bind_sin.sin_port = htons(L2TPD_DEFAULT_UDP_PORT);

	plistener->sock = -1;
	plistener->self = _this;
	plistener->index = idx;
	strlcpy(plistener->phy_label, label, sizeof(plistener->phy_label));

	if (slist_add(&_this->listener, plistener) == NULL) {
		l2tpd_log(_this, LOG_ERR, "slist_add() failed in %s: %m",
		    __func__);
		goto reigai;
	}
	return 0;
reigai:
	if (plistener != NULL)
		free(plistener);
	return 1;
}

/** L2TP�ǡ���󥤥󥹥��󥹤ν�λ������Ԥ��ޤ���*/
void
l2tpd_uninit(l2tpd *_this)
{
	l2tpd_listener *plsnr;

	L2TPD_ASSERT(_this != NULL);

	if (_this->ctrl_map != NULL) {
		hash_free(_this->ctrl_map);
		_this->ctrl_map = NULL;
	}

	if (_this->ip4_allow != NULL)
		in_addr_range_list_remove_all(&_this->ip4_allow);

	slist_itr_first(&_this->listener);
	while (slist_itr_has_next(&_this->listener)) {
		plsnr = slist_itr_next(&_this->listener);
		L2TPD_ASSERT(plsnr != NULL);
		L2TPD_ASSERT(plsnr->sock == -1);
		free(plsnr);
	}
	slist_fini(&_this->listener);

	event_del(&_this->ev_timeout);	// �ͤ�Τ���
	_this->state = L2TPD_STATE_STOPPED;
	_this->config = NULL;
}

/** �Ԥ������򳫻Ϥ��ޤ���*/
static int
l2tpd_listener_start(l2tpd_listener *_this, char *ipsec_policy_in,
    char *ipsec_policy_out)
{
	int sock, ival;
	l2tpd *_l2tpd;
#ifdef NPPPD_FAKEBIND
	int wildcardbinding = 0;
	extern void set_faith(int, int);

	wildcardbinding =
	    (_this->bind_sin.sin_addr.s_addr == INADDR_ANY)?  1 : 0;
#endif
	sock = -1;
	_l2tpd = _this->self;

	if (_this->phy_label[0] == '\0')
		strlcpy(_this->phy_label, L2TPD_DEFAULT_LAYER2_LABEL,
		    sizeof(_this->phy_label));
	if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		l2tpd_log(_l2tpd, LOG_ERR,
		    "socket() failed in %s(): %m", __func__);
		goto reigai;
	}
#ifdef NPPPD_FAKEBIND
	if (!wildcardbinding)
		set_faith(sock, 1);
#endif
#if defined(IP_STRICT_RCVIF) && defined(USE_STRICT_RCVIF)
	ival = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_STRICT_RCVIF, &ival, sizeof(ival))
	    != 0)
		l2tpd_log(_l2tpd, LOG_WARNING,
		    "%s(): setsockopt(IP_STRICT_RCVIF) failed: %m", __func__);
#endif
	if ((ival = fcntl(sock, F_GETFL, 0)) < 0) {
		l2tpd_log(_l2tpd, LOG_ERR,
		    "fcntl(,F_GETFL) failed in %s(): %m", __func__);
		goto reigai;
	} else if (fcntl(sock, F_SETFL, ival | O_NONBLOCK) < 0) {
		l2tpd_log(_l2tpd, LOG_ERR, "fcntl(,F_SETFL,O_NONBLOCK) failed "
		    "in %s(): %m", __func__);
		goto reigai;
	}
	ival = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &ival, sizeof(ival))
	    != 0) {
		l2tpd_log(_l2tpd, LOG_ERR,
		    "setsockopt(,,SO_REUSEPORT) failed in %s(): %m", __func__);
		goto reigai;
	}
	if (bind(sock, (struct sockaddr *)&_this->bind_sin,
	    _this->bind_sin.sin_len) != 0) {
		l2tpd_log(_l2tpd, LOG_ERR, "Binding %s:%u/udp: %m",
		    inet_ntoa(_this->bind_sin.sin_addr),
		    ntohs(_this->bind_sin.sin_port));
		goto reigai;
	}
#ifdef NPPPD_FAKEBIND
	if (!wildcardbinding)
		set_faith(sock, 0);
#endif
#ifdef UDP_NO_CKSUM
	ival = 1;
	if (setsockopt(sock, IPPROTO_UDP, UDP_NO_CKSUM, &ival, sizeof(ival))
	    != 0) {
		l2tpd_log(_l2tpd, LOG_ERR,
		    "setsockopt(,,UDP_NO_CKSUM) failed in %s(): %m",
		    __func__);
		goto reigai;
	}
#endif
#ifdef USE_LIBSOCKUTIL
	if (setsockoptfromto(sock) != 0) {
		l2tpd_log(_l2tpd, LOG_ERR,
		    "setsockoptfromto() failed in %s(): %m", __func__);
		goto reigai;
	}
#else
	// recvfromto �Τ����
	ival = 1;
	if (setsockopt(sock, IPPROTO_IP, IP_RECVDSTADDR, &ival, sizeof(ival))
	    != 0) {
		l2tpd_log(_l2tpd, LOG_ERR,
		    "setsockopt(,,IP_RECVDSTADDR) failed in %s(): %m",
		    __func__);
		goto reigai;
	}
#endif
#ifdef IP_IPSEC_POLICY
/*XXX */
	if (ipsec_policy_in != NULL &&
	    setsockopt(sock, IPPROTO_IP, IP_IPSEC_POLICY,
	    ipsec_policy_in, ipsec_get_policylen(ipsec_policy_in)) < 0) {
		l2tpd_log(_l2tpd, LOG_WARNING,
		    "setsockopt(,,IP_IPSEC_POLICY(in)) failed in %s(): %m",
		    __func__);
	}
	if (ipsec_policy_out != NULL &&
	    setsockopt(sock, IPPROTO_IP, IP_IPSEC_POLICY,
	    ipsec_policy_out, ipsec_get_policylen(ipsec_policy_out)) < 0) {
		l2tpd_log(_l2tpd, LOG_WARNING,
		    "setsockopt(,,IP_IPSEC_POLICY(out)) failed in %s(): %m",
		    __func__);
	}
#endif
	_this->sock = sock;

	event_set(&_this->ev_sock, _this->sock, EV_READ | EV_PERSIST,
	    l2tpd_io_event, _this);
	event_add(&_this->ev_sock, NULL);

	l2tpd_log(_l2tpd, LOG_INFO, "Listening %s:%u/udp (L2TP LNS) [%s]",
	    inet_ntoa(_this->bind_sin.sin_addr),
	    ntohs(_this->bind_sin.sin_port), _this->phy_label);

	return 0;
reigai:
	if (sock >= 0)
		close(sock);
		
	return 1;
}

/** L2TP�ǡ����򳫻Ϥ��ޤ���*/
int
l2tpd_start(l2tpd *_this)
{
	int rval;
	caddr_t ipsec_policy_in, ipsec_policy_out;
	l2tpd_listener *plsnr;

	rval = 0;
	ipsec_policy_in = NULL;
	ipsec_policy_out = NULL;

	L2TPD_ASSERT(_this->state == L2TPD_STATE_INIT);
	if (_this->state != L2TPD_STATE_INIT) {
		l2tpd_log(_this, LOG_ERR, "Failed to start l2tpd: illegal "
		    "state.");
		return -1;
	}
	if (_this->require_ipsec != 0) {
#if 0
		/*
		 * NOTE ipsec_set_policy() ������Ѥ��� yacc �Υ����å��Ѥ�
		 * �Хåե���ưŪ�˳�����Ƥ��ޤ�������������ޤ���
		 * yasuoka ��Ĵ������ 2000 �Х��ȥ꡼�����ޤ���
		 */
		if ((ipsec_policy_in = ipsec_set_policy(L2TPD_IPSEC_POLICY_IN,
		    strlen(L2TPD_IPSEC_POLICY_IN))) == NULL) {
			l2tpd_log(_this, LOG_ERR,
			    "ipsec_set_policy(L2TPD_IPSEC_POLICY_IN) failed "
			    "at %s(): %s: %m", __func__, ipsec_strerror());
				goto reigai;
		}
		if ((ipsec_policy_out = ipsec_set_policy(L2TPD_IPSEC_POLICY_OUT,
		    strlen(L2TPD_IPSEC_POLICY_OUT))) == NULL) {
			l2tpd_log(_this, LOG_ERR,
			    "ipsec_set_policy(L2TPD_IPSEC_POLICY_OUT) failed "
			    "at %s(): %s: %m", __func__, ipsec_strerror());
			goto reigai;
		}
#endif
	}

	slist_itr_first(&_this->listener);
	while (slist_itr_has_next(&_this->listener)) {
		plsnr = slist_itr_next(&_this->listener);
		rval |= l2tpd_listener_start(plsnr, ipsec_policy_in,
		    ipsec_policy_out);
	}

	if (ipsec_policy_in != NULL)
		free(ipsec_policy_in);
	if (ipsec_policy_out != NULL)
		free(ipsec_policy_out);

	if (rval == 0)
		_this->state = L2TPD_STATE_RUNNING;

	return rval;
reigai:
	if (ipsec_policy_in != NULL)
		free(ipsec_policy_in);
	if (ipsec_policy_out != NULL)
		free(ipsec_policy_out);

	return 1;
}

/** �Ԥ�������λ���ޤ� */
static void
l2tpd_listener_stop(l2tpd_listener *_this)
{
	if (_this->sock >= 0) {
		event_del(&_this->ev_sock);
		close(_this->sock);
		l2tpd_log(_this->self, LOG_INFO,
		    "Shutdown %s:%u/udp (L2TP LNS)",
		    inet_ntoa(_this->bind_sin.sin_addr),
		    ntohs(_this->bind_sin.sin_port));
		_this->sock = -1;
	}
}
/**
 * ���Ǥ�ͱͽ�����ˤ�������ߤ��ޤ���
 */
void
l2tpd_stop_immediatly(l2tpd *_this)
{
	l2tpd_listener *plsnr;

	slist_itr_first(&_this->listener);
	while (slist_itr_has_next(&_this->listener)) {
		plsnr = slist_itr_next(&_this->listener);
		l2tpd_listener_stop(plsnr);
	}
	event_del(&_this->ev_timeout);	// �ͤ�Τ���
	_this->state = L2TPD_STATE_STOPPED;
}

/**
 * {@link ::_l2tp_ctrl ����ȥ���} ����λ�����ݤ˥����뤵��ޤ���
 */
void
l2tpd_ctrl_finished_notify(l2tpd *_this)
{
	if (_this->state != L2TPD_STATE_SHUTTING_DOWN)
		return;

	if (hash_first(_this->ctrl_map) != NULL)
		return;

	l2tpd_stop_immediatly(_this);
}

static void
l2tpd_stop_timeout(int fd, short evtype, void *ctx)
{
	hash_link *hl;
	l2tp_ctrl *ctrl;
	l2tpd *_this;

	_this = ctx;
	l2tpd_log(_this, LOG_INFO, "Shutdown timeout");
	for (hl = hash_first(_this->ctrl_map); hl != NULL;
	    hl = hash_next(_this->ctrl_map)) {
		ctrl = hl->item;
		l2tp_ctrl_stop(ctrl, 0);
	}
	l2tpd_stop_immediatly(_this);
}

/**
 * L2TP�ǡ�������ߤ��ޤ���
 */
void
l2tpd_stop(l2tpd *_this)
{
	int nctrls = 0;
	hash_link *hl;
	l2tp_ctrl *ctrl;

	nctrls = 0;
	event_del(&_this->ev_timeout);
	if (l2tpd_is_stopped(_this))
		return;
	if (l2tpd_is_shutting_down(_this)) {
		/*
		 * 2���ܤϤ����˽�λ
		 */
		l2tpd_stop_immediatly(_this);
		return;
	}
	for (hl = hash_first(_this->ctrl_map); hl != NULL;
	    hl = hash_next(_this->ctrl_map)) {
		ctrl = hl->item;
		l2tp_ctrl_stop(ctrl, L2TP_STOP_CCN_RCODE_SHUTTING_DOWN);
		nctrls++;
	}
	_this->state = L2TPD_STATE_SHUTTING_DOWN;
	if (nctrls > 0) {
		struct timeval tv0;

		tv0.tv_usec = 0;
		tv0.tv_sec = L2TPD_SHUTDOWN_TIMEOUT;

		evtimer_set(&_this->ev_timeout, l2tpd_stop_timeout, _this);
		evtimer_add(&_this->ev_timeout, &tv0);

		return;
	}
	l2tpd_stop_immediatly(_this);
}

/***********************************************************************
 * �����Ϣ
 ***********************************************************************/
#define	CFG_KEY(p, s)	config_key_prefix((p), (s))
#define	VAL_SEP		" \t\r\n"

CONFIG_FUNCTIONS(l2tpd_config, l2tpd, config);
PREFIXED_CONFIG_FUNCTIONS(l2tp_ctrl_config, l2tp_ctrl, l2tpd->config,
    phy_label);

int
l2tpd_reload(l2tpd *_this, struct properties *config, const char *name,
    int default_enabled)
{
	int i, do_start, aierr;
	const char *val;
	char *tok, *cp, buf[L2TPD_CONFIG_BUFSIZ], *label;
	struct addrinfo *ai;

	_this->config = config;
	do_start = 0;
	if (l2tpd_config_str_equal(_this, CFG_KEY(name, "enabled"), "true", 
	    default_enabled)) {
		// false �ˤ���ľ��� true �ˤ���뤫�⤷��ʤ���
		if (l2tpd_is_shutting_down(_this)) 
			l2tpd_stop_immediatly(_this);
		if (l2tpd_is_stopped(_this))
			do_start = 1;
	} else {
		if (!l2tpd_is_stopped(_this))
			l2tpd_stop(_this);
		return 0;
	}
	if (do_start && l2tpd_init(_this) != 0)
		return 1;
	_this->config = config;

	/* ���꤬�ʤ��ä���Ȥ��� */
	 gethostname(_this->default_hostname, sizeof(_this->default_hostname));

	_this->ctrl_in_pktdump = l2tpd_config_str_equal(_this,
	    "log.l2tp.ctrl.in.pktdump", "true", 0);
	_this->data_in_pktdump = l2tpd_config_str_equal(_this,
	    "log.l2tp.data.in.pktdump", "true", 0);
	_this->ctrl_out_pktdump = l2tpd_config_str_equal(_this,
	    "log.l2tp.ctrl.out.pktdump", "true", 0);
	_this->data_out_pktdump = l2tpd_config_str_equal(_this,
	    "log.l2tp.data.out.pktdump", "true", 0);
	_this->phy_label_with_ifname = l2tpd_config_str_equal(_this,
	    CFG_KEY(name, "label_with_ifname"), "true", 0);

	// ip4_allow ��ѡ���
	in_addr_range_list_remove_all(&_this->ip4_allow);
	val = l2tpd_config_str(_this, CFG_KEY(name, "ip4_allow"));
	if (val != NULL) {
		if (strlen(val) >= sizeof(buf)) {
			l2tpd_log(_this, LOG_ERR, "configuration error at "
			    "l2tpd.ip4_allow: too long");
			return 1;
		}
		strlcpy(buf, val, sizeof(buf));
		for (cp = buf; (tok = strsep(&cp, VAL_SEP)) != NULL;) {
			if (*tok == '\0')
				continue;
			if (in_addr_range_list_add(&_this->ip4_allow, tok)
			    != 0) {
				l2tpd_log(_this, LOG_ERR,
				    "configuration error at "
				    "l2tpd.ip4_allow: %s", tok);
				return 1;
			}
		}
	}

	if (do_start) {
		/*
		 * ��ưľ��ȡ�l2tpd.enable �� false -> true ���ѹ����줿
		 * ���ˡ�do_start�����٤ƤΥꥹ�ʡ�������������줿���֤�
		 * ����Ǥ���
		 */
		// l2tpd.listener_in ���ɤ߹���
		val = l2tpd_config_str(_this, CFG_KEY(name, "listener_in"));
		if (val != NULL) {
			if (strlen(val) >= sizeof(buf)) {
				l2tpd_log(_this, LOG_ERR,
				    "configuration error at %s: too long",
				    CFG_KEY(name, "listener"));
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
				if ((aierr = addrport_parse(tok, IPPROTO_UDP,
				    &ai)) != 0) {
					l2tpd_log(_this, LOG_ERR,
					    "configuration error at "
					    "l2tpd.listener_in: %s: %s", label,
					    gai_strerror(aierr));
					label = NULL;
					return 1;
				}
				L2TPD_ASSERT(ai != NULL &&
				    ai->ai_family == AF_INET);
				if (l2tpd_add_listener(_this, i, label,
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
				l2tpd_log(_this, LOG_ERR, "configuration "
				    "error at l2tpd.listener_in: %s", label);
				return 1;
			}
		}
		_this->purge_ipsec_sa = l2tpd_config_str_equal(_this,
		    CFG_KEY(name, "purge_ipsec_sa"), "true", 1);
		_this->require_ipsec = l2tpd_config_str_equal(_this,
		    CFG_KEY(name, "require_ipsec"), "true", 1);

		if (l2tpd_start(_this) != 0)
			return 1;
	}

	return 0;
}

/***********************************************************************
 * I/O ��Ϣ
 ***********************************************************************/
/** ������������ݤ������Ȥ���˻Ĥ� */
void
l2tpd_log_access_deny(l2tpd *_this, const char *reason, struct sockaddr *peer)
{
	char hostbuf[NI_MAXHOST], servbuf[NI_MAXSERV];

	if (getnameinfo(peer, peer->sa_len, hostbuf, sizeof(hostbuf),
	    servbuf, sizeof(servbuf), NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
		l2tpd_log(_this, LOG_ERR, "getnameinfo() failed at %s(): %m",
		    __func__);
		return;
	}
	l2tpd_log(_this, LOG_ALERT, "Received packet from %s:%s/udp: "
	    "%s", hostbuf, servbuf, reason);
}

/** I/O���٥�ȥϥ�ɥ� */
static void
l2tpd_io_event(int fd, short evtype, void *ctx)
{
	int sz;
	l2tpd *_l2tpd;
	l2tpd_listener *_this;
	socklen_t peerlen, socklen;
	struct sockaddr_storage peer, sock;
	u_char buf[8192];
	void *nat_t;

	_this = ctx;
	_l2tpd = _this->self;
	if ((evtype & EV_READ) != 0) {
		peerlen = sizeof(peer);
		socklen = sizeof(sock);
		while (!l2tpd_is_stopped(_l2tpd)) {
#ifdef USE_LIBSOCKUTIL
			int sa_cookie_len;
			struct in_ipsec_sa_cookie sa_cookie;

			sa_cookie_len = sizeof(sa_cookie);
			if ((sz = recvfromto_nat_t(_this->sock, buf,
			    sizeof(buf), 0,
			    (struct sockaddr *)&peer, &peerlen,
			    (struct sockaddr *)&sock, &socklen,
			    &sa_cookie, &sa_cookie_len)) <= 0) {
#else
			if ((sz = recvfromto(_this->sock, buf,
			    sizeof(buf), 0,
			    (struct sockaddr *)&peer, &peerlen,
			    (struct sockaddr *)&sock, &socklen)) <= 0) {
#endif
				if (errno == EAGAIN || errno == EINTR)
					break;
				l2tpd_log(_l2tpd, LOG_ERR,
				    "recvfrom() failed in %s(): %m",
				    __func__);
				l2tpd_stop(_l2tpd);
				return;
			}
			//�����������å�(allows.in)
			switch (peer.ss_family) {
			case AF_INET:
#ifdef USE_LIBSOCKUTIL
				if (sa_cookie_len > 0)
					nat_t = &sa_cookie;
				else
					nat_t = NULL;
#else
				nat_t = NULL;
#endif
				/*
				 * XXX NAT-T �ξ��������������å�
				 */
				if (in_addr_range_list_includes(
				    &_l2tpd->ip4_allow,
				    &((struct sockaddr_in *)&peer)->sin_addr))
					l2tp_ctrl_input(_l2tpd, _this->index,
					    (struct sockaddr *)&peer,
					    (struct sockaddr *)&sock, nat_t,
					    buf, sz);
				else
					l2tpd_log_access_deny(_l2tpd, 
					    "not allowed by acl.",
					    (struct sockaddr *)&peer);
				break;
			default:
				l2tpd_log(_l2tpd, LOG_ERR,
				    "received from unknown address family = %d",
				    peer.ss_family);
				break;
			}
		}
	}
}

/***********************************************************************
 * L2TP����ȥ����Ϣ
 ***********************************************************************/
l2tp_ctrl *
l2tpd_get_ctrl(l2tpd *_this, int tunid)
{
	hash_link *hl;

	hl = hash_lookup(_this->ctrl_map, (void *)tunid);
	if (hl == NULL)
		return NULL;

	return hl->item;
}

void
l2tpd_add_ctrl(l2tpd *_this, l2tp_ctrl *ctrl)
{
	hash_insert(_this->ctrl_map, (void *)ctrl->tunnel_id, ctrl);
}

void
l2tpd_remove_ctrl(l2tpd *_this, int tunid)
{
	hash_delete(_this->ctrl_map, (void *)tunid, 0);
}


/***********************************************************************
 * ��¿
 ***********************************************************************/

void
l2tpd_log(l2tpd *_this, int prio, const char *fmt, ...)
{
	char logbuf[BUFSIZ];
	va_list ap;

	va_start(ap, fmt);
#ifdef	L2TPD_MULITPLE
	snprintf(logbuf, sizeof(logbuf), "l2tpd id=%u %s", _this->id, fmt);
#else
	snprintf(logbuf, sizeof(logbuf), "l2tpd %s", fmt);
#endif
	vlog_printf(prio, logbuf, ap);
	va_end(ap);
}
