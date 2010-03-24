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
#ifndef NPPPD_INTERFACE_H
#define NPPPD_INTERFACE_H 1

typedef struct _npppd_iface {
 	/** �١����Ȥʤ� npppd */
	void	*npppd;
 	/** ���󥿥ե�����̾ */
	char	ifname[IFNAMSIZ];
 	/** �ǥХ����ե�����Υǥ�����ץ� */
	int	devf;

 	/** ������Ƥ�줿 IPv4 ���ɥ쥹 */
	struct in_addr	ip4addr;
 	/** event(3) �ѥ��С� */
	struct event	ev;

	/** Ʊ��PPP�桼������³�Ǥ������� PPP���å����� */
	int		user_max_session;
	/** ��³�Ǥ������� PPP���å����� */
	int		max_session;

	/** ��³��� PPP���å����� */
	int		nsession;

 	int	/**
 		 * npppd_iface �ν����Ȥ���IP���ɥ쥹�򥻥åȤ��뤫��
 		 * <p>0 �Ǥ���С�npppd_iface �ϡ����åȤ��줿 IP���ɥ쥹��
 		 * ���Ȥ�������Ǥ���</p>
 		 */
 		set_ip4addr:1,
 		/** ������Ѥߥե饰 */
  		initialized:1;
} npppd_iface;

/** ���󥿥ե�������IP���ɥ쥹�ϻ��ѤǤ��뤫 */
#define npppd_iface_ip_is_ready(int) \
    ((int)->initialized != 0 && (int)->ip4addr.s_addr != INADDR_ANY)

#ifdef __cplusplus
extern "C" {
#endif

void  npppd_iface_init (npppd_iface *, const char *);
int   npppd_iface_reinit (npppd_iface *);
int   npppd_iface_start (npppd_iface *);
void  npppd_iface_stop (npppd_iface *);
void  npppd_iface_fini (npppd_iface *);
void  npppd_iface_write (npppd_iface *, int proto, u_char *, int);

#ifdef __cplusplus
}
#endif
#endif
