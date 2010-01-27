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
 * PPPoE�����Фμ�����
 * <dl>
 *  <dt>RFC 2516</dt>
 *  <dd>A Method for Transmitting PPP Over Ethernet (PPPoE)</dd>
 * </dl>
 * $Id: pppoed.c,v 1.2 2010/01/13 07:49:44 yasuoka Exp $
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_types.h>
#if defined(__NetBSD__)
#include <net/if_ether.h>
#else
#include <netinet/if_ether.h>
#endif
#include <net/if_dl.h>
#include <net/ethertypes.h>
#include <net/bpf.h>
#include <string.h>
#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <event.h>
#include <signal.h>
#include <stdlib.h>
#include <ifaddrs.h>
#include <stdarg.h>
#include <errno.h>

#include "debugutil.h"
#include "slist.h"
#include "bytebuf.h"
#include "hash.h"
#include "properties.h"
#include "config_helper.h"
#include "rtev.h"

#include "pppoe.h"
#include "pppoe_local.h"

static int pppoed_seqno = 0;

#ifdef	PPPOED_DEBUG
#define	PPPOED_ASSERT(x)	ASSERT(x)
#define	PPPOED_DBG(x)	pppoed_log x
#else
#define	PPPOED_ASSERT(x)
#define	PPPOED_DBG(x)
#endif

static void      pppoed_log (pppoed *, int, const char *, ...) __printflike(3,4);
static void      pppoed_listener_init(pppoed *, pppoed_listener *);
static int       pppoed_output (pppoed_listener *, u_char *, u_char *, int);
static int       pppoed_listener_start (pppoed_listener *, int);
static void      pppoed_io_event (int, short, void *);
static void      pppoed_input (pppoed_listener *, uint8_t [ETHER_ADDR_LEN], int, u_char *, int);
static void      pppoed_recv_PADR (pppoed_listener *, uint8_t [ETHER_ADDR_LEN], slist *);
static void      pppoed_recv_PADI (pppoed_listener *, uint8_t [ETHER_ADDR_LEN], slist *);
static int       session_id_cmp (void *, void *);
static uint32_t  session_id_hash (void *, size_t);

#ifdef PPPOE_TEST
static void      pppoed_on_sigterm (int, short, void *);
static void      usage (void);
#endif
static const char *pppoe_code_string(int);
#ifdef	PPPOED_DEBUG
static const char *pppoe_tag_string(int);
#endif

/***********************************************************************
 * �ǡ�����Ϣ
 ***********************************************************************/
/** PPPoE �ǡ������������ޤ� */
int
pppoed_init(pppoed *_this)
{
	int i, off, id;

	memset(_this, 0, sizeof(pppoed));
	_this->id = pppoed_seqno++;

	if ((_this->session_hash = hash_create(
	    (int (*) (const void *, const void *))session_id_cmp,
	    (uint32_t (*) (const void *, int))session_id_hash,
	    PPPOE_SESSION_HASH_SIZ)) == NULL) {
		pppoed_log(_this, LOG_ERR, "hash_create() failed on %s(): %m",
		    __func__);
		goto reigai;
	}

	slist_init(&_this->session_free_list);
	if (slist_add(&_this->session_free_list,
	    (void *)PPPOED_SESSION_SHUFFLE_MARK) == NULL) {
		pppoed_log(_this, LOG_ERR, "slist_add() failed on %s(): %m",
		    __func__);
		goto reigai;
	}

	/* XXX ���å����ϥå���ν���� */
	if ((_this->acookie_hash = hash_create(
	    (int (*) (const void *, const void *))session_id_cmp,
	    (uint32_t (*) (const void *, int))session_id_hash,
	    PPPOE_SESSION_HASH_SIZ)) == NULL) {
		pppoed_log(_this, LOG_WARNING,
		    "hash_create() failed on %s(): %m", __func__);
		pppoed_log(_this, LOG_WARNING, "hash_create() failed on %s(): "
		    "ac-cookie hash create failed.", __func__);
		_this->acookie_hash = NULL;
	}
	_this->acookie_next = random();

#if PPPOE_NSESSION > 0xffff
#error PPPOE_NSESSION must be less than 65536
#endif
	off = random() % 0xffff;
	for (i = 0; i < PPPOE_NSESSION; i++) {
		id = (i + off) % 0xffff;
		if (id == 0)
			id = (off - 1) % 0xffff;
		if (slist_add(&_this->session_free_list, (void *)id) == NULL) {
			pppoed_log(_this, LOG_ERR,
			    "slist_add() failed on %s(): %m", __func__);
			goto reigai;
		}
	}

	_this->state = PPPOED_STATE_INIT;

	return 0;
reigai:
	pppoed_uninit(_this);
	return 1;
}

static void
pppoed_listener_init(pppoed *_this, pppoed_listener *listener) 
{
	memset(listener, 0, sizeof(pppoed_listener));
	listener->bpf = -1;
	listener->self = _this;
	listener->index = PPPOED_LISTENER_INVALID_INDEX;
}

/** �ꥹ�ʤ����ɤ��ޤ� */
int
pppoed_reload_listeners(pppoed *_this)
{
	int rval = 0;

	if (_this->state == PPPOED_STATE_RUNNING &&
	    _this->listen_incomplete != 0)
		rval = pppoed_start(_this);

	return rval;
}

/**
 * ¾�Ͱ��Υѥ��åȤ�������뤫��(see bpf(4))����ʬ���ȥ֥��ɥ��㥹�Ȱ�
 * ���Ƥ���
 */
#define	REJECT_FOREIGN_ADDRESS 1

#define ETHER_FIRST_INT(e)	((e)[0]<<24|(e)[1]<<16|(e)[2]<<8|(e)[3])
#define ETHER_LAST_SHORT(e)	((e)[4]<<8|(e)[5])

static int
pppoed_listener_start(pppoed_listener *_this, int restart)
{
	int i;
	int log_level;
	char buf[BUFSIZ];
	struct ifreq ifreq;
	int ival;
	int found;
	struct ifaddrs *ifa0, *ifa;
	struct sockaddr_dl *sdl;
	struct bpf_insn insns[] = {
	    /* check etyer type = PPPOEDESC or PPPOE */
		BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 12),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETHERTYPE_PPPOEDISC, 2, 0),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, ETHERTYPE_PPPOE, 1, 0),
		BPF_STMT(BPF_RET+BPF_K, (u_int)0),
#ifndef	REJECT_FOREIGN_ADDRESS
		BPF_STMT(BPF_RET+BPF_K, (u_int)-1),
#else
	/* ff:ff:ff:ff:ff:ff �� */
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 0),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0xffffffff, 0, 3),
		BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 4),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K, 0xffff, 0, 1),
		BPF_STMT(BPF_RET+BPF_K, (u_int)-1),
	/* ��ʬ�� Mac �� */
		BPF_STMT(BPF_LD+BPF_W+BPF_ABS, 0),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K,
		    ETHER_FIRST_INT(_this->ether_addr), 0, 3),
		BPF_STMT(BPF_LD+BPF_H+BPF_ABS, 4),
		BPF_JUMP(BPF_JMP+BPF_JEQ+BPF_K,
		    ETHER_LAST_SHORT(_this->ether_addr), 0, 1),
		BPF_STMT(BPF_RET+BPF_K, (u_int)-1),
		BPF_STMT(BPF_RET+BPF_K, (u_int)0),
#endif 
	};
	struct bpf_program bf_filter = {
		.bf_len = countof(insns),
		.bf_insns = insns
	};
	pppoed *_pppoed;

	if (restart == 0)
		log_level = LOG_ERR;
	else
		log_level = LOG_INFO;

	_pppoed = _this->self;

	ifa0 = NULL;
	if (getifaddrs(&ifa0) != 0) {
		pppoed_log(_pppoed, log_level,
		    "getifaddrs() failed on %s(): %m", __func__);
		return -1;
	}
	found = 0;
	for (ifa = ifa0; ifa != NULL; ifa = ifa->ifa_next) {
		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		if (sdl->sdl_family != AF_LINK || sdl->sdl_type != IFT_ETHER ||
		    sdl->sdl_alen != ETHER_ADDR_LEN)
			continue;
		if (strcmp(ifa->ifa_name, _this->listen_ifname) == 0) {
			memcpy(_this->ether_addr,
			    (caddr_t)LLADDR(sdl), ETHER_ADDR_LEN);
			found = 1;
			break;
		}
	}
	freeifaddrs(ifa0);
	if (!found) {
		pppoed_log(_pppoed, log_level, "%s is not available.",
		    _this->listen_ifname);
		goto reigai;
	}

	/* Open /dev/bpfXX */
	/* FIXME: NetBSD 3.0 �Ǥϡ�/dev/bpf ��Ĥǲ��٤Ⳬ����餷�� */
	for (i = 0; i < 256; i++) {
		snprintf(buf, sizeof(buf), "/dev/bpf%d", i);
		if ((_this->bpf = open(buf, O_RDWR, 0600)) >= 0) {
			break;
		} else if (errno == ENXIO || errno == ENOENT)
			break;	/* ����ʾ�õ���Ƥ�ߤĤ���ʤ��Ϥ� */
	}
	if (_this->bpf < 0) {
		pppoed_log(_pppoed, log_level, "Cannot open bpf");
		goto reigai;
	}

	ival = BPF_CAPTURE_SIZ;
	if (ioctl(_this->bpf, BIOCSBLEN, &ival) != 0) {
		pppoed_log(_pppoed, log_level, "ioctl(bpf, BIOCSBLEN(%d)): %m",
		    ival);
		goto reigai;
	}
	ival = 1;
	if (ioctl(_this->bpf, BIOCIMMEDIATE, &ival) != 0) {
		pppoed_log(_pppoed, log_level, "Cannot start bpf on %s: %m",
		    _this->listen_ifname);
		goto reigai;
	}

	/* bind interface */
	memset(&ifreq, 0, sizeof(ifreq));
	strlcpy(ifreq.ifr_name, _this->listen_ifname, sizeof(ifreq.ifr_name));
	if (ioctl(_this->bpf, BIOCSETIF, &ifreq) != 0) {
		pppoed_log(_pppoed, log_level, "Cannot start bpf on %s: %m",
		    _this->listen_ifname);
		goto reigai;
	}

	/* set linklocal address */
#ifdef	REJECT_FOREIGN_ADDRESS
	insns[10].k = ETHER_FIRST_INT(_this->ether_addr);
	insns[12].k = ETHER_LAST_SHORT(_this->ether_addr);
#endif

	/* set filter */
	if (ioctl(_this->bpf, BIOCSETF, &bf_filter) != 0) {
		pppoed_log(_pppoed, log_level, "ioctl(bpf, BIOCSETF()): %m");
		goto reigai;
	}

	event_set(&_this->ev_bpf, _this->bpf, EV_READ | EV_PERSIST,
	    pppoed_io_event, _this);
	event_add(&_this->ev_bpf, NULL);

	pppoed_log(_pppoed, LOG_INFO, "Listening on %s (PPPoE) [%s] using=%s "
	    "address=%02x:%02x:%02x:%02x:%02x:%02x", _this->listen_ifname,
	    _this->phy_label, buf, _this->ether_addr[0], _this->ether_addr[1],
	    _this->ether_addr[2], _this->ether_addr[3], _this->ether_addr[4],
	    _this->ether_addr[5]);

	return 0;
reigai:
	if (_this->bpf >= 0) {
		close(_this->bpf);
		_this->bpf = -1;
	}

	return 1;
}

/** PPPoE �ǡ����򳫻Ϥ��ޤ� */
int
pppoed_start(pppoed *_this)
{
	int rval = 0;
	int nlistener_fail = 0;
	pppoed_listener *plistener;

	slist_itr_first(&_this->listener);
	while (slist_itr_has_next(&_this->listener)) {
		plistener = slist_itr_next(&_this->listener);
		PPPOED_ASSERT(plistener != NULL);
		if (plistener->bpf < 0) {
			if (pppoed_listener_start(plistener,
			    _this->listen_incomplete) != 0)
				nlistener_fail++;
		}
	}
	if (nlistener_fail > 0)
		_this->listen_incomplete = 1;
	else
		_this->listen_incomplete = 0;

	_this->state = PPPOED_STATE_RUNNING;

	return rval;
}

static void
pppoed_listener_stop(pppoed_listener *_this)
{
	pppoed *_pppoed;

	PPPOED_ASSERT(_this != NULL);
	_pppoed = _this->self;
	PPPOED_ASSERT(_pppoed != NULL);

	if (_this->bpf >= 0) {
		event_del(&_this->ev_bpf);
		close(_this->bpf);
		pppoed_log(_pppoed, LOG_INFO, "Shutdown %s (PPPoE) [%s] "
		    "address=%02x:%02x:%02x:%02x:%02x:%02x",
		    _this->listen_ifname, _this->phy_label,
		    _this->ether_addr[0], _this->ether_addr[1],
		    _this->ether_addr[2], _this->ether_addr[3],
		    _this->ether_addr[4], _this->ether_addr[5]);
		_this->bpf = -1;
	}
}

/** PPPoE �ǡ�������ߤ��ޤ� */
void
pppoed_stop(pppoed *_this)
{
	pppoed_listener *plistener;
	hash_link *hl;
	pppoe_session *session; 

	if (!pppoed_is_running(_this))
		return;

	_this->state = PPPOED_STATE_STOPPED;
	if (_this->session_hash != NULL) {
		for (hl = hash_first(_this->session_hash);
		    hl != NULL;
		    hl = hash_next(_this->session_hash)) {
			session = (pppoe_session *)hl->item;
			pppoe_session_disconnect(session);
			pppoe_session_stop(session);
		}
	}
	for (slist_itr_first(&_this->listener);
	    slist_itr_has_next(&_this->listener);) {
		plistener = slist_itr_next(&_this->listener);
		pppoed_listener_stop(plistener);
		free(plistener);
		slist_itr_remove(&_this->listener);
	}
	pppoed_log(_this, LOG_NOTICE, "Stopped");
}

/** PPPoE �ǡ�����������ޤ� */
void
pppoed_uninit(pppoed *_this)
{
	if (_this->session_hash != NULL) {
		hash_free(_this->session_hash);
		_this->session_hash = NULL;
	}
	if (_this->acookie_hash != NULL) {
		hash_free(_this->acookie_hash);
		_this->acookie_hash = NULL;
	}
	slist_fini(&_this->session_free_list);
	slist_fini(&_this->listener); // stop ���Фϲ����ѡ�
	_this->config = NULL;
}

/** PPPoE ���å���� close ���줿���˸ƤӽФ���ޤ���*/
void
pppoed_pppoe_session_close_notify(pppoed *_this, pppoe_session *session)
{
	slist_add(&_this->session_free_list, (void *)session->session_id);

	if (_this->acookie_hash != NULL)
		hash_delete(_this->acookie_hash, (void *)session->acookie, 0);
	if (_this->session_hash != NULL)
		hash_delete(_this->session_hash, (void *)session->session_id,
		    0);

	pppoe_session_fini(session);
	free(session);
}

/***********************************************************************
 * �����Ϣ
 ***********************************************************************/
#define	CFG_KEY(p, s)	config_key_prefix((p), (s))
#define	VAL_SEP		" \t\r\n"

CONFIG_FUNCTIONS(pppoed_config, pppoed, config);
PREFIXED_CONFIG_FUNCTIONS(pppoed_listener_config, pppoed_listener, self->config,
    phy_label);

/** PPPoE �ǡ������������ɤ߹��ߤ��ޤ� */
int
pppoed_reload(pppoed *_this, struct properties *config, const char *name,
    int default_enabled)
{
	struct sockaddr_dl *sdl;
	int i, count, found;
	hash_link *hl;
	const char *val;
	char *tok, *cp, buf[PPPOED_CONFIG_BUFSIZ], *label;
	pppoed_listener *l;
	int do_start;
	struct {
		char ifname[IF_NAMESIZE];
		char label[PPPOED_PHY_LABEL_SIZE];
	} listeners[PPPOE_NLISTENER];
	struct ifaddrs *ifa0, *ifa;
	slist rmlist, newlist;
	pppoe_session *session;

	do_start = 0;

	_this->config = config;
	if (pppoed_config_str_equal(_this, CFG_KEY(name, "enabled"), "true", 
	    default_enabled)) {
		// false �ˤ���ľ��� true �ˤ���뤫�⤷��ʤ���
		if (pppoed_is_stopped(_this) || !pppoed_is_running(_this))
			do_start = 1;
	} else {
		if (!pppoed_is_stopped(_this))
			pppoed_stop(_this);
		return 0;
	}

	if (do_start) {
		if (pppoed_init(_this) != 0)
			return 1;
		_this->config = config;
	}

	ifa0 = NULL;
	slist_init(&rmlist);
	slist_init(&newlist);

	_this->desc_in_pktdump = pppoed_config_str_equal(_this,
	    "log.pppoe.desc.in.pktdump", "true", 0);
	_this->desc_out_pktdump = pppoed_config_str_equal(_this,
	    "log.pppoe.desc.out.pktdump", "true", 0);

	_this->session_in_pktdump = pppoed_config_str_equal(_this,
	    "log.pppoe.session.in.pktdump", "true", 0);
	_this->session_out_pktdump = pppoed_config_str_equal(_this,
	    "log.pppoe.session.out.pktdump", "true", 0);

	if (getifaddrs(&ifa0) != 0) {
		pppoed_log(_this, LOG_ERR,
		    "getifaddrs() failed on %s(): %m", __func__);
		goto reigai;
	}
	count = 0;
	val = pppoed_config_str(_this, CFG_KEY(name, "interface"));
	if (val != NULL) {
		if (strlen(val) >= sizeof(buf)) {
			log_printf(LOG_ERR, "configuration error at "
			    "%s: too long", CFG_KEY(name, "interface"));
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
			PPPOED_ASSERT(count < countof(listeners));
			if (count >= countof(listeners)) {
				pppoed_log(_this, LOG_ERR,
				    "Too many listeners");
				goto reigai;
			}
			/* ���󥿥ե������μº߳�ǧ */
			found = 0;
			for (ifa = ifa0; ifa != NULL; ifa = ifa->ifa_next) {
				sdl = (struct sockaddr_dl *)ifa->ifa_addr;
				if (sdl->sdl_family == AF_LINK &&
				    IFTYPE_IS_LAN(sdl->sdl_type) &&
				    strcmp(ifa->ifa_name, tok) == 0) {
					found = 1;
					break;
				}
			}
			if (!found) {
				pppoed_log(_this, LOG_ERR,
				    "interface %s is not found", tok);
				goto reigai;
			}
			strlcpy(listeners[count].ifname, tok,
			    sizeof(listeners[count].ifname));
			strlcpy(listeners[count].label, label,
			    sizeof(listeners[count].label));

			label = NULL;
			count++;
		}
		if (label != NULL) {
			log_printf(LOG_ERR, "configuration error at %s: %s",
			    CFG_KEY(name, "interface"), label);
			return 1;
		}
	}

	if (slist_add_all(&rmlist, &_this->listener) != 0)
		goto reigai;

	for (i = 0; i < count; i++) {
		found = 0;
		l = NULL;
		slist_itr_first(&rmlist);
		while (slist_itr_has_next(&rmlist)) {
			l = slist_itr_next(&rmlist);
			if (strcmp(l->listen_ifname, listeners[i].ifname) == 0){
				slist_itr_remove(&rmlist);
				found = 1;
				break;
			}
		}
		if (!found) {
			if ((l = malloc(sizeof(pppoed_listener))) == NULL)
				goto reigai;
			pppoed_listener_init(_this, l);
		}
		l->self = _this;
		strlcpy(l->phy_label, listeners[i].label,
		    sizeof(l->phy_label));
		strlcpy(l->listen_ifname, listeners[i].ifname,
		    sizeof(l->listen_ifname));
		if (slist_add(&newlist, l) == NULL) {
			pppoed_log(_this, LOG_ERR,
			    "slist_add() failed in %s(): %m", __func__);
			goto reigai;
		}
	}

	if (slist_set_size(&_this->listener, count) != 0)
		goto reigai;

	/* �Ȥ�ʤ��ʤä��ꥹ�ʤ���� */
	slist_itr_first(&rmlist);
	while (slist_itr_has_next(&rmlist)) {
		l = slist_itr_next(&rmlist);
		/* �������� PPPoE���å����ι�θ */
		if (_this->session_hash != NULL) {
			for (hl = hash_first(_this->session_hash); hl != NULL;
			    hl = hash_next(_this->session_hash)) {
				session = (pppoe_session *)hl->item;
				if (session->listener_index == l->index)
					pppoe_session_stop(session);
			}
		}
		pppoed_listener_stop(l);
		free(l);
	}
	slist_remove_all(&_this->listener);
	/* slist_set_size ���Ƥ���Τǡ����Ԥ��ʤ��Ϥ� */
	(void)slist_add_all(&_this->listener, &newlist);

	/* ����ǥå����Υꥻ�å� */
	slist_itr_first(&newlist);
	for (i = 0; slist_itr_has_next(&newlist); i++) {
		l = slist_itr_next(&newlist);
		if (l->index != i && l->index != PPPOED_LISTENER_INVALID_INDEX){
			PPPOED_DBG((_this, LOG_DEBUG, "listener %d => %d",
			    l->index, i));
			for (hl = hash_first(_this->session_hash); hl != NULL;
			    hl = hash_next(_this->session_hash)) {
				session = (pppoe_session *)hl->item;
				if (session->listener_index == l->index)
					session->listener_index = i;
			}
		}
		l->index = i;
	}

	slist_fini(&rmlist);
	slist_fini(&newlist);
	if (ifa0 != NULL)
		freeifaddrs(ifa0);

	if (pppoed_start(_this) != 0)
		return 1;

	return 0;
reigai:
	slist_fini(&rmlist);
	slist_fini(&newlist);
	if (ifa0 != NULL)
		freeifaddrs(ifa0);

	return 1;
}
/***********************************************************************
 * I/O ��Ϣ
 ***********************************************************************/
static void
pppoed_io_event(int fd, short evmask, void *ctx)
{
	u_char buf[BPF_CAPTURE_SIZ], *pkt;
	int lpkt, off;
	pppoed_listener *_this;
	struct ether_header *ether;
	struct bpf_hdr *bpf;

	_this = ctx;

	PPPOED_ASSERT(_this != NULL);

	lpkt = read(_this->bpf, buf, sizeof(buf));
	pkt = buf;
	while (lpkt > 0) {
		if (lpkt < sizeof(struct bpf_hdr)) {
			pppoed_log(_this->self, LOG_WARNING,
			    "Received bad PPPoE packet: packet too short(%d)",
			    lpkt);
			break;
		}
		bpf = (struct bpf_hdr *)pkt;
		ether = (struct ether_header *)(pkt + bpf->bh_hdrlen);
		ether->ether_type = ntohs(ether->ether_type);
		if (memcmp(ether->ether_shost, _this->ether_addr,
		    ETHER_ADDR_LEN) == 0)
			goto next_pkt;	// ��ʬ�ѥ��å�
		off = bpf->bh_hdrlen + sizeof(struct ether_header);
		if (lpkt < off + sizeof(struct pppoe_header)) {
			pppoed_log(_this->self, LOG_WARNING,
			    "Received bad PPPoE packet: packet too short(%d)",
			    lpkt);
			break;
		}
		pppoed_input(_this, ether->ether_shost,
		    (ether->ether_type == ETHERTYPE_PPPOEDISC)? 1 : 0,
		    pkt + off, lpkt - off);
next_pkt:
		pkt = pkt + BPF_WORDALIGN(bpf->bh_hdrlen +
		    bpf->bh_caplen);
		lpkt -= BPF_WORDALIGN(bpf->bh_hdrlen + bpf->bh_caplen);
	}
	return;
}

static void
pppoed_input(pppoed_listener *_this, uint8_t shost[ETHER_ADDR_LEN], int is_disc,
    u_char *pkt, int lpkt)
{
	hash_link *hl;
	pppoe_session *session;
	struct pppoe_header *pppoe;
	struct pppoe_tlv *tlv;
	u_char tlvspace[2048], *p_tlvspace;
	int session_id;
	slist tag_list;
	const char *reason;

	reason = "";
	p_tlvspace = tlvspace;
	session = NULL;

	pppoe = (struct pppoe_header *)pkt;
	session_id = pppoe->session_id = ntohs(pppoe->session_id);
	pppoe->length = ntohs(pppoe->length);

#ifdef PPPOED_DEBUG
	if (is_disc) {
		PPPOED_DBG((_this->self, DEBUG_LEVEL_1,
		    "Recv%s(%02x) ver=%d type=%d session-id=%d if=%s",
		    pppoe_code_string(pppoe->code), pppoe->code, 
		    pppoe->ver, pppoe->type, pppoe->session_id,
		    _this->listen_ifname));
	}
#endif
	pkt += sizeof(struct pppoe_header);
	lpkt -= sizeof(struct pppoe_header);

	if (lpkt < pppoe->length) {
		reason = "received packet is shorter than "
		    "pppoe length field.";
		goto bad_packet;
	}
	lpkt = pppoe->length;	/* PPPoE�إå����ͤ�Ȥ� */

	if (pppoe->type != PPPOE_RFC2516_TYPE ||
	    pppoe->ver != PPPOE_RFC2516_VER) {
		reason = "received packet has wrong version or type.";
		goto bad_packet;
	}

	if (session_id != 0) {
		hl = hash_lookup(_this->self->session_hash, (void *)session_id);
		if (hl != NULL)
			session = (pppoe_session *)hl->item;
	}
	if (!is_disc) {
		if (session != NULL)
			pppoe_session_input(session, pkt, pppoe->length);
		return;
	}

	/*
	 * PPPoE-Discovery Packet proccessing.
	 */
	slist_init(&tag_list);
	while (lpkt > 0) {
		if (lpkt < 4) {
			reason = "tlv list is broken.  "
			    "Remaining octet is too short.";
			goto reigai;
		}
		tlv = (struct pppoe_tlv *)p_tlvspace;
		GETSHORT(tlv->type, pkt);
		GETSHORT(tlv->length, pkt);
		p_tlvspace += 4;
		lpkt -= 4;
		if (tlv->length > lpkt) {
			reason = "tlv list is broken.  length is wrong.";
			goto reigai;
		}
		if (tlv->length > 0) {
			memcpy(&tlv->value, pkt, tlv->length);
			pkt += tlv->length;
			lpkt -= tlv->length;
			p_tlvspace += tlv->length;
			p_tlvspace = (u_char *)ALIGN(p_tlvspace);
		}
#ifdef	PPPOED_DEBUG
		if (debuglevel >= 2)
			pppoed_log(_this->self, DEBUG_LEVEL_2,
			    "Recv%s tag %s(%04x)=%s",
			    pppoe_code_string(pppoe->code),
			    pppoe_tag_string(tlv->type), tlv->type,
			    pppoed_tlv_value_string(tlv));
#endif
		if (tlv->type == PPPOE_TAG_END_OF_LIST)
			break;
		if (slist_add(&tag_list, tlv) == NULL) {
			goto reigai;
		}
	}
	switch (pppoe->code) {
	case PPPOE_CODE_PADI:
		if (_this->self->state != PPPOED_STATE_RUNNING)
			break;
		pppoed_recv_PADI(_this, shost, &tag_list);
		break;
	case PPPOE_CODE_PADR:
		if (_this->self->state != PPPOED_STATE_RUNNING)
			break;
		pppoed_recv_PADR(_this, shost, &tag_list);
		break;
	case PPPOE_CODE_PADT:
		PPPOED_DBG((_this->self, LOG_DEBUG, "RecvPADT"));
		if (session != NULL)
			pppoe_session_recv_PADT(session, &tag_list);
		break;
	}
	slist_fini(&tag_list);

	return;
reigai:
	slist_fini(&tag_list);
bad_packet:
	pppoed_log(_this->self, LOG_INFO,
	    "Received a bad packet: code=%s(%02x) ver=%d type=%d session-id=%d"
	    " if=%s: %s", pppoe_code_string(pppoe->code), pppoe->code,
	    pppoe->ver, pppoe->type, pppoe->session_id, _this->listen_ifname,
	    reason);
}

static int
pppoed_output(pppoed_listener *_this, u_char *dhost, u_char *pkt, int lpkt)
{
	int sz, iovc;
	struct iovec iov[3];
	struct ether_header ether;
	struct pppoe_header *pppoe;
	u_char pad[ETHERMIN];

	memcpy(ether.ether_dhost, dhost, ETHER_ADDR_LEN);
	memcpy(ether.ether_shost, _this->ether_addr, ETHER_ADDR_LEN);

	iov[0].iov_base = &ether;
	iov[0].iov_len = sizeof(struct ether_header);
	ether.ether_type = htons(ETHERTYPE_PPPOEDISC);
	iov[1].iov_base = pkt;
	iov[1].iov_len = lpkt;
	pppoe = (struct pppoe_header *)pkt;
	pppoe->length = htons(lpkt - sizeof(struct pppoe_header));

	iovc = 2;

	if (lpkt < ETHERMIN) {
		memset(pad, 0, ETHERMIN - lpkt);
		iov[2].iov_base = pad;
		iov[2].iov_len = ETHERMIN - lpkt;
		iovc++;
	}

	sz = writev(_this->bpf, iov, iovc);

	return (sz > 0)? 0 : -1;
}

static void
pppoed_recv_PADR(pppoed_listener *_this, uint8_t shost[ETHER_ADDR_LEN],
    slist *tag_list) 
{
	int session_id, shuffle_cnt;
	pppoe_session *session;
	pppoed *_pppoed;

	_pppoed = _this->self;
	if ((session = malloc(sizeof(pppoe_session))) == NULL) {
		pppoed_log(_pppoed, LOG_ERR, "malloc() failed on %s(): %m",
		    __func__);
		goto reigai;
	}

	/* ���å���� Id �κ��� */
	shuffle_cnt = 0;
	do {
		session_id = (int)slist_remove_first(
		    &_pppoed->session_free_list);
		if (session_id != PPPOED_SESSION_SHUFFLE_MARK)
			break;
		PPPOED_ASSERT(shuffle_cnt == 0);
		if (shuffle_cnt++ > 0) {
			pppoed_log(_pppoed, LOG_ERR,
			    "unexpected errror in %s(): session_free_list full",
			    __func__);
			slist_add(&_pppoed->session_free_list,
			    (void *)PPPOED_SESSION_SHUFFLE_MARK);
			goto reigai;
		}
		slist_shuffle(&_pppoed->session_free_list);
		slist_add(&_pppoed->session_free_list,
		    (void *)PPPOED_SESSION_SHUFFLE_MARK);
	} while (1);

	if (pppoe_session_init(session, _pppoed, _this->index, session_id,
	    shost) != 0)
		goto reigai;

	hash_insert(_pppoed->session_hash, (void *)session_id, session);

	if (pppoe_session_recv_PADR(session, tag_list) != 0)
		goto reigai;

	session = NULL;	/* don't free */
	/* FALL THROUGH */
reigai:
	if (session != NULL)
		pppoe_session_fini(session);
	return;
}

static void
pppoed_recv_PADI(pppoed_listener *_this, uint8_t shost[ETHER_ADDR_LEN],
    slist *tag_list) 
{
	int len, accept_any_service_req;
	const char *val, *service_name, *ac_name;
	u_char bufspace[2048];
	u_char sn[2048], ac_name0[40];
	struct pppoe_header pppoe;
	struct pppoe_tlv tlv, *tlv_hostuniq, *tlv0, *tlv_service_name;
	bytebuffer *buf;

	if ((buf = bytebuffer_wrap(bufspace, sizeof(bufspace))) == NULL) {
		pppoed_log(_this->self, LOG_ERR,
		"bytebuffer_wrap() failed on %s(): %m", __func__);
		return;
	}
	bytebuffer_clear(buf);

	tlv_hostuniq = NULL;
	tlv_service_name = NULL;

	service_name = "";
	if ((val = pppoed_listener_config_str(_this, "pppoe.service_name"))
	    != NULL)
		service_name = val;
	accept_any_service_req = pppoed_listener_config_str_equal(_this,
		"pppoe.accept_any_service_request", "true", 1);

	for (slist_itr_first(tag_list); slist_itr_has_next(tag_list);) {
		tlv0 = slist_itr_next(tag_list);
		if (tlv0->type == PPPOE_TAG_HOST_UNIQ)
			tlv_hostuniq = tlv0;
		if (tlv0->type == PPPOE_TAG_SERVICE_NAME) {

			len = tlv0->length;
			if (len >= sizeof(sn))
				goto reigai;

			memcpy(sn, tlv0->value, len);
			sn[len] = '\0';

			if (strcmp(service_name, sn) == 0 ||
			    (sn[0] == '\0' && accept_any_service_req))
				tlv_service_name = tlv0;
		}
	}
	if (tlv_service_name == NULL) {
		pppoed_log(_this->self, LOG_INFO,
		    "Deny PADI from=%02x:%02x:%02x:%02x:%02x:%02x "
		    "service-name=%s host-uniq=%s if=%s: serviceName is "
		    "not allowed.", shost[0], shost[1],
		    shost[2], shost[3], shost[4], shost[5], sn, tlv_hostuniq?
		    pppoed_tlv_value_string(tlv_hostuniq) : "none",
		    _this->listen_ifname);
		goto reigai;
	}

	pppoed_log(_this->self, LOG_INFO,
	    "RecvPADI from=%02x:%02x:%02x:%02x:%02x:%02x service-name=%s "
	    "host-uniq=%s if=%s", shost[0], shost[1], shost[2], shost[3],
	    shost[4], shost[5], sn, tlv_hostuniq?
	    pppoed_tlv_value_string(tlv_hostuniq) : "none",
	    _this->listen_ifname);

	/*
	 * PPPoE Header
	 */
	memset(&pppoe, 0, sizeof(pppoe));
	pppoe.ver = PPPOE_RFC2516_VER;
	pppoe.type = PPPOE_RFC2516_TYPE;
	pppoe.code = PPPOE_CODE_PADO;
	bytebuffer_put(buf, &pppoe, sizeof(pppoe));

	/*
	 * Tag - Service-Name
	 */
	tlv.type = htons(PPPOE_TAG_SERVICE_NAME);
	len = strlen(service_name);
	tlv.length = htons(len);
	bytebuffer_put(buf, &tlv, sizeof(tlv));
	if (len > 0)
		bytebuffer_put(buf, service_name, len);

	/*
	 * Tag - Access Concentrator Name
	 */
	ac_name = pppoed_listener_config_str(_this, "pppoe.ac_name");
	if (ac_name == NULL) {
		/*
		 * use the ethernet address as default AC-Name.
		 * suggested by RFC 2516.
		 */
		snprintf(ac_name0, sizeof(ac_name0),
		    "%02x:%02x:%02x:%02x:%02x:%02x", _this->ether_addr[0],
		    _this->ether_addr[1], _this->ether_addr[2],
		    _this->ether_addr[3], _this->ether_addr[4],
		    _this->ether_addr[5]);
		ac_name = ac_name0;
	}

	tlv.type = htons(PPPOE_TAG_AC_NAME);
	len = strlen(ac_name);
	tlv.length = htons(len);
	bytebuffer_put(buf, &tlv, sizeof(tlv));
	bytebuffer_put(buf, ac_name, len);

	/*
	 * Tag - ac-cookie
	 */
	if (_this->self->acookie_hash != NULL) {
		/*
		 * ac-cookie �μ����ͤ�õ����
		 * (uint32_t �ζ��֤��ͤ��롼�פ��ޤ�)
		 */
		do {
			_this->self->acookie_next += 1;
		}
		while(hash_lookup(_this->self->acookie_hash,
		    (void *)_this->self->acookie_next) != NULL);

		tlv.type = htons(PPPOE_TAG_AC_COOKIE);
		tlv.length = ntohs(sizeof(uint32_t));
		bytebuffer_put(buf, &tlv, sizeof(tlv));
		bytebuffer_put(buf, &_this->self->acookie_next,
		    sizeof(uint32_t));
	}

	/*
	 * Tag - Host-Uniq
	 */
	if (tlv_hostuniq != NULL) {
		tlv.type = htons(PPPOE_TAG_HOST_UNIQ);
		tlv.length = ntohs(tlv_hostuniq->length);
		bytebuffer_put(buf, &tlv, sizeof(tlv));
		bytebuffer_put(buf, tlv_hostuniq->value,
		    tlv_hostuniq->length);
	}

	/*
	 * Tag - End-Of-List
	 */
	tlv.type = htons(PPPOE_TAG_END_OF_LIST);
	tlv.length = ntohs(0);
	bytebuffer_put(buf, &tlv, sizeof(tlv));

	bytebuffer_flip(buf);

	if (pppoed_output(_this, shost, bytebuffer_pointer(buf),
	    bytebuffer_remaining(buf)) != 0) {
		pppoed_log(_this->self, LOG_ERR, "pppoed_output() failed:%m");
	}
	pppoed_log(_this->self, LOG_INFO,
	    "SendPADO to=%02x:%02x:%02x:%02x:%02x:%02x serviceName=%s "
	    "acName=%s hostUniq=%s eol if=%s", shost[0], shost[1], shost[2],
	    shost[3], shost[4], shost[5], service_name, ac_name,
	    tlv_hostuniq? pppoed_tlv_value_string(tlv_hostuniq) : "none",
		_this->listen_ifname);
	// FALL THROUGH
reigai:
	bytebuffer_unwrap(buf);
	bytebuffer_destroy(buf);
}

/***********************************************************************
 * ����Ϣ
 ***********************************************************************/
static void
pppoed_log(pppoed *_this, int prio, const char *fmt, ...)
{
	char logbuf[BUFSIZ];
	va_list ap;

	PPPOED_ASSERT(_this != NULL);
	va_start(ap, fmt);
#ifdef	PPPOED_MULITPLE
	snprintf(logbuf, sizeof(logbuf), "pppoed id=%u %s", _this->id, fmt);
#else
	snprintf(logbuf, sizeof(logbuf), "pppoed %s", fmt);
#endif
	vlog_printf(prio, logbuf, ap);
	va_end(ap);
}

#define	NAME_VAL(x)	{ x, #x }
static struct _label_name {
	int		label;
	const char	*name;
} pppoe_code_labels[] = {
	NAME_VAL(PPPOE_CODE_PADI),
	NAME_VAL(PPPOE_CODE_PADO),
	NAME_VAL(PPPOE_CODE_PADR),
	NAME_VAL(PPPOE_CODE_PADS),
	NAME_VAL(PPPOE_CODE_PADT),
#ifdef PPPOED_DEBUG
}, pppoe_tlv_labels[] = {
	NAME_VAL(PPPOE_TAG_END_OF_LIST),
	NAME_VAL(PPPOE_TAG_SERVICE_NAME),
	NAME_VAL(PPPOE_TAG_AC_NAME),
	NAME_VAL(PPPOE_TAG_HOST_UNIQ),
	NAME_VAL(PPPOE_TAG_AC_COOKIE),
	NAME_VAL(PPPOE_TAG_VENDOR_SPECIFIC),
	NAME_VAL(PPPOE_TAG_RELAY_SESSION_ID),
	NAME_VAL(PPPOE_TAG_SERVICE_NAME_ERROR),
	NAME_VAL(PPPOE_TAG_AC_SYSTEM_ERROR),
	NAME_VAL(PPPOE_TAG_GENERIC_ERROR)
#endif
};
#define LABEL_TO_STRING(func_name, label_names, prefix_len)		\
	static const char *						\
	func_name(int code)						\
	{								\
		int i;							\
									\
		for (i = 0; i < countof(label_names); i++) {		\
			if (label_names[i].label == code)		\
				return label_names[i].name + prefix_len;\
		}							\
									\
		return "UNKNOWN";					\
	}
LABEL_TO_STRING(pppoe_code_string, pppoe_code_labels, 11)
#ifdef PPPOED_DEBUG
LABEL_TO_STRING(pppoe_tag_string, pppoe_tlv_labels, 10)
#endif

const char *
pppoed_tlv_value_string(struct pppoe_tlv *tlv)
{
	int i;
	char buf[3];
	static char _tlv_string_value[8192];

	_tlv_string_value[0] = '\0';
	for (i = 0; i < tlv->length; i++) {
		snprintf(buf, sizeof(buf), "%02x", tlv->value[i]);
		strlcat(_tlv_string_value, buf,
		    sizeof(_tlv_string_value));
	}
	return _tlv_string_value;
}

/***********************************************************************
 * ��¿�ʴؿ�
 ***********************************************************************/
static int
session_id_cmp(void *a, void *b)
{
	int ia, ib;

	ia = (int)a;
	ib = (int)b;

	return ib - ia;
}

static uint32_t
session_id_hash(void *a, size_t siz)
{
	int ia;
	
	ia = (int)a;

	return ia % siz;
}
