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
/* $Id: ppp.c 39106 2010-01-10 21:01:39Z yasuoka $ */
/**@file
 * {@link :: _npppd_ppp PPP���󥹥���} �˴ؤ���������󶡤��ޤ���
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/time.h>
#include <time.h>
#include <event.h>

#include "slist.h"

#include "npppd.h"
#include "time_utils.h"
#include "ppp.h"
#include "psm-opt.h"

#include "debugutil.h"

#ifdef	PPP_DEBUG
#define	PPP_DBG(x)	ppp_log x
#define	PPP_ASSERT(cond)					\
	if (!(cond)) {						\
	    fprintf(stderr,					\
		"\nASSERT(" #cond ") failed on %s() at %s:%d.\n"\
		, __func__, __FILE__, __LINE__);		\
	    abort(); 						\
	}
#else
#define	PPP_ASSERT(cond)			
#define	PPP_DBG(x)
#endif

#include "debugutil.h"

static u_int32_t ppp_seq = 0;

static void  ppp_stop0 __P((npppd_ppp *));
static int   ppp_recv_packet (npppd_ppp *, unsigned char *, int, int);
static const char * ppp_peer_auth_string(npppd_ppp *);
static void ppp_idle_timeout(int, short, void *);
#ifdef USE_NPPPD_PIPEX
static void ppp_on_network_pipex(npppd_ppp *);
#endif

#define AUTH_IS_PAP(ppp) 	((ppp)->peer_auth == PPP_AUTH_PAP)
#define AUTH_IS_CHAP(ppp)	((ppp)->peer_auth == PPP_AUTH_CHAP_MD5 ||\
				(ppp)->peer_auth == PPP_AUTH_CHAP_MS ||	\
				(ppp)->peer_auth == PPP_AUTH_CHAP_MS_V2)
#define AUTH_IS_EAP(ppp) 	((ppp)->peer_auth == PPP_AUTH_EAP)

/*
 * ��λ����
 *	ppp_lcp_finished	LCP ����λ
 *				������ TermReq
 *				�����餬 TermReq (ppp_stop ����������ܤ�)
 *	ppp_phy_downed		ʪ���ؤ��ڤ줿
 *
 * �ɤ���� ppp_stop0��ppp_down_others ��ƤӽФ��Ƥ��롣
 */
/** npppd_ppp ���֥������Ȥ������� */
npppd_ppp *
ppp_create()
{
	npppd_ppp *_this;

	if ((_this = malloc(sizeof(npppd_ppp))) == NULL) {
		log_printf(LOG_ERR, "malloc() failed in %s(): %m", __func__ );
		return NULL;
	}
	memset(_this, 0, sizeof(npppd_ppp));

	_this->snp.snp_family = AF_INET;
	_this->snp.snp_len = sizeof(_this->snp);
	_this->snp.snp_type = SNP_PPP;
	_this->snp.snp_data_ptr = _this;

	return _this;
}

/**
 * npppd_ppp ���������ޤ���
 * npppd_ppp#mru npppd_ppp#phy_label �ϸƤӽФ����˥��åȤ��Ƥ���������
 */
int
ppp_init(npppd *pppd, npppd_ppp *_this)
{

	PPP_ASSERT(_this != NULL);
	PPP_ASSERT(strlen(_this->phy_label) > 0);

	_this->id = -1;
	_this->ifidx = -1;
	_this->has_acf = 1;
	_this->recv_packet = ppp_recv_packet;
	_this->id = ppp_seq++;
	_this->pppd = pppd;

	lcp_init(&_this->lcp, _this);

	_this->mru = ppp_config_int(_this, "lcp.mru", DEFAULT_MRU);
	if (_this->outpacket_buf == NULL) {
		_this->outpacket_buf = malloc(_this->mru + 64);
		if (_this->outpacket_buf == NULL){
			log_printf(LOG_ERR, "malloc() failed in %s(): %m",
			    __func__);
			return -1;
		}
	}
	_this->adjust_mss = ppp_config_str_equal(_this, "ip.adjust_mss", "true",
	    0);
#ifdef USE_NPPPD_PIPEX
	_this->use_pipex = ppp_config_str_equal(_this, "pipex.enabled", "true",
	    1);
#endif
	/*
	 * ����������ɤ߹��ࡣ
	 */
	_this->log_dump_in =
	    ppp_config_str_equal(_this, "log.in.pktdump",  "true", 0);
	_this->log_dump_out =
	    ppp_config_str_equal(_this, "log.out.pktdump",  "true", 0);


#ifdef	USE_NPPPD_MPPE
	mppe_init(&_this->mppe, _this);
#endif
	ccp_init(&_this->ccp, _this);
	ipcp_init(&_this->ipcp, _this);
	pap_init(&_this->pap, _this);
	chap_init(&_this->chap, _this);

	/* �����ɥ륿���ޡ���Ϣ */
	_this->timeout_sec = ppp_config_int(_this, "idle_timeout", 0);
	if (!evtimer_initialized(&_this->idle_event))
		evtimer_set(&_this->idle_event, ppp_idle_timeout, _this);

	_this->auth_timeout = ppp_config_int(_this, "auth.timeout",
	    DEFAULT_AUTH_TIMEOUT);

	_this->lcp.echo_interval = ppp_config_int(_this,
	    "lcp.echo_interval", DEFAULT_LCP_ECHO_INTERVAL);
	_this->lcp.echo_max_retries = ppp_config_int(_this,
	    "lcp.echo_max_retries", DEFAULT_LCP_ECHO_MAX_RETRIES);
	_this->lcp.echo_retry_interval = ppp_config_int(_this,
	    "lcp.echo_retry_interval", DEFAULT_LCP_ECHO_RETRY_INTERVAL);

	return 0;
}

static void
ppp_set_tunnel_label(npppd_ppp *_this, char *buf, int lbuf)
{
	int flag, af;
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

	hbuf[0] = 0;
	sbuf[0] = 0;
	af = ((struct sockaddr *)&_this->phy_info)->sa_family;
	if (af < AF_MAX) {
		flag = NI_NUMERICHOST;
		if (af == AF_INET || af == AF_INET6)
			flag |= NI_NUMERICSERV;
		if (getnameinfo((struct sockaddr *)&_this->phy_info,
		    ((struct sockaddr *)&_this->phy_info)->sa_len, hbuf,
		    sizeof(hbuf), sbuf, sizeof(sbuf), flag) != 0) {
			ppp_log(_this, LOG_ERR, "getnameinfo() failed at %s",
			    __func__);
			strlcpy(hbuf, "0.0.0.0", sizeof(hbuf));
			strlcpy(sbuf, "0", sizeof(sbuf));
		}
		if (af == AF_INET || af == AF_INET6)
			snprintf(buf, lbuf, "%s:%s", hbuf, sbuf);
		else
			snprintf(buf, lbuf, "%s", hbuf);
	} else if (af == NPPPD_AF_PHONE_NUMBER) {
		strlcpy(buf,
		    ((npppd_phone_number *)&_this->phy_info)->pn_number, lbuf);
	}
}
/**
 * npppd_ppp �򳫻Ϥ��ޤ���
 * npppd_ppp#phy_context
 * npppd_ppp#send_packet
 * npppd_ppp#phy_close
 * npppd_ppp#phy_info
 * �ϸƤӽФ����˥��åȤ��Ƥ���������
 */
void
ppp_start(npppd_ppp *_this)
{
	char label[512];

	PPP_ASSERT(_this != NULL);
	PPP_ASSERT(_this->recv_packet != NULL);
	PPP_ASSERT(_this->send_packet != NULL);
	PPP_ASSERT(_this->phy_close != NULL);

	_this->start_time = time(NULL);
	_this->start_monotime = get_monosec();
	/*
	 * ���̥쥤��ξ������˻Ĥ���
	 */
	ppp_set_tunnel_label(_this, label, sizeof(label));
	ppp_log(_this, LOG_INFO, "logtype=Started tunnel=%s(%s)",
	    _this->phy_label, label);

	lcp_lowerup(&_this->lcp);
}

/**
 * Dialin proxy �ν����򤷤ޤ���dialin proxy �Ǥ��ʤ����ˤϡ�0 �ʳ���
 * �֤�ޤ���
 */
int
ppp_dialin_proxy_prepare(npppd_ppp *_this, dialin_proxy_info *dpi)
{
	int renego_force, renego;

	renego = (ppp_config_str_equal(_this,
	    "l2tp.dialin.lcp_renegotiation", "disable", 0))? 0 : 1;
	renego_force = ppp_config_str_equal(_this,
	    "l2tp.dialin.lcp_renegotiation", "force", 0);
	if (renego_force)
		renego = 1;

	if (lcp_dialin_proxy(&_this->lcp, dpi, renego, renego_force) != 0) {
		ppp_log(_this, LOG_ERR, 
		    "Failed to proxy-dialin, proxied lcp is broken.");
		return 1;
	}

	return 0;
}

static void
ppp_down_others(npppd_ppp *_this)
{
	fsm_lowerdown(&_this->ccp.fsm);
	fsm_lowerdown(&_this->ipcp.fsm);

	npppd_release_ip(_this->pppd, _this);
	if (AUTH_IS_PAP(_this))
		pap_stop(&_this->pap);
	if (AUTH_IS_CHAP(_this))
		chap_stop(&_this->chap);
#ifdef USE_NPPPD_EAP_RADIUS 
	if (AUTH_IS_EAP(_this))
		eap_stop(&_this->eap);
#endif
	evtimer_del(&_this->idle_event);
}

void
ppp_stop(npppd_ppp *_this, const char *reason)
{
	ppp_stop_ex(_this, reason, PPP_DISCON_NO_INFORMATION, 0, 0, NULL);
}

/**
 * PPP ����ߤ���npppd_ppp ���֥������Ȥ��˴����ޤ���
 * @param reason	��ߤ���ͳ���ä���ͳ���ʤ���� NULL ����ꤷ�ޤ���
 *	�����ͤ� LCP �� TermReq �ѥ��åȤ� reason �ե�����ɤ˳�Ǽ����ơ�
 *	���������Τ���ޤ���
 * @param code		disconnect code in {@link ::npppd_ppp_disconnect_code}.
 * @param proto		control protocol number.  see RFC3145.
 * @param direction	disconnect direction.  see RFC 3145
 */
void
ppp_stop_ex(npppd_ppp *_this, const char *reason,
    npppd_ppp_disconnect_code code, int proto, int direction,
    const char *message)
{
	PPP_ASSERT(_this != NULL);

	if (_this->disconnect_code == PPP_DISCON_NO_INFORMATION) {
		_this->disconnect_code = code;
		_this->disconnect_proto = proto;
		_this->disconnect_direction = direction;
		_this->disconnect_message = message;
	}
	ppp_down_others(_this);
	fsm_close(&_this->lcp.fsm, reason);
}

static void
ppp_stop0(npppd_ppp *_this)
{
	char mppe_str[BUFSIZ];
	char label[512];

	_this->end_monotime = get_monosec();

	if (_this->phy_close != NULL)
		_this->phy_close(_this);
	_this->phy_close = NULL;

	/*
	 * PPTP(GRE) �� NAT/�֥�å��ۡ��븡��
	 */
	if (_this->lcp.dialin_proxy != 0 &&
	    _this->lcp.dialin_proxy_lcp_renegotiation == 0) {
		/*
		 * dialin-proxy���ƥͥ������������̵���Ǥ� LCP�Τ��Ȥ��
		 * �ʤ�
		 */
	} else if (_this->lcp.recv_ress == 0) {	// �����ʤ�
		if (_this->lcp.recv_reqs == 0)	// �׵�ʤ�
			ppp_log(_this, LOG_WARNING, "no PPP frames from the "
			    "peer.  router/NAT issue? (may have filtered out)");
		else
			ppp_log(_this, LOG_WARNING, "my PPP frames may not "
			    "have arrived at the peer.  router/NAT issue? (may "
			    "be the only-first-person problem)");
	}
#ifdef USE_NPPPD_PIPEX
	if (npppd_ppp_pipex_disable(_this->pppd, _this) != 0)
		ppp_log(_this, LOG_ERR,
		    "npppd_ppp_pipex_disable() failed: %m");
#endif

	ppp_set_tunnel_label(_this, label, sizeof(label));
#ifdef	USE_NPPPD_MPPE
	if (_this->mppe_started) {
		snprintf(mppe_str, sizeof(mppe_str),
		    "mppe=yes mppe_in=%dbits,%s mppe_out=%dbits,%s",
		    _this->mppe.recv.keybits,
		    (_this->mppe.recv.stateless)? "stateless" : "stateful",
		    _this->mppe.send.keybits,
		    (_this->mppe.send.stateless)? "stateless" : "stateful");
	} else
#endif
		snprintf(mppe_str, sizeof(mppe_str), "mppe=no");
	ppp_log(_this, LOG_NOTICE,
		"logtype=TUNNELUSAGE user=\"%s\" duration=%ldsec layer2=%s "
		"layer2from=%s auth=%s data_in=%qubytes,%upackets "
		"data_out=%qubytes,%upackets error_in=%u error_out=%u %s "
		"iface=%s",
		_this->username[0]? _this->username : "<unknown>",
		(long)(_this->end_monotime - _this->start_monotime),
		_this->phy_label,  label,
		_this->username[0]? ppp_peer_auth_string(_this) : "none",
		_this->ibytes, _this->ipackets, _this->obytes, _this->opackets,
		_this->ierrors, _this->oerrors, mppe_str,
		npppd_ppp_get_iface_name(_this->pppd, _this));

	npppd_ppp_unbind_iface(_this->pppd, _this);
#ifdef	USE_NPPPD_MPPE
	mppe_fini(&_this->mppe);
#endif
	evtimer_del(&_this->idle_event);

	npppd_release_ip(_this->pppd, _this);
	ppp_destroy(_this);
}

/**
 * npppd_ppp ���֥������Ȥ��˴����ޤ���ppp_start �򥳡����ϡ�ppp_stop() ��
 * ����Ѥ������δؿ��ϻȤ��ޤ���
 */
void
ppp_destroy(void *ctx)
{
	npppd_ppp *_this = ctx;

	if (_this->proxy_authen_resp != NULL)
		free(_this->proxy_authen_resp);
	/*
	 * ppp_stop ���Ƥ⡢�������� PPP �ե졼�ब�Ϥ����ޤ����Ϥ��Ƥ���
	 * �äƤ����礬����Τǡ����� down, stop
	 */
	fsm_lowerdown(&_this->ccp.fsm);
	fsm_lowerdown(&_this->ipcp.fsm);
	pap_stop(&_this->pap);
	chap_stop(&_this->chap);

	if (_this->outpacket_buf != NULL)
		free(_this->outpacket_buf);

	free(_this);
}

/************************************************************************
 * �ץ�ȥ���˴ؤ��륤�٥��
 ************************************************************************/
static const char *
ppp_peer_auth_string(npppd_ppp *_this)
{
	switch(_this->peer_auth) {
	case PPP_AUTH_PAP:		return "PAP";
	case PPP_AUTH_CHAP_MD5:		return "MD5-CHAP";
	case PPP_AUTH_CHAP_MS:		return "MS-CHAP";
	case PPP_AUTH_CHAP_MS_V2:	return "MS-CHAP-V2";
	case PPP_AUTH_EAP:		return "EAP";
	default:			return "ERROR";
	}
}

/**
 * LCP�����åפ������˸ƤӽФ���ޤ���
 */
void
ppp_lcp_up(npppd_ppp *_this)
{
#ifdef USE_NPPPD_MPPE
	if (MPPE_REQUIRED(_this) && !MPPE_MUST_NEGO(_this)) {
		ppp_log(_this, LOG_ERR, "MPPE is required, auth protocol must "
		    "be MS-CHAP-V2 or EAP");
		ppp_stop(_this, "Encryption required");
		return;
	}
#endif
	/*
	 * ��꤬�礭�� MRU ����ꤷ�Ƥ⡢��ʬ�� MRU �ʲ��ˤ��롣�����ǡ�
	 * peer_mtu ��̤���ȡ���ϩ MTU ���̤�Τǡ�MRU ��ۤ���褦��
	 * �ѥ��åȤ���ã���ʤ��褦�ˤʤ롣(���Ȥ���Ԥ��Ƥ���)
	 */
	if (_this->peer_mru > _this->mru)
		_this->peer_mru = _this->mru;

	if (_this->peer_auth != 0 && _this->auth_runonce == 0) {
		if (AUTH_IS_PAP(_this)) {
			pap_start(&_this->pap);
			_this->auth_runonce = 1;
			return;
		}
		if (AUTH_IS_CHAP(_this)) {
			chap_start(&_this->chap);
			_this->auth_runonce = 1;
			return;
		}
#ifdef USE_NPPPD_EAP_RADIUS
                if (AUTH_IS_EAP(_this)) {
                        eap_init(&_this->eap, _this);
                        eap_start(&_this->eap);
                        return;
                }
#endif
	}
	if (_this->peer_auth == 0)
		ppp_auth_ok(_this);
}

/**
 * LCP����λ�������˸ƤӽФ���ޤ���
 * <p>STOPPED �ޤ� CLOSED ���ơ��Ȥ����ä����˸ƤӽФ���ޤ���</p>
 */
void
ppp_lcp_finished(npppd_ppp *_this)
{
	PPP_ASSERT(_this != NULL);

	ppp_down_others(_this);

	fsm_lowerdown(&_this->lcp.fsm);
	ppp_stop0(_this);
}

/**
 * ʪ���ؤ����Ǥ��줿����ʪ���ؤ���ƤӽФ���ޤ���
 * <p>
 * ʪ���ؤ� PPP�ե졼��������ϤǤ��ʤ��Ȥ��������Ǥ��δؿ���ƤӽФ���
 * �����������»�Ū�� PPP �����Ǥ�����ˤϡ�{@link ::#ppp_stop} ��Ȥ�
 * �ޤ���</p>
 */
void
ppp_phy_downed(npppd_ppp *_this)
{
	PPP_ASSERT(_this != NULL);

	ppp_down_others(_this);
	fsm_lowerdown(&_this->lcp.fsm);
	fsm_close(&_this->lcp.fsm, NULL);

	ppp_stop0(_this);
}

static const char *
proto_name(uint16_t proto)
{
	switch (proto) {
	case PPP_PROTO_IP:		return "ip";
	case PPP_PROTO_LCP:		return "lcp";
	case PPP_PROTO_PAP:		return "pap";
	case PPP_PROTO_CHAP:		return "chap";
	case PPP_PROTO_EAP:		return "eap";
	case PPP_PROTO_MPPE:		return "mppe";
	case PPP_PROTO_NCP | NCP_CCP:	return "ccp";
	case PPP_PROTO_NCP | NCP_IPCP:	return "ipcp";
	// �ʲ���������
	case PPP_PROTO_NCP | NCP_IP6CP:	return "ip6cp";
	case PPP_PROTO_ACSP:		return "acsp";
	}
	return "unknown";
}

/** ǧ�ڤ������������˸ƤӽФ���ޤ���*/
void
ppp_auth_ok(npppd_ppp *_this)
{
	if (npppd_ppp_bind_iface(_this->pppd, _this) != 0) {
		ppp_log(_this, LOG_WARNING, "No interface binding.");
		ppp_stop(_this, NULL);

		return;
	}
	if (_this->realm != NULL) {
		npppd_ppp_get_username_for_auth(_this->pppd, _this,
		    _this->username, _this->username);
		if (!npppd_check_calling_number(_this->pppd, _this)) {
			ppp_log(_this, LOG_ALERT,
			    "logtype=TUNNELDENY user=\"%s\" "
			    "reason=\"Calling number check is failed\"",
			    _this->username);
			    /* XXX */
			ppp_stop(_this, NULL);
			return;
		}
	}
	if (_this->peer_auth != 0) {
		/* �桼����κ�����³�������¤��� */
		if (!npppd_check_user_max_session(_this->pppd, _this)) {
#ifdef IDGW
			ppp_log(_this, LOG_ALERT, "logtype=TUNNELDENY user=\"%s\" "
			    "reason=\"PPP duplicate login limit exceeded\"",
			    _this->username);
#else
			ppp_log(_this, LOG_WARNING,
			    "user %s exceeds user-max-session limit",
			    _this->username);
#endif
			ppp_stop(_this, NULL);

			return;
		}
		PPP_ASSERT(_this->realm != NULL);
	}

	if (!npppd_ppp_iface_is_ready(_this->pppd, _this)) {
		ppp_log(_this, LOG_WARNING,
		    "interface '%s' is not ready.",
		    npppd_ppp_get_iface_name(_this->pppd, _this));
		ppp_stop(_this, NULL);

		return;
	}
	if (_this->proxy_authen_resp != NULL) {
		free(_this->proxy_authen_resp);
		_this->proxy_authen_resp = NULL;
	}

	fsm_lowerup(&_this->ipcp.fsm);
	fsm_open(&_this->ipcp.fsm);
#ifdef	USE_NPPPD_MPPE
	if (MPPE_MUST_NEGO(_this)) {
		fsm_lowerup(&_this->ccp.fsm);
		fsm_open(&_this->ccp.fsm);
	}
#endif

	return;
}

/** event ���饳����Хå�����륤�٥�ȥϥ�ɥ�Ǥ� */
static void
ppp_idle_timeout(int fd, short evtype, void *context)
{
	npppd_ppp *_this;

	_this = context;

	ppp_log(_this, LOG_NOTICE, "Idle timeout(%d sec)", _this->timeout_sec);
	ppp_stop(_this, NULL);
}

/** �����ɥ륿���ޡ���ꥻ�åȤ��ޤ��������ɥ�Ǥ�̵�����˸ƤӽФ��ޤ��� */
void
ppp_reset_idle_timeout(npppd_ppp *_this)
{
	struct timeval tv;

	//PPP_DBG((_this, LOG_INFO, "%s", __func__));
	evtimer_del(&_this->idle_event);
	if (_this->timeout_sec > 0) {
		tv.tv_usec = 0;
		tv.tv_sec = _this->timeout_sec;

		evtimer_add(&_this->idle_event, &tv);
	}
}

/** IPCP ����λ�������˸ƤӽФ���ޤ� */
void
ppp_ipcp_opened(npppd_ppp *_this)
{
	time_t curr_time;

	curr_time = get_monosec();

	npppd_set_ip_enabled(_this->pppd, _this, 1);
	if (_this->logged_acct_start == 0) {
		char label[512], ipstr[64];

		ppp_set_tunnel_label(_this, label, sizeof(label));

		strlcpy(ipstr, " ip=", sizeof(ipstr));
		strlcat(ipstr, inet_ntoa(_this->ppp_framed_ip_address),
		    sizeof(ipstr));
		if (_this->ppp_framed_ip_netmask.s_addr != 0xffffffffL) {
			strlcat(ipstr, ":", sizeof(ipstr));
			strlcat(ipstr, inet_ntoa(_this->ppp_framed_ip_netmask),
			    sizeof(ipstr));
		}

		ppp_log(_this, LOG_NOTICE,
		    "logtype=TUNNELSTART user=\"%s\" duration=%lusec layer2=%s "
 		    "layer2from=%s auth=%s %s iface=%s%s",
		    _this->username[0]? _this->username : "<unknown>",
		    (long)(curr_time - _this->start_monotime),
		    _this->phy_label, label,
		    _this->username[0]? ppp_peer_auth_string(_this) : "none",
 		    ipstr, npppd_ppp_get_iface_name(_this->pppd, _this),
		    (_this->lcp.dialin_proxy != 0)? " dialin_proxy=yes" : ""
		    );
		_this->logged_acct_start = 1;
		ppp_reset_idle_timeout(_this);
	}
#ifdef USE_NPPPD_PIPEX
	ppp_on_network_pipex(_this);
#endif
}

/** CCP �� Opened �ˤʤä����˸ƤӽФ���ޤ���*/
void
ppp_ccp_opened(npppd_ppp *_this)
{
#ifdef USE_NPPPD_MPPE
	if (_this->ccp.mppe_rej == 0) {
		if (_this->mppe_started == 0) {
			mppe_start(&_this->mppe);
		}
	} else {
		ppp_log(_this, LOG_INFO, "mppe is rejected by peer");
		if (_this->mppe.required)
			ppp_stop(_this, "MPPE is requred");
	}
#endif
#ifdef USE_NPPPD_PIPEX
	ppp_on_network_pipex(_this);
#endif
}

/************************************************************************
 * �ͥåȥ�� I/O ��Ϣ
 ************************************************************************/
/**
 * �ѥ��åȼ���
 * @param	flags	���������ѥ��åȤˤĤ��Ƥξ����ե饰��ɽ���ޤ���
 *	���ߡ�PPP_IO_FLAGS_MPPE_ENCRYPTED �����ꤵ����礬����ޤ���
 * @return	������������ 0 ���֤ꡢ���Ԥ������� 1 ���֤�ޤ���
 */
static int
ppp_recv_packet(npppd_ppp *_this, unsigned char *pkt, int lpkt, int flags)
{
	u_char *inp, *inp_proto;
	uint16_t proto;

	PPP_ASSERT(_this != NULL);

	inp = pkt;

	if (lpkt < 4) {
		ppp_log(_this, LOG_DEBUG, "%s(): Rcvd short header.", __func__);
		return 0;
	}


	if (_this->has_acf == 0) {
		/* nothing to do */
	} else if (inp[0] == PPP_ALLSTATIONS && inp[1] == PPP_UI) {
		inp += 2;
	} else {
		/*
		 * Address and Control Field Compression
		 */
		if (!psm_opt_is_accepted(&_this->lcp, acfc) &&
		    _this->logged_no_address == 0) {
			/*
			 * �ѥ��å������ȯ������Ķ��Ǥϡ�������� LCP
			 * ����Ω���Ƥ��ʤ��Τˡ�Windows ¦����λ���Ƥ��ơ�
			 * ACFC ���줿�ѥ��åȤ��Ϥ���
			 */
			ppp_log(_this, LOG_INFO,
			    "%s: Rcvd broken frame.  ACFC is not accepted, "
			    "but received ppp frame that has no address.",
			    __func__);
			/*
			 * Yamaha RTX-1000 �Ǥϡ�ACFC �� Reject ����Τˡ�
			 * �ѥ��åȤ˥��ɥ쥹�����äƤ��ʤ��Τǡ�����
			 * ���̤˽��Ϥ���Ƥ��ޤ���
			 */
			_this->logged_no_address = 1;
		}
	}
	inp_proto = inp;
	if ((inp[0] & 0x01) != 0) {
		/*
		 * Protocol Field Compression
		 */
		if (!psm_opt_is_accepted(&_this->lcp, pfc)) {
			ppp_log(_this, LOG_INFO,
			    "%s: Rcvd broken frame.  No protocol field: "
			    "%02x %02x", __func__, inp[0], inp[1]);
			return 1;
		}
		GETCHAR(proto, inp);
	} else {
		GETSHORT(proto, inp);
	}

	if (_this->log_dump_in != 0 && debug_get_debugfp() != NULL) {
		char buf[256];

		snprintf(buf, sizeof(buf), "log.%s.in.pktdump",
		    proto_name(proto));
		if (ppp_config_str_equal(_this, buf, "true", 0) != 0)  {
			ppp_log(_this, LOG_DEBUG,
			    "PPP input dump proto=%s(%d/%04x)",
			    proto_name(proto), proto, proto);
			show_hd(debug_get_debugfp(), pkt, lpkt);
		}
	}
#ifdef USE_NPPPD_PIPEX
	if (_this->pipex_enabled != 0 &&
	    _this->tunnel_type == PPP_TUNNEL_PPPOE) {
		switch (proto) {
		case PPP_PROTO_IP:
			return 2;		/* handled by PIPEX */
		case PPP_PROTO_NCP | NCP_CCP:
			if (lpkt - (inp - pkt) < 4)
				break;		/* ���顼���� fsm.c �ǽ��� */
			if (*inp == 0x0e ||	/* Reset-Request */
			    *inp == 0x0f	/* Reset-Ack */) {
				return 2;	/* handled by PIPEX */
			}
			/* FALLTHROUGH */
		default:
			break;
		}
	}
#endif /* USE_NPPPD_PIPEX */

	// MPPE �Υ����å�
	switch (proto) {
#ifdef	USE_NPPPD_MPPE
	case PPP_PROTO_IP:
		if ((flags & PPP_IO_FLAGS_MPPE_ENCRYPTED) == 0) {
			if (MPPE_REQUIRED(_this)) {
				/* MPPE ɬ�ܤʤΤˡ��� IP��*/

				if (_this->logged_naked_ip == 0) {
					ppp_log(_this, LOG_INFO,
					    "mppe is required but received "
					    "naked IP.");
					/* ���˻Ĥ��ΤϺǽ�� 1 ����� */
					_this->logged_naked_ip = 1;
				}
				/*
				 * Windows �ϡ�MPPE ̤��Ω��IPCP ��Ω�ξ��֤�
				 * ��IP�ѥ��åȤ��ꤲ�Ƥ��뢨1��CCP ���ѥ��å�
				 * ���ʤɤǳ�Ω���٤줿��硢���Ψ�Ǥ��ξ�
				 * �֤˴٤롣������ ppp_stop �����硢�ѥ��å�
				 * ���������촹����ȯ������Ķ��Ǥϡ��ط�
				 * ����ʤ��ٸ��ݤˤߤ��롣
				 * (��1 ���ʤ��Ȥ� Windows 2000 Pro SP4)
				 ppp_stop(_this, "Encryption is required.");
				 */
				return 1;
			}
			if (MPPE_READY(_this)) {
				/* MPPE ��Ω�����Τˡ��� IP��*/
				ppp_log(_this, LOG_WARNING,
				    "mppe is avaliable but received naked IP.");
			}
		}
		/* else MPPE ��������� */
		break;
	case PPP_PROTO_MPPE:
#ifdef USE_NPPPD_MPPE
		if (_this->mppe_started == 0)  {
#else
		{
#endif
			ppp_log(_this, LOG_ERR,
			    "mppe packet is received but mppe is stopped.");
			return 1;
		}
		break;
#endif
	}

	switch (proto) {
	case PPP_PROTO_IP:
		npppd_network_output(_this->pppd, _this, AF_INET, inp,
		    lpkt - (inp - pkt));
		goto handled;
	case PPP_PROTO_LCP:
		fsm_input(&_this->lcp.fsm, inp, lpkt - (inp - pkt));
		goto handled;
	case PPP_PROTO_PAP:
		pap_input(&_this->pap, inp, lpkt - (inp - pkt));
		goto handled;
	case PPP_PROTO_CHAP:
		chap_input(&_this->chap, inp, lpkt - (inp - pkt));
		goto handled;
#ifdef USE_NPPPD_EAP_RADIUS
	case PPP_PROTO_EAP:
		eap_input(&_this->eap, inp, lpkt - (inp - pkt));
		goto handled;
#endif
#ifdef	USE_NPPPD_MPPE
	case PPP_PROTO_MPPE:
#ifdef USE_NPPPD_PIPEX
		if (_this->pipex_enabled != 0)
			return -1; /* silent discard */
#endif /* USE_NPPPD_PIPEX */
		mppe_input(&_this->mppe, inp, lpkt - (inp - pkt));
		goto handled;
#endif
	default:
		if ((proto & 0xff00) == PPP_PROTO_NCP) {
			switch (proto & 0xff) {
			case NCP_CCP:	/* Compression */
#ifdef	USE_NPPPD_MPPE
				if (MPPE_MUST_NEGO(_this)) {
					fsm_input(&_this->ccp.fsm, inp,
					    lpkt - (inp - pkt));
					goto handled;
				}
				// �ͥ�����ɬ�פΤʤ����� Protocol Reject
#endif
				break;
			case NCP_IPCP:	/* IPCP */
				fsm_input(&_this->ipcp.fsm, inp,
				    lpkt - (inp - pkt));
				goto handled;
			}
		}
	}
	/* ProtoRej ���˻Ĥ� */
	ppp_log(_this, LOG_INFO, "unhandled protocol %s, %d(%04x)",
	    proto_name(proto), proto, proto);

	if ((flags & PPP_IO_FLAGS_MPPE_ENCRYPTED) != 0) {
		/*
		 * Don't return a protocol-reject for the packet was encrypted,
		 * because lcp protocol-reject is not encrypted by mppe.
		 */
	} else {
		/*
		 * as RFC1661: Rejected-Information MUST be truncated to
		 * comply with the peer's established MRU.
		 */
		lcp_send_protrej(&_this->lcp, inp_proto,
		    MIN(lpkt - (inp_proto - pkt), NPPPD_MIN_MRU - 32));
	}

	return 1;
handled:

	return 0;
}

/** PPP�˽��Ϥ�����˸ƤӽФ��ޤ��� */
inline void
ppp_output(npppd_ppp *_this, uint16_t proto, u_char code, u_char id,
    u_char *datap, int ldata)
{
	u_char *outp;
	int outlen, hlen, is_lcp = 0;

	outp = _this->outpacket_buf;

	/* LCP�ϰ��̤�Ȥ�ʤ� */
	is_lcp = (proto == PPP_PROTO_LCP)? 1 : 0;


	if (_this->has_acf == 0 ||
		(!is_lcp && psm_peer_opt_is_accepted(&_this->lcp, acfc))) {
		/*
		 * Address and Control Field (ACF) �����⤽��̵������
		 * ACFC ���ͥ�����Ƥ������ ACF ���ɲä��ʤ���
		 */
	} else {
		PUTCHAR(PPP_ALLSTATIONS, outp); 
		PUTCHAR(PPP_UI, outp); 
	}
	if (!is_lcp && proto <= 0xff &&
	    psm_peer_opt_is_accepted(&_this->lcp, pfc)) {
		/*
		 * Protocol Field Compression
		 */
		PUTCHAR(proto, outp); 
	} else {
		PUTSHORT(proto, outp); 
	}
	hlen = outp - _this->outpacket_buf;

	if (_this->mru > 0) {
		if (MRU_PKTLEN(_this->mru, proto) < ldata) {
			PPP_DBG((_this, LOG_ERR, "packet too large %d. mru=%d",
			    ldata , _this->mru));
			_this->oerrors++;
			PPP_ASSERT("NOT REACHED HERE" == NULL);
			return;
		}
	}

	if (code != 0) {
		outlen = ldata + HEADERLEN;

		PUTCHAR(code, outp);
		PUTCHAR(id, outp);
		PUTSHORT(outlen, outp);
	} else {
		outlen = ldata;
	}

	if (outp != datap && ldata > 0)
		memmove(outp, datap, ldata);

	if (_this->log_dump_out != 0 && debug_get_debugfp() != NULL) {
		char buf[256];

		snprintf(buf, sizeof(buf), "log.%s.out.pktdump",
		    proto_name(proto));
		if (ppp_config_str_equal(_this, buf, "true", 0) != 0)  {
			ppp_log(_this, LOG_DEBUG,
			    "PPP output dump proto=%s(%d/%04x)",
			    proto_name(proto), proto, proto);
			show_hd(debug_get_debugfp(),
			    _this->outpacket_buf, outlen + hlen);
		}
	}
	_this->send_packet(_this, _this->outpacket_buf, outlen + hlen, 0);
}

/**
 * PPP �����ѤΥХåե��ΰ���֤��ޤ����إå����̤ˤ�륺����������ޤ���
 * �Хåե��ΰ��Ĺ���� npppd_ppp#mru �ʾ�Ǥ���
 */
u_char *
ppp_packetbuf(npppd_ppp *_this, int proto)
{
	int save;

	save = 0;
	if (proto != PPP_PROTO_LCP) {
		if (psm_peer_opt_is_accepted(&_this->lcp, acfc))
			save += 2;
		if (proto <= 0xff && psm_peer_opt_is_accepted(&_this->lcp, pfc))
			save += 1;
	}
	return _this->outpacket_buf + (PPP_HDRLEN - save);
}

/** ���Υ��󥹥��󥹤˴�Ť�����٥뤫��Ϥޤ����Ͽ���ޤ��� */
int
ppp_log(npppd_ppp *_this, int prio, const char *fmt, ...)
{
	int status;
	char logbuf[BUFSIZ];
	va_list ap;

	PPP_ASSERT(_this != NULL);

	va_start(ap, fmt);
	snprintf(logbuf, sizeof(logbuf), "ppp id=%u layer=base %s",
	    _this->id, fmt);
	status = vlog_printf(prio, logbuf, ap);
	va_end(ap);

	return status;
}

#ifdef USE_NPPPD_RADIUS
#define UCHAR_BUFSIZ 255
/**
 * RADIUS �ѥ��åȤ� Framed-IP-Address �����ȥ�ӥ塼�Ȥ� Framed-IP-Netmask
 * �����ȥ�ӥ塼�Ȥ�������ޤ���
 */ 
void
ppp_proccess_radius_framed_ip(npppd_ppp *_this, RADIUS_PACKET *pkt)
{
	struct in_addr ip4;
	
	if (radius_get_ipv4_attr(pkt, RADIUS_TYPE_FRAMED_IP_ADDRESS, &ip4)
	    == 0)
		_this->realm_framed_ip_address = ip4;

	_this->realm_framed_ip_netmask.s_addr = 0xffffffffL;
	if (radius_get_ipv4_attr(pkt, RADIUS_TYPE_FRAMED_IP_NETMASK, &ip4)
	    == 0)
		_this->realm_framed_ip_netmask = ip4;
}

/**
 * RADIUS ǧ���׵��Ѥ� RADIUS�����ȥ�ӥ塼�Ȥ򥻥åȤ��ޤ���
 * �����������ˤ� 0 ���֤�ޤ���
 */
int
ppp_set_radius_attrs_for_authreq(npppd_ppp *_this,
    radius_req_setting *rad_setting, RADIUS_PACKET *radpkt)
{
	/* RFC 2865 "5.4 NAS-IP-Address" or RFC3162 "2.1. NAS-IPv6-Address" */
	if (radius_prepare_nas_address(rad_setting, radpkt) != 0)
		goto reigai;

	/* RFC 2865 "5.6. Service-Type" */
	if (radius_put_uint32_attr(radpkt, RADIUS_TYPE_SERVICE_TYPE,
	    RADIUS_SERVICE_TYPE_FRAMED) != 0)
		goto reigai;

	/* RFC 2865 "5.7. Framed-Protocol" */
	if (radius_put_uint32_attr(radpkt, RADIUS_TYPE_FRAMED_PROTOCOL, 
	    RADIUS_FRAMED_PROTOCOL_PPP) != 0)
		goto reigai;

	if (_this->calling_number[0] != '\0') {
		if (radius_put_string_attr(radpkt,
		    RADIUS_TYPE_CALLING_STATION_ID, _this->calling_number) != 0)
			return 1;
	}
	return 0;
reigai:
	return 1;
}
#endif

#ifdef USE_NPPPD_PIPEX
/** Network ��ͭ���ˤʤä����� callback �ؿ� PIPEX ��*/
static void
ppp_on_network_pipex(npppd_ppp *_this)
{
	if (_this->use_pipex == 0)
		return;	
	if (_this->tunnel_type != PPP_TUNNEL_PPTP &&
	    _this->tunnel_type != PPP_TUNNEL_PPPOE)
		return;
	if (_this->pipex_started != 0)
		return;	/* already started */

	PPP_DBG((_this, LOG_INFO, "%s() assigned_ip4_enabled = %s, "
	    "MPPE_MUST_NEGO = %s, ccp.fsm.state = %s", __func__,
	    (_this->assigned_ip4_enabled != 0)? "true" : "false",
	    (MPPE_MUST_NEGO(_this))? "true" : "false",
	    (_this->ccp.fsm.state == OPENED)? "true" : "false"));

	if (_this->assigned_ip4_enabled != 0 &&
	    (!MPPE_MUST_NEGO(_this) || _this->ccp.fsm.state == OPENED)) {
		/* IPCP ����λ����MPPE ���פޤ��� MPPE ��λ������� */
		npppd_ppp_pipex_enable(_this->pppd, _this);
		ppp_log(_this, LOG_NOTICE, "Using pipex=%s",
		    (_this->pipex_enabled != 0)? "yes" : "no");
		_this->pipex_started = 1;
	}
	/* else CCP or IPCP �Ԥ� */
}
#endif

#ifdef	NPPPD_USE_CLIENT_AUTH
#ifdef USE_NPPPD_LINKID
#include "linkid.h"
#endif
/** ü��ID�򥻥åȤ��ޤ� */
void
ppp_set_client_auth_id(npppd_ppp *_this, const char *client_auth_id)
{
	PPP_ASSERT(_this != NULL);
	PPP_ASSERT(client_auth_id != NULL);
	PPP_ASSERT(strlen(client_auth_id) <= NPPPD_CLIENT_AUTH_ID_MAXLEN);

	strlcpy(_this->client_auth_id, client_auth_id,
	    sizeof(_this->client_auth_id));
	_this->has_client_auth_id = 1;
#ifdef USE_NPPPD_LINKID
	linkid_purge(_this->ppp_framed_ip_address);
#endif
	ppp_log(_this, LOG_NOTICE,
	    "Set client authentication id successfully.  linkid=\"%s\" client_auth_id=%s",
	    _this->username, client_auth_id);
}
#endif
