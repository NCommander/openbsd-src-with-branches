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
 * Next pppd(nppd). This file provides a npppd daemon process and operations
 * for npppd instance.
 * @author	Yasuoka Masahiko
 * $Id: npppd.c,v 1.3 2010/01/31 05:49:51 yasuoka Exp $
 */
#include <sys/cdefs.h>
#include "version.h"
#ifndef LINT
__COPYRIGHT(
"@(#) npppd - PPP daemon for PPP Access Concentrators\n"
"@(#) Version " VERSION "\n" 
"@(#) \n"
"@(#) Copyright 2005-2008\n"
"@(#) 	Internet Initiative Japan Inc.  All rights reserved.\n"
"@(#) \n"
"@(#) \n"
"@(#) \n"
);
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/route.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <unistd.h>
#include <time.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <netdb.h>
#include <libgen.h>
#include <fcntl.h>
#include <event.h>
#include <errno.h>
#include <ifaddrs.h>
#include <err.h>
#include <pwd.h>

#include "pathnames.h"
#include "debugutil.h"
#include "addr_range.h"
#include "npppd_subr.h"
#include "npppd_local.h"
#include "npppd_auth.h"
#include "radish.h"
#include "rtev.h"
#ifdef NPPPD_USE_RT_ZEBRA
#include "rt_zebra.h"
#endif
#include "net_utils.h"
#include "time_utils.h"

#ifdef USE_NPPPD_LINKID
#include "linkid.h"
#endif

#ifdef USE_NPPPD_ARP
#include "npppd_arp.h"
#endif

#ifdef USE_NPPPD_PIPEX
#ifdef USE_NPPPD_PPPOE
#include "pppoe_local.h"
#endif /* USE_NPPPD_PPPOE */
#include "psm-opt.h"
#include <sys/ioctl.h>
#if defined(__NetBSD__)
#include <net/if_ether.h>
#else
#include <netinet/if_ether.h>
#endif
#include <net/pipex.h>
#endif /* USE_NPPPD_PIPEX */

static npppd s_npppd;	/* singleton */

static void            npppd_reload0(npppd *);
static int             npppd_rd_walktree_delete(struct radish_head *);
static void            usage (void);
static void            npppd_start (npppd *);
static void            npppd_stop_really (npppd *);
static uint32_t        str_hash(const void *, int);
static void            npppd_on_sighup (int, short, void *);
static void            npppd_on_sigterm (int, short, void *);
static void            npppd_on_sigint (int, short, void *);
static void            npppd_reset_timer(npppd *);
static void            npppd_timer(int, short, void *);
static void	       npppd_auth_finalizer_periodic(npppd *);
static int  rd2slist_walk (struct radish *, void *);
static int  rd2slist (struct radish_head *, slist *);
static inline void     seed_random(long *);

#ifndef	NO_ROUTE_FOR_POOLED_ADDRESS
static struct in_addr loop;	/* initialize at npppd_init() */
#endif
static uint32_t        str_hash(const void *, int);

#ifdef USE_NPPPD_PIPEX
static int npppd_ppp_pipex_ip_disable(npppd *, npppd_ppp *);
static void pipex_periodic(npppd *);
#endif /* USE_NPPPD_PIPEX */

#ifdef NPPPD_DEBUG
#define NPPPD_DBG(x) 	log_printf x
#define NPPPD_ASSERT(x) ASSERT(x)
#else
#define NPPPD_DBG(x) 
#define NPPPD_ASSERT(x)
#endif

/***********************************************************************
 * Daemon process
 ***********************************************************************/
int        main (int, char *[]);

int
main(int argc, char *argv[])
{
	int ch, retval = 0, ll_adjust = 0, runasdaemon = 0;
	extern char *optarg;
	const char *npppd_conf0 = DEFAULT_NPPPD_CONF;
	struct passwd *pw;

	while ((ch = getopt(argc, argv, "Dc:dhs")) != -1) {
		switch (ch) {
		case 's':
			ll_adjust++;
			break;
		case 'c':
			npppd_conf0 = optarg;
			break;
		case 'D':
			runasdaemon = 1;
			break;
		case 'd':
			debuglevel++;
			break;
		case '?':
		case 'h':
			usage();
			exit(1);
		}
	}
	if (debuglevel > 0) {
		debug_set_debugfp(stderr);
		debug_use_syslog(0);
	} else {
		debug_set_syslog_level_adjust(ll_adjust);
		openlog(NULL, LOG_PID, LOG_NPPPD);
		if (runasdaemon)
			daemon(0, 0);
	}

	/* check for root privileges */
	if (geteuid())
		errx(1, "need root privileges");
	/* check for npppd user */
	if (getpwnam(NPPPD_USER) == NULL)
		errx(1, "unknown user %s", NPPPD_USER);

	if (privsep_init() != 0)
		err(1, "cannot drop privileges");

	if (npppd_init(&s_npppd, npppd_conf0) != 0) {
		retval = 1;
		goto fail;
	}

	if ((pw = getpwnam(NPPPD_USER)) == NULL)
		err(1, "gwpwnam");
	if (chroot(pw->pw_dir) == -1)
		err(1, "chroot");
	if (chdir("/") == -1)
		err(1, "chdir(\"/\")");
        if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		err(1, "cannot drop privileges");
	/* privileges is dropped */

	npppd_start(&s_npppd);
	npppd_fini(&s_npppd);
	/* FALLTHROUGH */
fail:
	privsep_fini();
	log_printf(LOG_NOTICE, "Terminate npppd.");

	return retval;
}

static void
usage()
{
	fprintf(stderr,
	    "usage: npppd [-sDdh] [-c config_file]\n"
	    "\t-d: increase debuglevel.  Output log to standard error.\n"
	    "\t-c: specify configuration file.  default=\"%s\".\n"
	    "\t-s: adjust syslog level to be silent.\n"
	    "\t-D: run as a daemon.\n"
	    "\t-h: show usage.\n"
	    , DEFAULT_NPPPD_CONF
	);
}

/** Returns the singleton npppd instance */
npppd *
npppd_get_npppd()
{
	return &s_npppd;
}

/***********************************************************************
 * Operations to npppd itself (initialize/finalize/start/stop)
 ***********************************************************************/
 /** Initialize the npppd */
int
npppd_init(npppd *_this, const char *config_file)
{
	int i, status = -1;
	char pidpath[MAXPATHLEN];
	const char *pidpath0;
	const char *coredir;
	FILE *pidfp = NULL;
	char	cwd[MAXPATHLEN];
	long seed;

	memset(_this, 0, sizeof(npppd));
	loop.s_addr = htonl(INADDR_LOOPBACK);

	NPPPD_ASSERT(config_file != NULL);

	pidpath0 = NULL;
	_this->pid = getpid();
	slist_init(&_this->realms);

	log_printf(LOG_NOTICE, "Starting npppd pid=%u version=%s",
	    _this->pid, VERSION);
#if defined(BUILD_DATE) && defined(BUILD_TIME)
	log_printf(LOG_INFO, "Build %s %s ", BUILD_DATE, BUILD_TIME);
#endif
	if (get_nanotime() == INT64_MIN) {
		log_printf(LOG_ERR, "get_nanotime() failed: %m");
		return 1;
	}

	if (realpath(config_file, _this->config_file) == NULL) {
		log_printf(LOG_ERR, "realpath(%s,) failed in %s(): %m",
		    config_file, __func__);
		return 1;
	}
	/* we assume 4.4 compatible realpath().  See realpath(3) on BSD. */
	NPPPD_ASSERT(_this->config_file[0] == '/');

	/* initialize random seeds */
	seed_random(&seed);
	srandom(seed);

	/* load configuration */
	if ((status = npppd_reload_config(_this)) != 0)
		return status;

	if ((_this->map_user_ppp = hash_create(
	    (int (*) (const void *, const void *))strcmp, str_hash,
	    NPPPD_USER_HASH_SIZ)) == NULL) {
		log_printf(LOG_ERR, "hash_create() failed in %s(): %m",
		    __func__);
		return -1;
	}

	if (npppd_ifaces_load_config(_this) != 0) {
		return -1;
	}

	if ((pidpath0 = npppd_config_str(_this, "pidfile")) == NULL)
		pidpath0 = DEFAULT_NPPPD_PIDFILE;

	/* Runtime directory */
	if ((coredir = npppd_config_str(_this, "coredir")) == NULL) {
		/* diretory for pid file */
		strlcpy(pidpath, pidpath0, sizeof(pidpath));
		strlcpy(cwd, dirname(pidpath), sizeof(cwd));
	}
	else {
		/* directory for dumping core */
		strlcpy(cwd, coredir, sizeof(cwd));
	}
	if (chdir(cwd) != 0) {
		log_printf(LOG_ERR, "chdir(%s,) failed in %s(): %m", __func__,
		    cwd);
		return -1;
	}

	/* initialize event(3) */
	event_init();

	/* initialize rtev */
	rtev_libevent_init(
	    npppd_config_int(_this, "rtsock.event_delay",
		DEFAULT_RTSOCK_EVENT_DELAY),
	    npppd_config_int(_this, "rtsock.send_wait_millisec",
		DEFAULT_RTSOCK_SEND_WAIT_MILLISEC),
	    npppd_config_int(_this, "rtsock.send_npkts",
		DEFAULT_RTSOCK_SEND_NPKTS), 0);

#ifdef NPPPD_USE_RT_ZEBRA
	if (rt_zebra_get_instance() == NULL)
		return -1;
	rt_zebra_init(rt_zebra_get_instance());
	rt_zebra_start(rt_zebra_get_instance());
#endif
	_this->rtev_event_serial = -1;

	/* ignore signals */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGURG, SIG_IGN);

	/* set signal handlers */
	signal_set(&_this->ev_sigterm, SIGTERM, npppd_on_sigterm, _this);
	signal_set(&_this->ev_sigint, SIGINT, npppd_on_sigint, _this);
	signal_set(&_this->ev_sighup, SIGHUP, npppd_on_sighup, _this);
	signal_add(&_this->ev_sigterm, NULL);
	signal_add(&_this->ev_sigint, NULL);
	signal_add(&_this->ev_sighup, NULL);

	evtimer_set(&_this->ev_timer, npppd_timer, _this);

	/* start tun(4) or pppac(4) */
	status = 0;
	for (i = 0; i < countof(_this->iface); i++) {
		if (_this->iface[i].initialized != 0)
			status |= npppd_iface_start(&_this->iface[i]);
	}
	if (status != 0)
		return -1;

	/*
	 * If the npppd can start(open) interfaces successfully, it can 
	 * act as only one npppd process on the system and overwrite the pid
	 * file.
	 */
	if ((pidfp = fopen(pidpath0, "w+")) == NULL) {
		log_printf(LOG_ERR, "fopen(%s,w+) failed in %s(): %m",
		    pidpath0, __func__);
		return -1;
	}
	strlcpy(_this->pidpath, pidpath0, sizeof(_this->pidpath));
	fprintf(pidfp, "%u\n", _this->pid);
	fclose(pidfp);
	pidfp = NULL;
#ifdef USE_NPPPD_LINKID
	linkid_sock_init();
#endif
#ifdef USE_NPPPD_ARP
	arp_set_strictintfnetwork(npppd_config_str_equali(_this, "arpd.strictintfnetwork", "true", ARPD_STRICTINTFNETWORK_DEFAULT));
	if (npppd_config_str_equali(_this, "arpd.enabled", "true", ARPD_DEFAULT) == 1)
        	arp_sock_init();
#endif
	return npppd_modules_reload(_this);
}

/** start the npppd */
void
npppd_start(npppd *_this)
{
	int rval = 0;

	npppd_reset_timer(_this);
	while ((event_loop(EVLOOP_ONCE)) == 0) {
		if (_this->finalized != 0)
			break;
	}
	if (rval != 0)
		log_printf(LOG_CRIT, "event_loop() failed: %m");
}

/** stop the npppd */
void
npppd_stop(npppd *_this)
{
	int i;
#ifdef	USE_NPPPD_L2TP
	l2tpd_stop(&_this->l2tpd);
#endif
#ifdef	USE_NPPPD_PPTP
	pptpd_stop(&_this->pptpd);
#endif
#ifdef	USE_NPPPD_PPPOE
	pppoed_stop(&_this->pppoed);
#endif
#ifdef	USE_NPPPD_NPPPD_CTL
	npppd_ctl_stop(&_this->ctl);
#endif
#ifdef USE_NPPPD_LINKID
	linkid_sock_fini();
#endif
#ifdef USE_NPPPD_ARP
        arp_sock_fini();
#endif
	for (i = countof(_this->iface) - 1; i >= 0; i--) {
		if (_this->iface[i].initialized != 0)
			npppd_iface_stop(&_this->iface[i]);
	}
	npppd_set_radish(_this, NULL);

	_this->finalizing = 1;
	npppd_reset_timer(_this);
}

static void
npppd_stop_really(npppd *_this)
{
	int i;
#if defined(USE_NPPPD_L2TP) || defined(USE_NPPPD_PPTP)
	int wait_again;

	wait_again = 0;

#ifdef	USE_NPPPD_L2TP
	if (!l2tpd_is_stopped(&_this->l2tpd))
		wait_again |= 1;
#endif
#ifdef	USE_NPPPD_PPTP
	if (!pptpd_is_stopped(&_this->pptpd))
		wait_again |= 1;
#endif
	if (wait_again != 0) {
		npppd_reset_timer(_this);
		return;
	}
#endif
#ifdef NPPPD_USE_RT_ZEBRA
	rt_zebra_stop(rt_zebra_get_instance());
#endif
	for (i = countof(_this->iface) - 1; i >= 0; i--) {
		if (_this->iface[i].initialized != 0)
			npppd_iface_fini(&_this->iface[i]);
	}
	_this->finalized = 1;
}

/** finalize the npppd */
void
npppd_fini(npppd *_this)
{
	int i;
	npppd_auth_base *auth_base;

#ifdef USE_NPPPD_L2TP
	l2tpd_uninit(&_this->l2tpd);
#endif
#ifdef USE_NPPPD_PPTP
	pptpd_uninit(&_this->pptpd);
#endif
#ifdef USE_NPPPD_PPPOE
	pppoed_uninit(&_this->pppoed);
#endif
	for (slist_itr_first(&_this->realms);
	    slist_itr_has_next(&_this->realms);) {
		auth_base = slist_itr_next(&_this->realms);
		npppd_auth_destroy(auth_base);
	}
	for (i = countof(_this->iface) - 1; i >= 0; i--) {
		if (_this->iface[i].initialized != 0)
			npppd_iface_fini(&_this->iface[i]);
	}
	for (i = 0; i < countof(_this->iface_bind); i++)
		slist_fini(&_this->iface_bind[i].pools);

	for (i = countof(_this->pool) - 1; i >= 0; i--) {
		if (_this->pool[i].initialized != 0)
			npppd_pool_uninit(&_this->pool[i]);
	}

	signal_del(&_this->ev_sigterm);
	signal_del(&_this->ev_sigint);
	signal_del(&_this->ev_sighup);

	if (_this->properties != NULL)
		properties_destroy(_this->properties);

	slist_fini(&_this->realms);

	if (_this->map_user_ppp != NULL)
		hash_free(_this->map_user_ppp);

#ifdef NPPPD_USE_RT_ZEBRA
	rt_zebra_fini(rt_zebra_get_instance());
#endif
	rtev_fini();
}

/***********************************************************************
 * Timer related functions
 ***********************************************************************/
static void
npppd_reset_timer(npppd *_this)
{
	struct timeval tv;

	if (_this->finalizing != 0) {
		/* we can use the timer exclusively on finalizing */
		tv.tv_usec = 500000;
		tv.tv_sec = 0;
		evtimer_add(&_this->ev_timer, &tv);
	} else {
		tv.tv_usec = 0;
		tv.tv_sec = NPPPD_TIMER_TICK_IVAL;
		evtimer_add(&_this->ev_timer, &tv);
	}
}

static void
npppd_timer(int fd, short evtype, void *ctx)
{
	npppd *_this;

	_this = ctx;
	if (_this->finalizing != 0) {
		npppd_stop_really(_this); /* The timer has been reset */
		return;	/* we can use the timer exclusively on finalizing */
	}
	_this->secs += NPPPD_TIMER_TICK_IVAL;
	if (_this->reloading_count > 0) {
		_this->reloading_count -= NPPPD_TIMER_TICK_IVAL;
		if (_this->reloading_count <= 0) {
			npppd_reload0(_this);
			_this->reloading_count = 0;
		}
	} else {
		if ((_this->secs % TIMER_TICK_RUP(
			    NPPPD_AUTH_REALM_FINALIZER_INTERVAL)) == 0)
			npppd_auth_finalizer_periodic(_this);
	}

	if (_this->rtev_event_serial != rtev_get_event_serial()) {
#ifdef USE_NPPPD_PPPOE
		if (pppoed_need_polling(&_this->pppoed))
			pppoed_reload_listeners(&_this->pppoed);
#endif
	}
	_this->rtev_event_serial = rtev_get_event_serial();

#ifdef USE_NPPPD_PIPEX
	pipex_periodic(_this);
#endif	
#ifdef NPPPD_USE_RT_ZEBRA
	if (!rt_zebra_is_running(rt_zebra_get_instance())) {
		rt_zebra_start(rt_zebra_get_instance());
	}
#endif

	npppd_reset_timer(_this);
}

int
npppd_reset_routing_table(npppd *_this, int pool_only)
{
#ifndef	NO_ROUTE_FOR_POOLED_ADDRESS
	slist rtlist0;

	slist_init(&rtlist0);
	if (rd2slist(_this->rd, &rtlist0) != 0)
		return 1;

	for (slist_itr_first(&rtlist0); slist_itr_has_next(&rtlist0); ) {
		struct radish *rd;
		struct sockaddr_npppd *snp;
		npppd_ppp *ppp;
		int is_first;

		rd = slist_itr_next(&rtlist0);
		snp = rd->rd_rtent;

		is_first = 1;
		for (snp = rd->rd_rtent; snp != NULL; snp = snp->snp_next) {
			switch (snp->snp_type) {
			case SNP_POOL:
			case SNP_DYN_POOL:
				if (is_first)
					in_route_add(&snp->snp_addr,
					    &snp->snp_mask, &loop,
					    LOOPBACK_IFNAME, RTF_BLACKHOLE, 0);
				break;

			case SNP_PPP:
				if (pool_only)
					break;
				ppp = snp->snp_data_ptr;
				if (ppp->ppp_framed_ip_netmask.s_addr
				    == 0xffffffffL) {
					in_host_route_add(&ppp->
					    ppp_framed_ip_address,
					    &ppp_iface(ppp)->ip4addr,
					    ppp_iface(ppp)->ifname,
					    MRU_IPMTU(ppp->peer_mru));
				} else {
					in_route_add(&ppp->
					    ppp_framed_ip_address,
					    &ppp->ppp_framed_ip_netmask,
					    &ppp_iface(ppp)->ip4addr,
					    ppp_iface(ppp)->ifname, 0, 
					    MRU_IPMTU(ppp->peer_mru));
				}
				break;
			}
			is_first = 0;
		}

	}

	slist_fini(&rtlist0);
#endif
	return 0;
}

/***********************************************************************
 * Other npppd related functions.
 ***********************************************************************/
/**
 * Get the user's password.  Return 0 on success.
 *
 * @param	username    Username who acquires password
 * @param	password    A pointer to a buffer space to store the password.
 *			    Use NULL when you need to know only the length of
 *			    the password.
 * @param	plpassword  A pointer to the length of the password parameter.
 *			    This function uses this parameter value and stores
 *			    the required length value pointed to by this
 *			    parameter.  Use NULL when use don't need to know
 *			    the password and its length.
 * @return	If the function succeeds, 0 is returned.  The function returns
 *		1 if the username is unknown, returns 2 if the password buffer
 *		length is not enough.  It returns negative value for other
 *		errors.
 */
int
npppd_get_user_password(npppd *_this, npppd_ppp *ppp,
    const char *username, char *password, int *plpassword)
{
	char buf0[MAX_USERNAME_LENGTH];

	NPPPD_ASSERT(ppp->realm != NULL);
	return npppd_auth_get_user_password(ppp->realm,
	    npppd_auth_username_for_auth(ppp->realm, username, buf0), password,
	    plpassword);
}

/** Get the Framed-IP-Address attribute of the user */
struct in_addr *
npppd_get_user_framed_ip_address(npppd *_this, npppd_ppp *ppp,
    const char *username)
{

	if (ppp->peer_auth == 0) {
		ppp->realm_framed_ip_address.s_addr = 0;
		goto do_default;
	}
	NPPPD_ASSERT(ppp->realm != NULL);

	if (ppp->realm_framed_ip_address.s_addr != 0) {
#if 1
/*
 * FIXME: This fix is ad hok, it overwrites the ip address here if assigning
 * FIXME: IP address by RADIUS is prohibited.  This will make a bug when a
 * FIXME: new authentication type is add.  Fix this until then.
 */
		if ((ppp_ipcp(ppp)->ip_assign_flags & NPPPD_IP_ASSIGN_RADIUS)
		    == 0) {
			ppp->realm_framed_ip_netmask.s_addr = 0;
		} else
#endif
		return &ppp->realm_framed_ip_address;
	}

	ppp->realm_framed_ip_netmask.s_addr = 0xffffffffL;
	if ((ppp_ipcp(ppp)->ip_assign_flags & NPPPD_IP_ASSIGN_FIXED) != 0) {
		/* assign by the authentication realm */
		if (npppd_auth_get_framed_ip(ppp->realm, username,
		    &ppp->realm_framed_ip_address,
			    &ppp->realm_framed_ip_netmask) != 0)
			ppp->realm_framed_ip_address.s_addr = 0;
	}

do_default:
	/* Use USER_SELECT if the realm doesn't specify the ip address */
	if (ppp->realm_framed_ip_address.s_addr == 0)
		ppp->realm_framed_ip_address.s_addr = INADDR_USER_SELECT;
	if (ppp->realm_framed_ip_address.s_addr == INADDR_USER_SELECT) {
		/* Use NAS_SELECT if USER_SELECT is not allowed by the config */
		if ((ppp_ipcp(ppp)->ip_assign_flags &
		    NPPPD_IP_ASSIGN_USER_SELECT) == 0)
			ppp->realm_framed_ip_address.s_addr = INADDR_NAS_SELECT;
	}
	NPPPD_DBG((LOG_DEBUG, "%s() = %s", __func__,
	    inet_ntoa(ppp->realm_framed_ip_address)));

	return &ppp->realm_framed_ip_address;
}

/** XXX */
int
npppd_check_calling_number(npppd *_this, npppd_ppp *ppp)
{
	int lnumber, rval, strict, loose;
	char number[NPPPD_PHONE_NUMBER_LEN + 1];

	strict = ppp_config_str_equal(ppp, "check_callnum", "strict", 0);
	loose  = ppp_config_str_equal(ppp, "check_callnum", "loose", 0);

	if (strict || loose) {
		lnumber = sizeof(number);
		if ((rval = npppd_auth_get_calling_number(ppp->realm,
		    ppp->username,
		    number, &lnumber)) == 0)
			return
			    (strcmp(number, ppp->calling_number) == 0)? 1 : 0;
		if (strict)
			return 0;
	}

	return 1;
}

/**
 * This function finds a {@link npppd_ppp} instance that is assigned the
 * specified ip address and returns it 
 * @param ipaddr	IP Address(Specify in network byte order)
 */
npppd_ppp *
npppd_get_ppp_by_ip(npppd *_this, struct in_addr ipaddr)
{
	struct sockaddr_npppd *snp;
	struct radish *rdp;
	struct sockaddr_in npppd_get_ppp_by_ip_sin4;

	npppd_get_ppp_by_ip_sin4.sin_family = AF_INET;
	npppd_get_ppp_by_ip_sin4.sin_len = sizeof(struct sockaddr_in);
	npppd_get_ppp_by_ip_sin4.sin_addr = ipaddr;
	if (_this->rd == NULL)
		return NULL;	/* no radix tree on startup */
	if (rd_match((struct sockaddr *)&npppd_get_ppp_by_ip_sin4, _this->rd,
	    &rdp)) {
		snp = rdp->rd_rtent;
		if (snp->snp_type == SNP_PPP)
			return snp->snp_data_ptr;
	}
	return NULL;
}

/**
 * This function finds {@link npppd_ppp} instances that are authenticated
 * as the specified username and returns them as a {@link slist} list.
 * @param username	PPP Username.
 * @return	{@link slist} that contans the {@link npppd_ppp} instances.
 * NULL may be returned if no instance has been found.
 */
slist *
npppd_get_ppp_by_user(npppd *_this, const char *username)
{
	hash_link *hl;

	if ((hl = hash_lookup(_this->map_user_ppp, username)) != NULL)
		return hl->item;

	return NULL;
}

/**
 * This function finds a {@link npppd_ppp} instance that matches the specified
 * ppp id and returns it.
 * @param	id	{@link npppd_ppp#id ppp's id}
 * @return	This function returns the pointer if the instance which has
 *		specified ID is found, otherwise it returns NULL.
 */
npppd_ppp *
npppd_get_ppp_by_id(npppd *_this, int ppp_id)
{
	slist users;
	npppd_ppp *ppp0, *ppp;

	NPPPD_ASSERT(_this != NULL);

	ppp = NULL;
	slist_init(&users);
	if (npppd_get_all_users(_this, &users) != 0) {
		log_printf(LOG_WARNING,
		    "npppd_get_all_users() failed in %s()", __func__);
	} else {
		/* FIXME: This linear search eats CPU. */
		for (slist_itr_first(&users); slist_itr_has_next(&users); ) {
			ppp0 = slist_itr_next(&users);
			if (ppp0->id == ppp_id) {
				ppp = ppp0;
				break;
			}
		}
	}
	slist_fini(&users);

	return ppp;
}

/**
 * Checks whether the user reaches the maximum session limit
 * (user_max_serssion).
 * @return	This function returns 1(true) if the user does not reach the
 *		limit, otherwise it returns 0(false).
 */
int
npppd_check_user_max_session(npppd *_this, npppd_ppp *ppp)
{
	int count;
	npppd_ppp *ppp1;
	slist *uppp;

	/* user_max_session == 0 means unlimit */
	if (ppp_iface(ppp)->user_max_session == 0)
		return 1;

	count = 0;
	if ((uppp = npppd_get_ppp_by_user(_this, ppp->username)) != NULL) {
		for (slist_itr_first(uppp); slist_itr_has_next(uppp); ) {
			ppp1 = slist_itr_next(uppp);
			if (strcmp(ppp_iface(ppp)->ifname,
			    ppp_iface(ppp1)->ifname) == 0)
				count++;
		}
	}

	return (count < ppp_iface(ppp)->user_max_session)? 1 : 0;
}

/***********************************************************************
 * Network I/O ralated functions.
 ***********************************************************************/
/**
 * Call this function to output packets to the network(tun).  This function
 * currently assumes the packet is a IPv4 datagram.
 */
void
npppd_network_output(npppd *_this, npppd_ppp *ppp, int proto, u_char *pktp,
    int lpktp)
{
	struct ip *pip;
	int lbuf;
	u_char buf[256];	/* enough size for TCP/IP header */

	NPPPD_ASSERT(ppp != NULL);

	if (!ppp_ip_assigned(ppp))
		return;

	if (lpktp < sizeof(struct ip)) {
		ppp_log(ppp, LOG_DEBUG, "Received IP packet is too small");
		return;
	}
	lbuf = MIN(lpktp, sizeof(buf));
	if (!ALIGNED_POINTER(pktp, struct ip)) {
		memcpy(buf, pktp, lbuf);
		pip = (struct ip *)buf;
	} else {
		pip = (struct ip *)pktp;
	}

#ifndef	NO_INGRES_FILTER
	if ((pip->ip_src.s_addr & ppp->ppp_framed_ip_netmask.s_addr) != 
	    (ppp->ppp_framed_ip_address.s_addr &
		    ppp->ppp_framed_ip_netmask.s_addr)) {
		char logbuf[80];
		strlcpy(logbuf, inet_ntoa(pip->ip_dst), sizeof(logbuf));
		ppp_log(ppp, LOG_INFO,
		    "Drop packet by ingress filter.  %s => %s",
		    inet_ntoa(pip->ip_src), logbuf);

		return;
	}
#endif
	if (ppp->timeout_sec > 0 && !ip_is_idle_packet(pip, lbuf))
		ppp_reset_idle_timeout(ppp);

#ifndef NO_ADJUST_MSS
	if (ppp->adjust_mss) {
		if (lpktp == lbuf) {
			/*
			 * We can assume the packet length is less than
			 * sizeof(buf).
			 */
			if (!ALIGNED_POINTER(pktp, struct ip))
				pktp = buf;
			adjust_tcp_mss(pktp, lpktp, MRU_IPMTU(ppp->peer_mru));
		}
	}
#endif
	npppd_iface_write(ppp_iface(ppp), proto, pktp, lpktp);
}

#ifdef USE_NPPPD_PIPEX
/***********************************************************************
 * PIPEX related functions
 ***********************************************************************/
static void
pipex_setup_common(npppd_ppp *ppp, struct pipex_session_req *req)
{
	memset(req, 0, sizeof(*req));
	if (psm_opt_is_accepted(&ppp->lcp, acfc))
		req->pr_ppp_flags |= PIPEX_PPP_ACFC_ENABLED;
	if (psm_peer_opt_is_accepted(&ppp->lcp, acfc))
		req->pr_ppp_flags |= PIPEX_PPP_ACFC_ACCEPTED;

	if (psm_peer_opt_is_accepted(&ppp->lcp, pfc))
		req->pr_ppp_flags |= PIPEX_PPP_PFC_ACCEPTED;
	if (psm_opt_is_accepted(&ppp->lcp, pfc))
		req->pr_ppp_flags |= PIPEX_PPP_PFC_ENABLED;

	if (ppp->has_acf != 0)
		req->pr_ppp_flags |= PIPEX_PPP_HAS_ACF;

	if (ppp->adjust_mss != 0)
		req->pr_ppp_flags |= PIPEX_PPP_ADJUST_TCPMSS;

	req->pr_ip_address = ppp->ppp_framed_ip_address;
	req->pr_ip_netmask = ppp->ppp_framed_ip_netmask;
	req->pr_peer_mru = ppp->peer_mru;
	req->pr_ppp_id = ppp->id;

	req->pr_timeout_sec = ppp->timeout_sec;

#ifdef USE_NPPPD_MPPE
	req->pr_ccp_id = ppp->ccp.fsm.id;
	memcpy(req->pr_mppe_send.master_key,
	    ppp->mppe.send.master_key, sizeof(req->pr_mppe_send.master_key));
	req->pr_mppe_send.stateless = ppp->mppe.send.stateless;
	req->pr_mppe_send.keylenbits = ppp->mppe.send.keybits;

	memcpy(req->pr_mppe_recv.master_key,
	    ppp->mppe.recv.master_key, sizeof(req->pr_mppe_recv.master_key));
	req->pr_mppe_recv.stateless = ppp->mppe.recv.stateless;
	req->pr_mppe_recv.keylenbits = ppp->mppe.recv.keybits;

	if (ppp->mppe_started != 0) {
		req->pr_ppp_flags |= PIPEX_PPP_MPPE_ACCEPTED;
		req->pr_ppp_flags |= PIPEX_PPP_MPPE_ENABLED;
	}
	if (ppp->mppe.required)
		req->pr_ppp_flags |= PIPEX_PPP_MPPE_REQUIRED;
#endif /* USE_NPPPD_MPPE */
}

/** Enable PIPEX of the {@link npppd_ppp ppp} */
int
npppd_ppp_pipex_enable(npppd *_this, npppd_ppp *ppp)
{
	struct pipex_session_req req;
#ifdef	USE_NPPPD_PPPOE
	pppoe_session *pppoe;
#endif
#ifdef	USE_NPPPD_PPTP
	pptp_call *call;
#endif
	int error;

	NPPPD_ASSERT(ppp != NULL);
	NPPPD_ASSERT(ppp->phy_context != NULL);
	NPPPD_ASSERT(ppp->use_pipex != 0);

	pipex_setup_common(ppp, &req);

	switch (ppp->tunnel_type) {
#ifdef USE_NPPPD_PPPOE
	case PPP_TUNNEL_PPPOE:
		pppoe = (pppoe_session *)ppp->phy_context;

		/* PPPoE specific informations */
		req.pr_protocol = PIPEX_PROTO_PPPOE;
		req.pr_session_id = pppoe->session_id;
		req.pr_peer_session_id = 0;
		strlcpy(req.pr_proto.pppoe.over_ifname,
		    pppoe_session_listen_ifname(pppoe),
		    sizeof(req.pr_proto.pppoe.over_ifname));
		memcpy(&req.pr_proto.pppoe.peer_address, &pppoe->ether_addr,
		    ETHER_ADDR_LEN);

		break;
#endif
#ifdef USE_NPPPD_PPTP
	case PPP_TUNNEL_PPTP:
		call = (pptp_call *)ppp->phy_context;

		/* PPTP specific informations */
		req.pr_session_id = call->id;
		req.pr_protocol = PIPEX_PROTO_PPTP;

		req.pr_peer_session_id = call->peers_call_id;
		req.pr_proto.pptp.snd_nxt = call->snd_nxt;
		req.pr_proto.pptp.snd_una = call->snd_una;
		req.pr_proto.pptp.rcv_nxt = call->rcv_nxt;
		req.pr_proto.pptp.rcv_acked = call->rcv_acked;
		req.pr_proto.pptp.winsz = call->winsz;
		req.pr_proto.pptp.maxwinsz = call->maxwinsz;
		req.pr_proto.pptp.peer_maxwinsz = call->peers_maxwinsz;

		NPPPD_ASSERT(call->ctrl->peer.ss_family == AF_INET);
		req.pr_proto.pptp.peer_address = 
		    ((struct sockaddr_in *)&call->ctrl->peer)->sin_addr;
		NPPPD_ASSERT(call->ctrl->our.ss_family == AF_INET);
		req.pr_proto.pptp.our_address = 
		    ((struct sockaddr_in *)&call->ctrl->our)->sin_addr;
		break;
#endif
	default:
		return 1;
	}

	if ((error = ioctl(_this->iface[ppp->ifidx].devf, PIPEXASESSION, &req))
	    != 0) {
		if (errno == ENXIO)	/* pipex is disabled on runtime */
			error = 0;
		ppp->pipex_enabled = 0;
		return error;
	}

	ppp->pipex_enabled = 1;
	if (ppp->timeout_sec > 0) {
		/* Stop the npppd's idle-timer.  We use pipex's idle-timer */
		ppp->timeout_sec = 0;
		ppp_reset_idle_timeout(ppp);
	}

	return error;
}

/** Disable PIPEX of the {@link npppd_ppp ppp} */
int
npppd_ppp_pipex_disable(npppd *_this, npppd_ppp *ppp)
{
	struct pipex_session_close_req req;
#ifdef USE_NPPPD_PPPOE
	pppoe_session *pppoe;
#endif
#ifdef USE_NPPPD_PPTP
	pptp_call *call;
#endif
	int error;

	if (ppp->pipex_started == 0)
		return 0;	/* not started */

	bzero(&req, sizeof(req));
	switch(ppp->tunnel_type) {
#ifdef USE_NPPPD_PPPOE
	case PPP_TUNNEL_PPPOE:
		pppoe = (pppoe_session *)ppp->phy_context;

		/* PPPoE specific informations */
		req.pcr_protocol = PIPEX_PROTO_PPPOE;
		req.pcr_session_id = pppoe->session_id;
		break;
#endif
#ifdef USE_NPPPD_PPTP
	case PPP_TUNNEL_PPTP:
		call = (pptp_call *)ppp->phy_context;

		/* PPTP specific informations */
		req.pcr_session_id = call->id;
		req.pcr_protocol = PIPEX_PROTO_PPTP;
		break;
#endif
	default:
		return 1;
	}

	error = ioctl(_this->iface[ppp->ifidx].devf, PIPEXDSESSION, &req);
	if (error == 0) {
		ppp->ipackets += req.pcr_stat.ipackets;
		ppp->opackets += req.pcr_stat.opackets;
		ppp->ierrors += req.pcr_stat.ierrors;
		ppp->oerrors += req.pcr_stat.oerrors;
		ppp->ibytes += req.pcr_stat.ibytes;
		ppp->obytes += req.pcr_stat.obytes;
		ppp->pipex_enabled = 0;
	}

	return error;
}

/* XXX: s/npppd_ppp_pipex_ip_disable/npppd_ppp_pipex_stop/ ?? */

/** Stop PIPEX of the {@link npppd_ppp ppp} */
static int
npppd_ppp_pipex_ip_disable(npppd *_this, npppd_ppp *ppp)
{
	struct pipex_session_config_req req;
#ifdef USE_NPPPD_PPPOE
	pppoe_session *pppoe;
#endif
#ifdef USE_NPPPD_PPTP
	pptp_call *call;
#endif
	if (ppp->pipex_started == 0)
		return 0;	/* not started */

	bzero(&req, sizeof(req));
	switch(ppp->tunnel_type) {
#ifdef USE_NPPPD_PPPOE
	case PPP_TUNNEL_PPPOE:
		pppoe = (pppoe_session *)ppp->phy_context;

		/* PPPoE specific informations */
		req.pcr_protocol = PIPEX_PROTO_PPPOE;
		req.pcr_session_id = pppoe->session_id;
		break;
#endif
#ifdef USE_NPPPD_PPTP
	case PPP_TUNNEL_PPTP:
		call = (pptp_call *)ppp->phy_context;

		/* PPTP specific informations */
		req.pcr_session_id = call->id;
		req.pcr_protocol = PIPEX_PROTO_PPTP;
		break;
#endif
	default:
		return 1;
	}
	req.pcr_ip_forward = 0;

	return ioctl(_this->iface[ppp->ifidx].devf, PIPEXCSESSION, &req);
}

static void
pipex_periodic(npppd *_this)
{
	struct pipex_session_list_req req;
	npppd_ppp *ppp;
	int i, error, ppp_id;
	slist dlist, users;

	slist_init(&dlist);
	slist_init(&users);
	do {
		error = ioctl(_this->iface[0].devf, PIPEXGCLOSED, &req);
		if (error) {
			if (errno != ENXIO)
				log_printf(LOG_WARNING,
				    "PIPEXGCLOSED failed: %m");
			break;
		}
		for (i = 0; i < req.plr_ppp_id_count; i++) {
			ppp_id = req.plr_ppp_id[i];
			slist_add(&dlist, (void *)ppp_id);
		}
	} while (req.plr_flags & PIPEX_LISTREQ_MORE);

	if (slist_length(&dlist) <= 0)
		goto pipex_done;
	if (npppd_get_all_users(_this, &users) != 0) {
		log_printf(LOG_WARNING,
		    "npppd_get_all_users() failed in %s()", __func__);
		slist_fini(&users);
		goto pipex_done;
	}

	/* Disconnect request */
	slist_itr_first(&dlist);
	while (slist_itr_has_next(&dlist)) {
		/* FIXME: Linear search by PPP Id eats CPU */
		ppp_id = (int)slist_itr_next(&dlist);
		slist_itr_first(&users);
		ppp = NULL;
		while (slist_itr_has_next(&users)) {
			ppp =  slist_itr_next(&users);
			if (ppp_id == ppp->id) {
				/* found */
				slist_itr_remove(&users);
				break;
			}
			ppp = NULL;
		}
		if (ppp == NULL) {
			log_printf(LOG_WARNING,
			    "kernel requested a ppp down, but it's not found.  "
			    "ppp=%d", ppp_id);
			continue;
		}
		ppp_log(ppp, LOG_INFO, "Stop requested by the kernel");
		ppp_stop(ppp, NULL);
	}
pipex_done:
	slist_fini(&users);
	slist_fini(&dlist);
}
#endif /* USE_NPPPD_PIPEX */

/***********************************************************************
 * IP address assignment related functions
 ***********************************************************************/
/** Prepare to use IP */
int
npppd_prepare_ip(npppd *_this, npppd_ppp *ppp)
{
	if (ppp_ipcp(ppp) == NULL)
		return 1;

	npppd_get_user_framed_ip_address(_this, ppp, ppp->username);

	if (npppd_iface_ip_is_ready(ppp_iface(ppp)))
		ppp->ipcp.ip4_our = ppp_iface(ppp)->ip4addr;
	else if (npppd_iface_ip_is_ready(&_this->iface[0]))
		ppp->ipcp.ip4_our = _this->iface[0].ip4addr;
	else
		return -1;
	if (ppp_ipcp(ppp)->dns_use_tunnel_end != 0) {
		ppp->ipcp.dns_pri = ppp->ipcp.ip4_our;
		ppp->ipcp.dns_sec.s_addr = INADDR_NONE;
	} else {
		ppp->ipcp.dns_pri = ppp_ipcp(ppp)->dns_pri;
		ppp->ipcp.dns_sec = ppp_ipcp(ppp)->dns_sec;
	}
	ppp->ipcp.nbns_pri = ppp_ipcp(ppp)->nbns_pri;
	ppp->ipcp.nbns_sec = ppp_ipcp(ppp)->nbns_sec;

	return 0;
}

/** Notify stop using IP to npppd and release the resources. */
void
npppd_release_ip(npppd *_this, npppd_ppp *ppp)
{

	if (!ppp_ip_assigned(ppp))
		return;

#ifdef USE_NPPPD_LINKID
	linkid_purge(ppp->ppp_framed_ip_address);
#endif

	npppd_set_ip_enabled(_this, ppp, 0);
	npppd_pool_release_ip(ppp->assigned_pool, ppp);
	ppp->assigned_pool = NULL;
	ppp->ppp_framed_ip_address.s_addr = 0;
}

/**
 * Change IP enableness.  When the enableness is change, npppd will operate
 * the route entry.
 */
void
npppd_set_ip_enabled(npppd *_this, npppd_ppp *ppp, int enabled)
{
	int was_enabled, found;
	slist *u;
	hash_link *hl;
	npppd_ppp *ppp1;

	NPPPD_ASSERT(ppp_ip_assigned(ppp));
	NPPPD_DBG((LOG_DEBUG,
	    "npppd_set_ip_enabled(%s/%s, %s)", ppp->username,
		inet_ntoa(ppp->ppp_framed_ip_address),
		(enabled)?"true" : "false"));

	/*
	 * Don't do anything if the enableness is not change.  Changing route
	 * makes many programs will wake up and do heavy operations, it causes
	 * system overload, so we refrain useless changing route.
	 */
	enabled = (enabled)? 1 : 0;	
	was_enabled = (ppp->assigned_ip4_enabled != 0)? 1 : 0;
	if (enabled == was_enabled)
		return;

	ppp->assigned_ip4_enabled = enabled;
	if (enabled) {
		if (ppp->username[0] != '\0') {
			if ((u = npppd_get_ppp_by_user(_this, ppp->username))
			    == NULL) {
				if ((u = malloc(sizeof(slist))) == NULL) {
					ppp_log(ppp, LOG_ERR,
					    "Out of memory on %s: %m",
					    __func__);
				} else {
					slist_init(u);
					slist_set_size(u, 4);
					hash_insert(_this->map_user_ppp,
					    ppp->username, u);
					NPPPD_DBG((LOG_DEBUG,
					    "hash_insert(user->ppp, %s)",
					    ppp->username));
				}
			}
			if (u != NULL)	/* above malloc() may failed */
				slist_add(u, ppp);
		}

#ifndef	NO_ROUTE_FOR_POOLED_ADDRESS
		if (ppp->snp.snp_next != NULL)
			/*
			 * There is a blackhole route that has same
			 * address/mask.
			 */
			in_route_delete(&ppp->ppp_framed_ip_address, 
			    &ppp->ppp_framed_ip_netmask, &loop, RTF_BLACKHOLE);
		/* See the comment for MRU_IPMTU() on ppp.h */
		if (ppp->ppp_framed_ip_netmask.s_addr == 0xffffffffL) {
			in_host_route_add(&ppp->ppp_framed_ip_address,
			    &ppp_iface(ppp)->ip4addr, ppp_iface(ppp)->ifname,
			    MRU_IPMTU(ppp->peer_mru));
		} else {
			in_route_add(&ppp->ppp_framed_ip_address,
			    &ppp->ppp_framed_ip_netmask,
			    &ppp_iface(ppp)->ip4addr, ppp_iface(ppp)->ifname, 0,
			    MRU_IPMTU(ppp->peer_mru));
		}
#endif
	} else {
#ifndef	NO_ROUTE_FOR_POOLED_ADDRESS
		if (ppp->ppp_framed_ip_netmask.s_addr == 0xffffffffL) {
			in_host_route_delete(&ppp->ppp_framed_ip_address,
			    &ppp_iface(ppp)->ip4addr);
		} else {
			in_route_delete(&ppp->ppp_framed_ip_address,
			    &ppp->ppp_framed_ip_netmask,
			    &ppp_iface(ppp)->ip4addr, 0);
		}
		if (ppp->snp.snp_next != NULL)
			/*
			 * There is a blackhole route that has same
			 * address/mask.
			 */
			in_route_add(&ppp->snp.snp_addr, &ppp->snp.snp_mask,
			    &loop, LOOPBACK_IFNAME, RTF_BLACKHOLE, 0);
#endif
		if (ppp->username[0] != '\0') {
			hl = hash_lookup(_this->map_user_ppp, ppp->username);
			NPPPD_ASSERT(hl != NULL);
			if (hl == NULL) {
				ppp_log(ppp, LOG_ERR,
				    "Unexpected error: cannot find user(%s) "
				    "from user database", ppp->username);
				return;
			}
			found = 0;
			u = hl->item;
			for (slist_itr_first(u); slist_itr_has_next(u);) {
				ppp1 = slist_itr_next(u);
				if (ppp1 == ppp) {
					slist_itr_remove(u);
					found++;
					break;
				}
			}
			if (found == 0) {
				ppp_log(ppp, LOG_ERR,
				    "Unexpected error: PPP instance is "
				    "not found in the user's list.");
			}
			NPPPD_ASSERT(found != 0);
			if (slist_length(u) <= 0) {
				/* The last PPP */
				NPPPD_DBG((LOG_DEBUG,
				    "hash_delete(user->ppp, %s)",
				    ppp->username));
				if (hash_delete(_this->map_user_ppp,
				    ppp->username, 0) != 0) {
					ppp_log(ppp, LOG_ERR,
					    "Unexpected error: cannot delete "
					    "user(%s) from user database",
					    ppp->username);
				}
				slist_fini(u);
				free(u);
			} else {
				/* Replace the reference. */
				ppp1 = slist_get(u, 0);
				hl->key = ppp1->username;
			}
		}
#ifdef USE_NPPPD_PIPEX
		if (npppd_ppp_pipex_ip_disable(_this, ppp) != 0)
			ppp_log(ppp, LOG_ERR,
			    "npppd_ppp_pipex_ip_disable() failed: %m");
#endif /* USE_NPPPD_PIPEX */
	}
}

/**
 * Assign the IP address.  Returning "struct in_addr" is stored IP address
 * in network byte order.
 * @param req_ip4	IP address request to assign.  If the address is used
 * already, this function will return fail.
 */
int
npppd_assign_ip_addr(npppd *_this, npppd_ppp *ppp, uint32_t req_ip4)
{
	uint32_t ip4, ip4mask;
	int flag, dyna, rval, fallback_dyna;
	const char *reason = "out of the pool";
	struct sockaddr_npppd *snp;
	npppd_pool *pool;
	npppd_auth_base *realm;

	NPPPD_DBG((LOG_DEBUG, "%s() assigned=%s", __func__, 
	    (ppp_ip_assigned(ppp))? "true" : "false"));
	if (ppp_ip_assigned(ppp))
		return 0;

	ip4 = INADDR_ANY;
	ip4mask = 0xffffffffL;
	flag = ppp_ipcp(ppp)->ip_assign_flags;
	realm = ppp->realm;
	dyna = 0;
	fallback_dyna = 0;
	pool = NULL;

	if (ppp->realm_framed_ip_address.s_addr == INADDR_USER_SELECT) {
		if (req_ip4 == INADDR_ANY)
			dyna = 1;
	} else if (ppp->realm_framed_ip_address.s_addr == INADDR_NAS_SELECT) {
		dyna = 1;
	} else {
		NPPPD_ASSERT(realm != NULL);
		/* We cannot assign a fixed ip address without realm */

		if ((npppd_auth_get_type(realm) == NPPPD_AUTH_TYPE_RADIUS &&
		    (flag & NPPPD_IP_ASSIGN_RADIUS) == 0 &&
			    (flag & NPPPD_IP_ASSIGN_FIXED) == 0) ||
		    (npppd_auth_get_type(realm) == NPPPD_AUTH_TYPE_LOCAL &&
		    (flag & NPPPD_IP_ASSIGN_FIXED) == 0))
			dyna = 1;
		else {
			fallback_dyna = 1;
			req_ip4 = ntohl(ppp->realm_framed_ip_address.s_addr);
			ip4mask = ntohl(ppp->realm_framed_ip_netmask.s_addr);
		}
	}
	if (!dyna) {
		/*
		 * Realm requires the fixed IP address, but the address
		 * doesn't belong any address pool.  Fallback to dynamic
		 * assignment.
		 */
		for (slist_itr_first(ppp_pools(ppp));
		    slist_itr_has_next(ppp_pools(ppp));){
			pool = slist_itr_next(ppp_pools(ppp));
			rval = npppd_pool_get_assignability(pool, req_ip4,
			    ip4mask, &snp);
			switch (rval) {
			case ADDRESS_OK:
				if (snp->snp_type == SNP_POOL) {
					/*
					 * Fixed address pool can be used
					 * only if the realm specified to use
					 * it.
					 */
					if (ppp->realm_framed_ip_address
					    .s_addr != INADDR_USER_SELECT)
						ip4 = req_ip4;
					break;
				}
				ppp->assign_dynapool = 1;
				ip4 = req_ip4;
				break;
			case ADDRESS_RESERVED:
				reason = "reserved";
				continue;
			case ADDRESS_OUT_OF_POOL:
				reason = "out of the pool";
				continue;	/* try next */
			case ADDRESS_BUSY:
				fallback_dyna = 0;
				reason = "busy";
				break;
			default:
			case ADDRESS_INVALID:
				fallback_dyna = 0;
				reason = "invalid";
				break;
			}
			break;
		}
#define	IP_4OCT(v) ((0xff000000 & (v)) >> 24), ((0x00ff0000 & (v)) >> 16),\
	    ((0x0000ff00 & (v)) >> 8), (0x000000ff & (v))
		if (ip4 == 0) {
			ppp_log(ppp, LOG_NOTICE,
			    "Requested IP address (%d.%d.%d.%d)/%d "
			    "is %s", IP_4OCT(req_ip4),
			    netmask2prefixlen(htonl(ip4mask)), reason);
			if (fallback_dyna)
				goto dyna_assign;
			return 1;
		}
		ppp->assigned_pool = pool;

		ppp->ppp_framed_ip_address.s_addr = htonl(ip4);
		ppp->ppp_framed_ip_netmask.s_addr = htonl(ip4mask);
	} else {
dyna_assign:
		for (slist_itr_first(ppp_pools(ppp));
		    slist_itr_has_next(ppp_pools(ppp));){
			pool = slist_itr_next(ppp_pools(ppp));
			ip4 = npppd_pool_get_dynamic(pool, ppp);
			if (ip4 != 0)
				break;
		}
		if (ip4 == 0) {
			ppp_log(ppp, LOG_NOTICE,
			    "No free address in the pool.");
			return 1;
		}
		ppp->assigned_pool = pool;
		ppp->assign_dynapool = 1;
		ppp->ppp_framed_ip_address.s_addr = htonl(ip4);
		ppp->ppp_framed_ip_netmask.s_addr = htonl(0xffffffffL);
	}

	return npppd_pool_assign_ip(ppp->assigned_pool, ppp);
}

static void *
rtlist_remove(slist *prtlist, struct radish *radish) 
{
	struct radish *r;

	slist_itr_first(prtlist);
	while (slist_itr_has_next(prtlist)) {
		r = slist_itr_next(prtlist);
		if (!sockaddr_npppd_match(radish->rd_route, r->rd_route) ||
		    !sockaddr_npppd_match(radish->rd_mask, r->rd_mask))
			continue;

		return slist_itr_remove(prtlist);
	}

	return NULL;
}

/** Set {@link ::npppd#rd the only radish of npppd} */
int
npppd_set_radish(npppd *_this, void *radish_head)
{
	int rval, delppp0, count;
	struct sockaddr_npppd *snp;
	struct radish *radish, *r;
	slist rtlist0, rtlist1, delppp;
	npppd_ppp *ppp;
	void *dummy;

	slist_init(&rtlist0);
	slist_init(&rtlist1);
	slist_init(&delppp);

	if (radish_head != NULL) {
		if (rd2slist(radish_head, &rtlist1) != 0) {
			log_printf(LOG_WARNING, "rd2slist failed: %m");
			goto fail;
		}
	}
	if (_this->rd != NULL) {
		if (rd2slist(_this->rd, &rtlist0) != 0) {
			log_printf(LOG_WARNING, "rd2slist failed: %m");
			goto fail;
		}
	}
	if (_this->rd != NULL && radish_head != NULL) {
		for (slist_itr_first(&rtlist0); slist_itr_has_next(&rtlist0);) {
			radish = slist_itr_next(&rtlist0);
			snp = radish->rd_rtent;
		    /*
		     * replace the pool address
		     */
			if (snp->snp_type == SNP_POOL ||
			    snp->snp_type == SNP_DYN_POOL) {
				if (rd_lookup(radish->rd_route, radish->rd_mask,
					    radish_head) == NULL)
					continue;
				/* don't add */
				rtlist_remove(&rtlist1, radish);
				/* don't delete */
				slist_itr_remove(&rtlist0);
				continue;
			}
		    /*
		     * handle the active PPP sessions.
		     */
			NPPPD_ASSERT(snp->snp_type == SNP_PPP);
			ppp =  snp->snp_data_ptr;

			/* Don't delete the route of active PPP session */
			slist_itr_remove(&rtlist0);

			/* clear informations about old pool configuration */
			ppp->assigned_pool = NULL;
			snp->snp_next = NULL;

			delppp0 = 0;
			if (!rd_match((struct sockaddr *)snp, radish_head, &r)){
				/*
				 * If the address doesn't belong the new pools,
				 * add the PPP session to the deletion list.
				 */
				slist_add(&delppp, snp->snp_data_ptr);
				delppp0 = 1;
			} else {
				NPPPD_ASSERT(
				    ((struct sockaddr_npppd *)r->rd_rtent)
					->snp_type == SNP_POOL ||
				    ((struct sockaddr_npppd *)r->rd_rtent)
					->snp_type == SNP_DYN_POOL);
				/*
				 * If there is a pool entry that has same
				 * address/mask, then make the RADISH entry a
				 * list.  Set SNP_PPP as the first in the list,
				 * set current entry in snp->snp_next and
				 * delete it.
				 */
				if (sockaddr_npppd_match(
					    radish->rd_route, r->rd_route) &&
				    sockaddr_npppd_match(
					    radish->rd_mask, r->rd_mask)) {
					/*
					 * Releasing it, so remove it from the
					 * new routing list.
					 */
					rtlist_remove(&rtlist1, radish);
					/* set as snp_snp_next */
					snp->snp_next = r->rd_rtent;
					rval = rd_delete(r->rd_route,
					    r->rd_mask, radish_head, &dummy);
					NPPPD_ASSERT(rval == 0);
				}
			}
			/* Register to the new radish */
			rval = rd_insert(radish->rd_route, radish->rd_mask,
			    radish_head, snp);
			if (rval != 0) {
				errno = rval;
				ppp_log(((npppd_ppp *)snp->snp_data_ptr),
				    LOG_ERR, 
				    "Fatal error on %s, cannot continue "
				    "this ppp session: %m", __func__);
				if (!delppp0)
					slist_add(&delppp, snp->snp_data_ptr);
			}
		}
	}
	count = 0;
#ifndef	NO_ROUTE_FOR_POOLED_ADDRESS
	for (slist_itr_first(&rtlist0); slist_itr_has_next(&rtlist0);) {
		radish = slist_itr_next(&rtlist0);
		in_route_delete(&SIN(radish->rd_route)->sin_addr,
		    &SIN(radish->rd_mask)->sin_addr, &loop, RTF_BLACKHOLE);
		count++;
	}
	if (count > 0)
		log_printf(LOG_INFO,
		    "Deleted %d routes for old pool addresses", count);

	count = 0;
	for (slist_itr_first(&rtlist1); slist_itr_has_next(&rtlist1);) {
		radish = slist_itr_next(&rtlist1);
		in_route_add(&(SIN(radish->rd_route)->sin_addr),
		    &SIN(radish->rd_mask)->sin_addr, &loop, LOOPBACK_IFNAME,
		    RTF_BLACKHOLE, 0);
		count++;
	}
	if (count > 0)
		log_printf(LOG_INFO,
		    "Added %d routes for new pool addresses", count);
#endif
	slist_fini(&rtlist0);
	slist_fini(&rtlist1);

	if (_this->rd != NULL)
		npppd_rd_walktree_delete(_this->rd);
	_this->rd = radish_head;

	for (slist_itr_first(&delppp); slist_itr_has_next(&delppp);) {
		ppp = slist_itr_next(&delppp);
                ppp_log(ppp, LOG_NOTICE,
                    "stop.  IP address of this ppp is out of the pool.: %s",
                    inet_ntoa(ppp->ppp_framed_ip_address));
		ppp_stop(ppp, NULL);
	}
	slist_fini(&delppp);

	return 0;
fail:
	slist_fini(&rtlist0);
	slist_fini(&rtlist1);
	slist_fini(&delppp);

	return 1;
}

/**
 * This function stores all users to {@link slist} and returns them.
 * References to {@link ::npppd_ppp} will be stored in users.
 */
int
npppd_get_all_users(npppd *_this, slist *users)
{
	int rval;
	struct radish *rd;
	struct sockaddr_npppd *snp;
	slist list;

	NPPPD_ASSERT(_this != NULL);

	slist_init(&list);
	rval = rd2slist(_this->rd, &list);
	if (rval != 0)
		return rval;

	for (slist_itr_first(&list); slist_itr_has_next(&list);) {
		rd = slist_itr_next(&list);
		snp = rd->rd_rtent;
		if (snp->snp_type == SNP_PPP) {
			if (slist_add(users, snp->snp_data_ptr) == NULL) {
				log_printf(LOG_ERR, 
				    "slist_add() failed in %s: %m", __func__);
				goto fail;
			}
		}
	}
	slist_fini(&list);

	return 0;
fail:
	slist_fini(&list);

	return 1;
}

static int 
rd2slist_walk(struct radish *rd, void *list0)
{
	slist *list = list0;
	void *r;

	r = slist_add(list, rd);
	if (r == NULL)
		return -1;
	return 0;
}
static int
rd2slist(struct radish_head *h, slist *list)
{
	return rd_walktree(h, rd2slist_walk, list);
}

static void
npppd_reload0(npppd *_this)
{
	npppd_reload_config(_this);
#ifdef USE_NPPPD_ARP
	arp_set_strictintfnetwork(npppd_config_str_equali(_this, "arpd.strictintfnetwork", "true", ARPD_STRICTINTFNETWORK_DEFAULT));
	if (npppd_config_str_equali(_this, "arpd.enabled", "true", ARPD_DEFAULT) == 1)
        	arp_sock_init();
	else
		arp_sock_fini();
#endif
	npppd_modules_reload(_this);
	npppd_ifaces_load_config(_this);
#ifdef NPPPD_RESET_IP_ADDRESS
	{
	    int i;
	    for (i = 0; i < countof(_this->iface); i++) {
		    if (_this->iface[i].initialized != 0)
			    npppd_iface_reinit(&_this->iface[i]);
	    }
	}
#endif
	npppd_auth_finalizer_periodic(_this);
}

/***********************************************************************
 * Signal handlers
 ***********************************************************************/
static void
npppd_on_sighup(int fd, short ev_type, void *ctx)
{
	npppd *_this;

	_this = ctx;
#ifndef	NO_DELAYED_RELOAD
	if (_this->delayed_reload > 0)
		_this->reloading_count = _this->delayed_reload;
	else
#endif
		npppd_reload0(_this);
}

static void
npppd_on_sigterm(int fd, short ev_type, void *ctx)
{
	npppd *_this;

	_this = ctx;
	npppd_stop(_this);
}

static void
npppd_on_sigint(int fd, short ev_type, void *ctx)
{
	npppd *_this;

	_this = ctx;
	npppd_stop(_this);
}

/***********************************************************************
 * Miscellaneous functions
 ***********************************************************************/
static uint32_t
str_hash(const void *ptr, int sz)
{
	uint32_t hash = 0;
	int i, len;
	const char *str;

	str = ptr;
	len = strlen(str);
	for (i = 0; i < len; i++)
		hash = hash*0x1F + str[i];
	hash = (hash << 16) ^ (hash & 0xffff);

	return hash % sz;
}

/**
 * Select a authentication realm that is for given {@link ::npppd_ppp PPP}.  
 * Return 0 on success.
 */
int
npppd_ppp_bind_realm(npppd *_this, npppd_ppp *ppp, const char *username, int
    eap_required)
{
	int lsuffix, lprefix, lusername, lmax;
	const char *val;
	char *tok, *buf0, buf[NPPPD_CONFIG_BUFSIZ], buf1[MAX_USERNAME_LENGTH];
	npppd_auth_base *realm = NULL, *realm0 = NULL, *realm1 = NULL;

	NPPPD_ASSERT(_this != NULL);
	NPPPD_ASSERT(ppp != NULL);
	NPPPD_ASSERT(username != NULL);

	/*
	 * If the PPP suffix is the longest, and the length of the suffix is
	 * same, select the first one.
	 */
	lusername = strlen(username);
	lmax = -1;
	realm = NULL;

	if ((val = ppp_config_str(ppp, "realm_list")) == NULL) {
#ifndef	NO_DEFAULT_REALM
		/*
		 * If the realm is not a list, because of compatibility for
		 * past versions, we try fallback from LOCAL to RADIUS.
		 */
		realm0 = NULL;
		slist_itr_first(&_this->realms);
		while (slist_itr_has_next(&_this->realms)) {
			realm1 = slist_itr_next(&_this->realms);
			if (!npppd_auth_is_usable(realm1))
				continue;
			switch (npppd_auth_get_type(realm1)) {
			case NPPPD_AUTH_TYPE_LOCAL:
				if (npppd_auth_get_user_password(
				    realm1, npppd_auth_username_for_auth(
					    realm1, username, buf1),
				    NULL, NULL) == 0) {
					realm = realm1;
					goto found;
				}
				break;

			case NPPPD_AUTH_TYPE_RADIUS:
				realm = realm1;
				goto found;
			}
		}
#else
		/* Nothing to do */
#endif
	} else {
		strlcpy(buf, val, sizeof(buf));
		buf0 = buf;
		while ((tok = strsep(&buf0, " ,\t\r\n")) != NULL) {
			if (tok[0] == '\0')
				continue;
			realm0 = NULL;
			slist_itr_first(&_this->realms);
			while (slist_itr_has_next(&_this->realms)) {
				realm1 = slist_itr_next(&_this->realms);
				if (!npppd_auth_is_usable(realm1))
					continue;
				if (eap_required &&
				    !npppd_auth_is_eap_capable(realm1)) 
					continue;
				if (strcmp(npppd_auth_get_label(realm1), tok)
				    == 0) {
					realm0 = realm1;
					break;
				}
			}
			if (realm0 == NULL)
				continue;
			lsuffix = strlen(npppd_auth_get_suffix(realm0));
			if (lsuffix > lmax &&
			    (lsuffix == 0 || (lsuffix < lusername &&
			    strcmp(username + lusername - lsuffix,
				npppd_auth_get_suffix(realm0)) == 0))) {
				/* check prefix */
				lprefix = strlen(npppd_auth_get_suffix(realm0));
				if (lprefix > 0 &&
				    strncmp(username,
					    npppd_auth_get_suffix(realm0),
					    lprefix) != 0)
					continue;

				lmax = lsuffix;
				realm = realm0;
			}
		}
	}
	if (realm == NULL) {
		log_printf(LOG_INFO, "user='%s' could not bind any realms",
		    username);
		return 1;
	}
#ifndef	NO_DEFAULT_REALM
found:
#endif
	NPPPD_DBG((LOG_DEBUG, "%s bind realm %s(%s)",
	    username, npppd_auth_get_label(realm), npppd_auth_get_name(realm)));

	if (npppd_auth_get_type(realm) == NPPPD_AUTH_TYPE_LOCAL)
		/* hook the auto reload */
		npppd_auth_get_user_password(realm,
		    npppd_auth_username_for_auth(realm1, username, buf1), NULL,
			NULL);
	ppp->realm = realm;

	return 0;
}

/** Is assigned realm a LOCAL authentication? */
int
npppd_ppp_is_realm_local(npppd *_this, npppd_ppp *ppp)
{
	NPPPD_ASSERT(_this != NULL);
	NPPPD_ASSERT(ppp != NULL);

	if (ppp->realm == NULL)
		return 0;

	return (npppd_auth_get_type(ppp->realm) == NPPPD_AUTH_TYPE_LOCAL)
	    ? 1 : 0;
}

/** Is assigned realm a RADIUS authentication? */
int
npppd_ppp_is_realm_radius(npppd *_this, npppd_ppp *ppp)
{
	NPPPD_ASSERT(_this != NULL);
	NPPPD_ASSERT(ppp != NULL);

	if (ppp->realm == NULL)
		return 0;

	return (npppd_auth_get_type(ppp->realm) == NPPPD_AUTH_TYPE_RADIUS)
	    ? 1 : 0;
}

/** Is assigned realm usable? */
int
npppd_ppp_is_realm_ready(npppd *_this, npppd_ppp *ppp)
{
	if (ppp->realm == NULL)
		return 0;

	return npppd_auth_is_ready(ppp->realm);
}

/** Return the name of assigned realm */
const char *
npppd_ppp_get_realm_name(npppd *_this, npppd_ppp *ppp)
{
	if (ppp->realm == NULL)
		return "(none)";
	return npppd_auth_get_name(ppp->realm);
}

/** Return the interface name that bound given {@link ::npppd_ppp PPP} */
const char *
npppd_ppp_get_iface_name(npppd *_this, npppd_ppp *ppp)
{
	if (ppp == NULL || ppp->ifidx < 0)
		return "(not binding)";
	return ppp_iface(ppp)->ifname;
}

/** Is the interface usable? */
int
npppd_ppp_iface_is_ready(npppd *_this, npppd_ppp *ppp)
{
	return (npppd_iface_ip_is_ready(ppp_iface(ppp)) &&
	    ppp_ipcp(ppp) != NULL)? 1 : 0;
}

/** Select a suitable interface for {@link :npppd_ppp PPP} and bind them  */
int
npppd_ppp_bind_iface(npppd *_this, npppd_ppp *ppp)
{
	int i, ifidx, ntotal_session;
	const char *ifname, *label;
	char buf[BUFSIZ];
	npppd_auth_base *realm;

	NPPPD_ASSERT(_this != NULL);
	NPPPD_ASSERT(ppp != NULL);

	if (ppp->ifidx >= 0)
		return 0;
	if (ppp->peer_auth == 0) {
		strlcpy(buf, "no_auth.concentrate", sizeof(buf));
	} else {
		realm = (npppd_auth_base *)ppp->realm;
		strlcpy(buf, "realm.", sizeof(buf));
		NPPPD_ASSERT(ppp->realm != NULL);
		label = npppd_auth_get_label(realm);
		if (label[0] != '\0') {
			strlcat(buf, label, sizeof(buf));
			strlcat(buf, ".concentrate", sizeof(buf));
		} else
			strlcat(buf, "concentrate", sizeof(buf));
	}

	ifname = ppp_config_str(ppp, buf);
	if (ifname == NULL)
		return 1;

	/* Search a interface */
	ifidx = -1;
	ntotal_session = 0;
	for (i = 0; i < countof(_this->iface); i++) {
		if (_this->iface[i].initialized == 0)
			continue;
		ntotal_session += _this->iface[i].nsession;
		if (strcmp(_this->iface[i].ifname, ifname) == 0)
			ifidx = i;
	}
	if (ifidx < 0)
		return 1;

	if (ntotal_session >= _this->max_session) {
		ppp_log(ppp, LOG_WARNING,
		    "Number of sessions reaches out of the limit=%d",
		    _this->max_session);
		return 1;
	}
	if (_this->iface[ifidx].nsession >= _this->iface[ifidx].max_session) {
		ppp_log(ppp, LOG_WARNING,
		    "Number of sessions reaches out of the interface limit=%d",
		    _this->iface[ifidx].max_session);
		return 1;
	}

	ppp->ifidx = ifidx;
	ppp_iface(ppp)->nsession++;

	return 0;
}

/** Unbind the interface from the {@link ::npppd_ppp PPP} */
void
npppd_ppp_unbind_iface(npppd *_this, npppd_ppp *ppp)
{
	if (ppp->ifidx >= 0)
		ppp_iface(ppp)->nsession--;

	ppp->ifidx = -1;
}

static int
npppd_rd_walktree_delete(struct radish_head *rh)
{
	void *dummy;
	struct radish *rd;
	slist list;

	slist_init(&list);
	if (rd2slist(rh, &list) != 0)
		return 1;
	for (slist_itr_first(&list); slist_itr_has_next(&list);) {
		rd = slist_itr_next(&list);
		rd_delete(rd->rd_route, rd->rd_mask, rh, &dummy);
	}
	slist_fini(&list);

	free(rh);

	return 0;
}

#ifdef USE_NPPPD_RADIUS
/**
 * Return radius_req_setting for the given {@link ::npppd_ppp PPP}.
 * @return return NULL if no usable RADIUS setting.
 */
void *
npppd_get_radius_req_setting(npppd *_this, npppd_ppp *ppp)
{
	NPPPD_ASSERT(_this != NULL);
	NPPPD_ASSERT(ppp != NULL);

	if (ppp->realm == NULL)
		return NULL;
	if (!npppd_ppp_is_realm_radius(_this, ppp))
		return NULL;

	return npppd_auth_radius_get_radius_req_setting(
	    (npppd_auth_radius *)ppp->realm);
}

/** Notice a failure on RAIDUS request/response */
void
npppd_radius_server_failure_notify(npppd *_this, npppd_ppp *ppp, void *rad_ctx,
    const char *reason)
{
	NPPPD_ASSERT(rad_ctx != NULL);
	NPPPD_ASSERT(ppp != NULL);

	npppd_auth_radius_server_failure_notify(
	    (npppd_auth_radius *)ppp->realm, radius_get_server_address(rad_ctx),
	    reason);
}
#endif

/** Finalize authentication realm */
static void
npppd_auth_finalizer_periodic(npppd *_this)
{
	int ndisposing = 0, refcnt;
	slist users;
	npppd_auth_base *auth_base;
	npppd_ppp *ppp;

	/*
	 * For the realms with disposing flag, if the realm has assigned PPPs,
	 * disconnect them.  If all PPPs are disconnected then free the realm. 
	 */
	NPPPD_DBG((DEBUG_LEVEL_2, "%s() called", __func__));
	slist_itr_first(&_this->realms);
	while (slist_itr_has_next(&_this->realms)) {
		auth_base = slist_itr_next(&_this->realms);
		if (!npppd_auth_is_disposing(auth_base))
			continue;
		refcnt = 0;
		if (ndisposing++ == 0) {
			slist_init(&users);
			if (npppd_get_all_users(_this, &users) != 0) {
				log_printf(LOG_WARNING,
				    "npppd_get_all_users() failed in %s(): %m",
				    __func__);
				break;
			}
		}
		slist_itr_first(&users);
		while (slist_itr_has_next(&users)) {
			ppp = slist_itr_next(&users);
			if (ppp->realm == auth_base) {
				refcnt++;
				ppp_stop(ppp, NULL);
				ppp_log(ppp, LOG_INFO,
				    "Stop request by npppd.  Binding "
				    "authentication realm is disposing.  "
				    "realm=%s", npppd_auth_get_name(auth_base));
				slist_itr_remove(&users);
			}
		}
		if (refcnt == 0)
			npppd_auth_destroy(auth_base);
	}
	if (ndisposing > 0)
		slist_fini(&users);
}

/** compare sockaddr_npppd.  return 0 if matches */
int
sockaddr_npppd_match(void *a0, void *b0)
{
	struct sockaddr_npppd *a, *b;

	a = a0;
	b = b0;

	return (a->snp_addr.s_addr == b->snp_addr.s_addr)? 1 : 0;
}

/**
 * This function stores the username for authentication to the space specified
 * by username_buffer and returns it.  username_buffer must have space more
 * than MAX_USERNAME_LENGTH.
 */
const char *
npppd_ppp_get_username_for_auth(npppd *_this, npppd_ppp *ppp,
    const char *username, char *username_buffer)
{
	NPPPD_ASSERT(_this != NULL);
	NPPPD_ASSERT(ppp != NULL);
	NPPPD_ASSERT(ppp->realm != NULL);

	return npppd_auth_username_for_auth(ppp->realm, username,
	    username_buffer);
}

static inline void
seed_random(long *seed)
{
	struct timeval t;
#ifdef KERN_URND
	size_t seedsiz;
	int mib[] = { CTL_KERN, KERN_URND };

	seedsiz = sizeof(*seed);
	if (sysctl(mib, countof(mib), seed, &seedsiz, NULL, 0) == 0) {
		NPPPD_ASSERT(seedsiz == sizeof(long));
		return;
	}
	log_printf(LOG_WARNING, "Could not set random seed from the system: %m");
#endif
	gettimeofday(&t, NULL);
	*seed = gethostid() ^ t.tv_sec ^ t.tv_usec ^ getpid();
}
