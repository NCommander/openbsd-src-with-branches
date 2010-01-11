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

struct _npppd_auth_base {
	/** ��٥�̾ */
	char	label[NPPPD_GENERIC_NAME_LEN];
	/** ����̾ */
	char	name[NPPPD_GENERIC_NAME_LEN];
	/** �� npppd �λ��� */
	void 	*npppd;
	/** ǧ�ڥ���Υ����� */
	int	type;
	/** PPP ���ե��å��� */
	char	pppsuffix[64];
	/** PPP �ץ�ե��å��� */
	char	pppprefix[64];
	uint32_t
		/** ������Ѥ� */
		initialized:1,
		/** ���ɤ߹��߲�ǽ */
		reloadable:1,
		/** �Ѵ��� */
		disposing:1,
		/** Is the account list ready */
		acctlist_ready:1,
		/** Is the radius configuration ready */
		radius_ready:1,
		/** EAP �����ѤǤ��뤫�ɤ��� */
		eap_capable:1,
		/** Windows-NT �ɥᥤ�����Ū�� strip ���뤫�ɤ��� */
		strip_nt_domain:1,
		/** PPP�桼��̾�� @ �ʹߤ���Ū�� strip ���뤫�ɤ�����*/
		strip_atmark_realm:1,
		/** has account-list */
		has_acctlist:1,
		reserved:24;

	/** �桼��̾ => npppd_auth_user hash */
	hash_table *users_hash;
	/** ��������ȥꥹ�ȤΥѥ�̾ */
	char	acctlist_path[64];
	/** �ǽ����ɻ��� */
	time_t	last_load;
};

#ifdef USE_NPPPD_RADIUS
struct _npppd_auth_radius {
	/** �� npppd_auth_base */
	npppd_auth_base nar_base;

	/** ���ߤ�������Υ����� */
	int curr_server;

	/** RADIUS������ */
	radius_req_setting rad_setting;

};
#endif

/** ������ǧ�ڥ���η� */
struct _npppd_auth_local {
	/* �� npppd_auth_base */
	npppd_auth_base nal_base;
};

/** �桼���Υ�������Ⱦ���򼨤��� */
typedef struct _npppd_auth_user {
	/** �桼��̾ */
	char *username;
	/** �ѥ���� */
	char *password;
	/** Framed-IP-Address */
	struct in_addr	framed_ip_address;
	/** Framed-IP-Netmask */
	struct in_addr	framed_ip_netmask;
	/** Calling-Number */
	char *calling_number;
	/** ���ڡ��������ѥե������ */
	char space[0];
} npppd_auth_user;

static int                npppd_auth_reload_acctlist (npppd_auth_base *);
static npppd_auth_user    *npppd_auth_find_user (npppd_auth_base *, const char *);
static int                radius_server_address_load (radius_req_setting *, int, const char *);
static int                npppd_auth_radius_reload (npppd_auth_base *);
static int                npppd_auth_base_log (npppd_auth_base *, int, const char *, ...);
static uint32_t           str_hash (const void *, int);
static const char *       npppd_auth_default_label(npppd_auth_base *);
static inline const char  *npppd_auth_config_prefix (npppd_auth_base *);
static const char         *npppd_auth_config_str (npppd_auth_base *, const char *);
static int                npppd_auth_config_int (npppd_auth_base *, const char *, int);
static int                npppd_auth_config_str_equal (npppd_auth_base *, const char *, const char *, int);

#ifdef NPPPD_AUTH_DEBUG
#define NPPPD_AUTH_DBG(x) 	npppd_auth_base_log x
#define NPPPD_AUTH_ASSERT(x)	ASSERT(x)
#else
#define NPPPD_AUTH_DBG(x) 
#define NPPPD_AUTH_ASSERT(x)
#endif
