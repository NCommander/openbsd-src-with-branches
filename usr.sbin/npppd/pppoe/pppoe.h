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
#ifndef	PPPOE_H
#define	PPPOE_H 1

/*
 * �ץ�ȥ�������� (RFC 2516)
 */
#define PPPOE_RFC2516_TYPE	0x01
#define PPPOE_RFC2516_VER	0x01

/** The PPPoE Active Discovery Initiation (PADI) packet */
#define	PPPOE_CODE_PADI		0x09

/** The PPPoE Active Discovery Offer (PADO) packet */
#define	PPPOE_CODE_PADO		0x07

/** The PPPoE Active Discovery Request (PADR) packet */
#define	PPPOE_CODE_PADR		0x19

/** The PPPoE Active Discovery Session-confirmation (PADS) packet */
#define	PPPOE_CODE_PADS		0x65

/** The PPPoE Active Discovery Terminate (PADT) packet */
#define	PPPOE_CODE_PADT		0xa7

#define	PPPOE_TAG_END_OF_LIST		0x0000
#define	PPPOE_TAG_SERVICE_NAME		0x0101
#define	PPPOE_TAG_AC_NAME		0x0102
#define	PPPOE_TAG_HOST_UNIQ		0x0103
#define	PPPOE_TAG_AC_COOKIE		0x0104
#define	PPPOE_TAG_VENDOR_SPECIFIC	0x0105
#define	PPPOE_TAG_RELAY_SESSION_ID	0x0110
#define	PPPOE_TAG_SERVICE_NAME_ERROR	0x0201
#define	PPPOE_TAG_AC_SYSTEM_ERROR	0x0202
#define	PPPOE_TAG_GENERIC_ERROR		0x0203

/** PPPoE �إå� */
struct pppoe_header {
#if _BYTE_ORDER == _BIG_ENDIAN
    uint8_t ver:4, type:4;
#else
    uint8_t type:4, ver:4;
#endif
    uint8_t code;
    uint16_t session_id;
    uint16_t length;
} __attribute__((__packed__));

/** PPPoE TLV �إå� */
struct pppoe_tlv {
	uint16_t	type;
	uint16_t	length;
	uint8_t		value[0];
} __attribute__((__packed__));

/*
 * ����
 */
/** �ǥե���Ȥ�ʪ���إ�٥� */
#define PPPOED_DEFAULT_LAYER2_LABEL	"PPPoE"

#define	PPPOED_CONFIG_BUFSIZ		65535
#define	PPPOED_HOSTUNIQ_LEN		64
#define PPPOED_PHY_LABEL_SIZE		16

/*
 * pppoed ���ơ�����
 */
/** ������� */
#define	PPPOED_STATE_INIT 		0
/** ������ */
#define	PPPOED_STATE_RUNNING 		1
/** ���j���� */
#define	PPPOED_STATE_STOPPED 		2

#define pppoed_is_stopped(pppoed)	\
	(((pppoed)->state == PPPOED_STATE_STOPPED)? 1 : 0)
#define pppoed_is_running(pppoed)	\
	(((pppoed)->state == PPPOED_STATE_RUNNING)? 1 : 0)

#define	PPPOED_LISTENER_INVALID_INDEX	UINT16_MAX

/** PPPoE �ǡ�����Ԥ������� */
typedef struct _pppoed_listener {
	/** bpf(4) �ǥХ����ե�����Υǥ�������ץ� */
	int bpf;
	/** bpf �ѤΥ��٥�ȥ���ƥ����� */
	struct event ev_bpf;
	/** PPPoE �ǡ���󼫿� */
	struct _pppoed *self;
	/** �������ͥåȥ��ɥ쥹 */
	u_char	ether_addr[ETHER_ADDR_LEN];
	/** ����ǥå����ֹ� */
	uint16_t	index;
	/** �Ԥ������륤�󥿥ե�����̾ */
	char	listen_ifname[IF_NAMESIZE];
	/** ʪ���ؤΥ�٥� */
	char	phy_label[PPPOED_PHY_LABEL_SIZE];
} pppoed_listener;

/** PPPoE �ǡ����򼨤����Ǥ���*/
typedef struct _pppoed {
	/** PPPoE �ǡ����� Id */
	int id;
	/** �Ԥ������ꥹ�� */
	slist listener;
	/** ���Υǡ����ξ��� */
	int state;
	/** �����ॢ���ȥ��٥�� **/

	/** ���å�����ֹ� => pppoe_session �Υϥå���ޥå� */
	hash_table	*session_hash;
	/** �������å�����ֹ�ꥹ�� */
	slist	session_free_list;

	/** ���å���������å����Υϥå���ޥå� */
	hash_table	*acookie_hash;
	/** ���Υ��å����ֹ� */
	uint32_t	acookie_next;

	/** ����ץ�ѥƥ� */
	struct properties *config;

	/** �ե饰 */
	uint32_t
	    desc_in_pktdump:1,
	    desc_out_pktdump:1,
	    session_in_pktdump:1,
	    session_out_pktdump:1,
	    listen_incomplete:1,
	    /* phy_label_with_ifname:1,	PPPoE ������ */
	    reserved:27;
} pppoed;

/** PPPoE ���å����򼨤����Ǥ� */
typedef struct _pppoe_session {
	int 		state;
	/** �� PPPoE �ǡ���� */
	pppoed		*pppoed;
	/** PPP ����ƥ����� */
	void 		*ppp;
	/** ���å���� Id */
	int		session_id;
	/** ���å����ֹ� */
	int		acookie;
	/** �й��Υ������ͥåȥ��ɥ쥹 */
	u_char 		ether_addr[ETHER_ADDR_LEN];
	/** �ꥹ�ʥ���ǥå��� */
	uint16_t	listener_index;
	/** �������إå�����å��� */
	struct ether_header ehdr;

	/** echo �������ֳ�(��) */
	int lcp_echo_interval;
	/** echo �κ���Ϣ³���Բ�� */
	int lcp_echo_max_failure;

	/** ��λ�����Τ���� event ����ƥ����� */
	struct event ev_disposing;
} pppoe_session;

/** pppoe_session �ξ��֤�������֤Ǥ��뤳�Ȥ򼨤�����Ǥ� */
#define	PPPOE_SESSION_STATE_INIT		0

/** pppoe_session �ξ��֤����Ծ��֤Ǥ��뤳�Ȥ򼨤�����Ǥ� */
#define	PPPOE_SESSION_STATE_RUNNING		1

/** pppoe_session �ξ��֤���λ��Ǥ��뤳�Ȥ򼨤�����Ǥ� */
#define	PPPOE_SESSION_STATE_DISPOSING		2

#define	pppoed_need_polling(pppoed)	(((pppoed)->listen_incomplete != 0)? 1 : 0)

#ifdef __cplusplus
extern "C" {
#endif

int         pppoe_session_init (pppoe_session *, pppoed *, int, int, u_char *);
void        pppoe_session_fini (pppoe_session *);
void        pppoe_session_stop (pppoe_session *);
int         pppoe_session_recv_PADR (pppoe_session *, slist *);
int         pppoe_session_recv_PADT (pppoe_session *, slist *);
void        pppoe_session_input (pppoe_session *, u_char *, int);
void        pppoe_session_disconnect (pppoe_session *);


int         pppoed_add_listener (pppoed *, int, const char *, const char *);
int         pppoed_reload_listeners(pppoed *);

int   pppoed_init (pppoed *);
int   pppoed_start (pppoed *);
void  pppoed_stop (pppoed *);
void  pppoed_uninit (pppoed *);
void  pppoed_pppoe_session_close_notify(pppoed *, pppoe_session *);
const char * pppoed_tlv_value_string(struct pppoe_tlv *);
int pppoed_reload(pppoed *, struct properties *, const char *, int);
const char   *pppoed_config_str (pppoed *, const char *);
int          pppoed_config_int (pppoed *, const char *, int);
int          pppoed_config_str_equal (pppoed *, const char *, const char *, int);
int          pppoed_config_str_equali (pppoed *, const char *, const char *, int);

const char   *pppoed_listener_config_str (pppoed_listener *, const char *);
int          pppoed_listener_config_int (pppoed_listener *, const char *, int);
int          pppoed_listener_config_str_equal (pppoed_listener *, const char *, const char *, int);
int          pppoed_listener_config_str_equali (pppoed_listener *, const char *, const char *, int);

#ifdef __cplusplus
}
#endif
#endif
