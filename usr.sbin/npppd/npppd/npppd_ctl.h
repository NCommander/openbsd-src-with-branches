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
#ifndef NPPPD_CTL_H
#define NPPPD_CTL_H 1

#define	NPPPD_CTL_MAX_MSGSIZ		8192

#ifndef	DEFAULT_NPPPD_CTL_SOCK_PATH
#define	DEFAULT_NPPPD_CTL_SOCK_PATH	"/var/run/npppd_ctl"
#endif

/** ���ѥ桼�������׾��� */
#define	NPPPD_CTL_CMD_WHO 			1

#ifndef DEFAULT_NPPPD_CTL_MAX_MSGSZ	
#define	DEFAULT_NPPPD_CTL_MAX_MSGSZ	51200
#endif

struct npppd_who {
	/** ppp Id */
	int id;
	/** �桼��̾ */
	char name[MAX_USERNAME_LENGTH];
	/** ���ϻ��� */
	time_t time;
	/** �в���� */
	uint32_t duration_sec;
	/** ʪ���إ�٥� */
	char phy_label[16];
	/** ���󥤥󥿡��ե����� **/
	char ifname[IF_NAMESIZE];

	char rlmname[NPPPD_GENERIC_NAME_LEN];
	union {
		struct sockaddr_in peer_in;
		struct sockaddr_dl peer_dl;
	} /** ʪ���ؤΥ��ɥ쥹���� */
	phy_info;

	/** ������Ƥ� IP ���ɥ쥹 */
	struct in_addr assign_ip4;
	/** ���ϥѥ��åȿ� */
	uint32_t	ipackets;
	/** ���ϥѥ��åȿ� */
	uint32_t	opackets;
	/** ���ϥ��顼�ѥ��åȿ� */
	uint32_t	ierrors;
	/** ���ϥ��顼�ѥ��åȿ� */
	uint32_t	oerrors;
	/** ���ϥѥ��åȥХ���*/
	uint64_t	ibytes;
	/** ���ϥѥ��åȥХ���*/
	uint64_t	obytes;
};
struct npppd_who_list {
	int			count;
	struct npppd_who	entry[0];
};

/** ���ꤷ���桼������³������ */
#define	NPPPD_CTL_CMD_DISCONNECT_USER		2

struct npppd_disconnect_user_req {
	int command;
	char username[MAX_USERNAME_LENGTH];
};

/** ü��ǧ�ڤ�ǧ�ھ���򥻥åȤ��ޤ� */
#define NPPPD_CTL_CMD_TERMID_SET_AUTH		3

/** npppd �λ��ķ�ϩ����򥷥��ƥ�˥ꥻ�åȤ��ޤ� */
#define NPPPD_CTL_CMD_RESET_ROUTING_TABLE	4

typedef	enum _npppd_ctl_ppp_key {
	NPPPD_CTL_PPP_ID = 0,
	NPPPD_CTL_PPP_FRAMED_IP_ADDRESS,
} npppd_ctl_ppp_key_type;

struct npppd_ctl_termid_set_auth_request {
	int command;
	int reserved;
	npppd_ctl_ppp_key_type	ppp_key_type;
	union {
		uint32_t id;
		struct in_addr framed_ip_address;
	} ppp_key;
	char authid[33];
};

#endif
