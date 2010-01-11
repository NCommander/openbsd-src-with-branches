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
#ifndef	_NPPPD_LOCAL_H
#define	_NPPPD_LOCAL_H	1

#ifndef	NPPPD_BUFSZ	
/** �Хåե������� */
#define	NPPPD_BUFSZ			BUFSZ
#endif

#include <sys/param.h>
#include <net/if.h>

#include "npppd_defs.h"

#include "slist.h"
#include "hash.h"
#include "properties.h"

#ifdef	USE_NPPPD_RADIUS
#include <radius+.h>
#include "radius_req.h"
#endif

#ifdef	USE_NPPPD_L2TP
#include "debugutil.h"
#include "bytebuf.h"
#include "l2tp.h"
#endif

#ifdef	USE_NPPPD_PPTP
#include "bytebuf.h"
#include "pptp.h"
#endif
#ifdef	USE_NPPPD_PPPOE
#if defined(__NetBSD__)
#include <net/if_ether.h>
#else
#include <netinet/if_ether.h>
#endif
#include "bytebuf.h"
#include "pppoe.h"
#endif
#include "npppd_auth.h"
#include "npppd_iface.h"
#include "npppd.h"

#ifdef	USE_NPPPD_NPPPD_CTL
typedef struct _npppd_ctl {
	/** ���٥�ȥ���ƥ����� */
	struct event ev_sock;
	/** �����å� */
	int sock;
	/** ͭ��/̵�� */
	int enabled;
	/** �� npppd */
	void *npppd;
	/** �����åȤΥѥ�̾ */
	char pathname[MAXPATHLEN];
	/** �����å�����Ĺ */
	int max_msgsz;
} npppd_ctl;
#endif

#include "addr_range.h"
#include "npppd_pool.h"

/** �ס���򼨤��� */
struct _npppd_pool {
	/** ��Ȥʤ� npppd */
	npppd		*npppd;
	/** ��٥�̾ */
	char		label[NPPPD_GENERIC_NAME_LEN];
	/** ̾��(name) */
	char		name[NPPPD_GENERIC_NAME_LEN];
	/** sockaddr_npppd ����Υ����� */
	int		addrs_size;
	/** sockaddr_npppd ���� */
	struct sockaddr_npppd *addrs;
	/** ưŪ�˳�����Ƥ륢�ɥ쥹�Υꥹ�� */
	slist 		dyna_addrs;
	int		/** ������� */
			initialized:1,
			/** ������ */
			running:1;
};

/** IPCP����򼨤��� */
typedef struct _npppd_ipcp_config {
	/** ̾�� */
	char	name[NPPPD_GENERIC_NAME_LEN];
	/** ��٥�(ɳ�դ��뤿��) */
	char	label[NPPPD_GENERIC_NAME_LEN];
	/** �� npppd �ؤΥݥ��� */
	npppd	*npppd;
	/**
	 * �ץ饤�ޥ�DNS�����С����������Τ��ʤ����� INADDR_NONE��
	 * �ͥåȥ���Х��ȥ���������
	 */
	struct in_addr	dns_pri;

	/** ���������DNS�����С����������Τ��ʤ����� INADDR_NONE��
	 * �ͥåȥ���Х��ȥ���������
	 */
	struct in_addr	dns_sec;

	/**
	 * �ץ饤�ޥ�WINS�����С����������Τ��ʤ����� INADDR_NONE��
	 * �ͥåȥ���Х��ȥ���������
	 */
	struct in_addr	nbns_pri;

	/**
	 * ���������WINS�����С����������Τ��ʤ����� INADDR_NONE��
	 * �ͥåȥ���Х��ȥ���������
	 */
	struct in_addr	nbns_sec;

	/**
	 * IP���ɥ쥹���������ˡ�Υӥåȥե饰��
	 * @see	#NPPPD_IP_ASSIGN_FIXED
	 * @see	#NPPPD_IP_ASSIGN_USER_SELECT
	 * @see	#NPPPD_IP_ASSIGN_RADIUS
	 */
	int 		ip_assign_flags;

	int		/** DNS �����ФȤ��ƥȥ�ͥ뽪ü���ɥ쥹��Ȥ� */
			dns_use_tunnel_end:1,
			/** ������Ѥ��ɤ��� */
			initialized:1,
			reserved:30;
} npppd_ipcp_config;

/** ���󥿥ե������� IPCP �����ס��륢�ɥ쥹�ؤλ��Ȥ��ݻ����뷿 */
typedef struct _npppd_iface_binding {
	npppd_ipcp_config	*ipcp;
	slist			pools;
} npppd_iface_binding;

/**
 * npppd
 */
struct _npppd {
	/** ���٥�ȥϥ�ɥ顼 */
	struct event ev_sigterm, ev_sigint, ev_sighup, ev_timer;

	/** PPP���󤹤륤�󥿡��ե����� */
	npppd_iface		iface[NPPPD_MAX_IFACE];
	/** ���󥿥ե������� IPCP �����ס��륢�ɥ쥹�ؤλ��� */
	npppd_iface_binding	iface_bind[NPPPD_MAX_IFACE];

	/** ���ɥ쥹�ס��� */
	npppd_pool		pool[NPPPD_MAX_POOL];

	/** radish �ס��롢������ƥ��ɥ쥹������ */
	struct radish_head *rd;

	/** IPCP ���� */
	npppd_ipcp_config ipcp_config[NPPPD_MAX_IPCP_CONFIG];

	/** �桼��̾ �� slist of npppd_ppp �Υޥå� */
	hash_table *map_user_ppp;

	/** ǧ�ڥ��� */
	slist realms;

	/** ǧ�ڥ��ཪλ�������Υ��󥿡��Х����(sec) */
	int auth_finalizer_itvl;

	/** ����ե�����̾ */
	char 	config_file[MAXPATHLEN];

	/** PID�ե�����̾ */
	char 	pidpath[MAXPATHLEN];

	/** �ץ��� ID */
	pid_t	pid;

#ifdef	USE_NPPPD_L2TP
	/** L2TP �ǡ���� */
	l2tpd l2tpd;
#endif
#ifdef	USE_NPPPD_PPTP
	/** PPTP �ǡ���� */
	pptpd pptpd;
#ifdef	IDGW_SSLDIP
	/** SSLDIP �� PPTP �ǡ���� */
	pptpd ssldipd;
#endif
#endif
#ifdef	USE_NPPPD_PPPOE
	/** PPPOE �ǡ���� */
	pppoed pppoed;
#endif
	/** ����ե����� */
	struct properties * properties;

	/** �桼������ե����� */
	struct properties * users_props;

#ifdef	USE_NPPPD_NPPPD_CTL
	npppd_ctl ctl;
#endif
	/** ��ư���Ƥ�����ÿ���*/
	uint32_t	secs;

	/** ������ɤ߹��ߤ���ͱͽ���뤫 */
	int16_t		delayed_reload;
	/** ������ɤ߹��ߥ����� */
	int16_t		reloading_count;

	/** �����ѤߤΥ롼�ƥ��󥰥��٥�ȥ��ꥢ�� */
	int		rtev_event_serial;

	/** ��³�Ǥ������� PPP���å����� */
	int		max_session;

	int /** ��λ������ */
	    finalizing:1,
	    /** ��λ������λ */
	    finalized:1;
};

#ifndef	NPPPD_CONFIG_BUFSIZ
#define	NPPPD_CONFIG_BUFSIZ	65536	// 64K
#endif
#ifndef	NPPPD_KEY_BUFSIZ
#define	NPPPD_KEY_BUFSIZ	512
#endif
#define	ppp_iface(ppp)	(&(ppp)->pppd->iface[(ppp)->ifidx])
#define	ppp_ipcp(ppp)	((ppp)->pppd->iface_bind[(ppp)->ifidx].ipcp)
#define	ppp_pools(ppp)	(&(ppp)->pppd->iface_bind[(ppp)->ifidx].pools)

#define	SIN(sa)		((struct sockaddr_in *)(sa))

#define	TIMER_TICK_RUP(interval)			\
	((((interval) % NPPPD_TIMER_TICK_IVAL) == 0)	\
	    ? (interval)				\
	    : (interval) + NPPPD_TIMER_TICK_IVAL	\
		- ((interval) % NPPPD_TIMER_TICK_IVAL))

#ifdef	USE_NPPPD_NPPPD_CTL
void  npppd_ctl_init (npppd_ctl *, npppd *, const char *);
int   npppd_ctl_start (npppd_ctl *);
void  npppd_ctl_stop (npppd_ctl *);
#endif
#define	sin46_port(x)	(((x)->sa_family == AF_INET6)	\
	? ((struct sockaddr_in6 *)(x))->sin6_port		\
	: ((struct sockaddr_in *)(x))->sin_port)


#endif
