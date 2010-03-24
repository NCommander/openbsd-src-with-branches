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
 * CCP - Compression Control Protocol
 * <p>
 * �б����륪�ץ����
 * <pre>
 *	- MPPE</pre></p>
 * $Id: ccp.c 35137 2008-05-19 14:06:23Z yasuoka $
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <event.h>

#include "slist.h"
#include "npppd.h"
#include "fsm.h"

#ifdef	CCP_DEBUG
#define	CCPDEBUG(x)	fsm_log(x)
#define	CCP_ASSERT(x)	ASSERT(x)
#else
#define	CCPDEBUG(x)	
#define	CCP_ASSERT(x)
#endif

static int   ccp_reqci (fsm *, u_char *, int *, int);
static void  ccp_open (fsm *);
static void  ccp_close (fsm *);
static void  ccp_start (fsm *);
static void  ccp_stop (fsm *);
static void  ccp_resetci (fsm *);
static int   ccp_cilen (fsm *);
static void  ccp_addci (fsm *, u_char *, int *);
static int   ccp_ackci (fsm *, u_char *, int);
static int   ccp_rejci (fsm *, u_char *, int);
static int   ccp_nakci (fsm *, u_char *, int);
static int   ccp_nackackci (fsm *, u_char *, int, int, int);
static int   ccp_ext (fsm *, int, int, u_char *, int);

static struct fsm_callbacks ccp_callbacks = {
	.cilen		= ccp_cilen,
	.resetci	= ccp_resetci,
	.addci		= ccp_addci,
	.ackci		= ccp_ackci,
	.nakci		= ccp_nakci,
	.rejci		= ccp_rejci,
	.reqci		= ccp_reqci,
	.up		= ccp_open,
	.down		= ccp_close,
	.starting	= ccp_start,
	.finished	= ccp_stop,
	.extcode	= ccp_ext,
	.proto_name	= "ccp",
};

/**
 * ccp ����ƥ����Ȥ��������ޤ���
 */
void
ccp_init(ccp *_this, npppd_ppp *ppp)
{
	memset(_this, 0, sizeof(ccp));

	_this->ppp = ppp;
	_this->fsm.callbacks = &ccp_callbacks;
	_this->fsm.protocol = PPP_PROTO_NCP | NCP_CCP;
	_this->fsm.ppp = ppp;

	fsm_init(&_this->fsm);
	//_this->fsm.flags |= OPT_SILENT;

	PPP_FSM_CONFIG(&_this->fsm, timeouttime,	"ccp.timeout");
	PPP_FSM_CONFIG(&_this->fsm, maxconfreqtransmits,"ccp.max_configure");
	PPP_FSM_CONFIG(&_this->fsm, maxtermtransmits,	"ccp.max_terminate");
	PPP_FSM_CONFIG(&_this->fsm, maxnakloops,	"ccp.max_nak_loop");
}

/**
 * Request Command Interpreter��
 */
static int
ccp_reqci(fsm *f, u_char *pktp, int *lpktp, int reject_if_disagree)
{
	int type, len, rcode, lrej, lnak;
	u_char *rejbuf, *nakbuf, *nakbuf0, *pktp0;
#ifdef USE_NPPPD_MPPE
	uint32_t peer_bits, our_bits;
#endif
	npppd_ppp *ppp;

	ppp = f->ppp;

	rejbuf = NULL;
	rcode = CONFACK;
	pktp0 = pktp;
	lrej = 0;
	lnak = 0;

	if ((rejbuf = malloc(*lpktp)) == NULL) {
		return rcode;
	}
	if ((nakbuf0 = malloc(*lpktp)) == NULL) {
		free(rejbuf);
		return rcode;
	}
	nakbuf = nakbuf0;
#define	remlen()	(*lpktp - (pktp - pktp0))

	while (remlen() >= 2) {
		GETCHAR(type, pktp);
		GETCHAR(len, pktp);
		if (len <= 0 || remlen() + 2 < len)
			goto reigai;

		switch (type) {
#ifdef USE_NPPPD_MPPE
		case CCP_MPPE:
			if (len < 6)
				goto reigai;

			if (ppp->mppe.enabled == 0)
				goto reject;
			GETLONG(peer_bits, pktp);
			our_bits = mppe_create_our_bits(&ppp->mppe, peer_bits);
			if (our_bits != peer_bits) {
				if (reject_if_disagree) {
					pktp -= 4;
					goto reject;
				}
				if (lrej > 0) {
				/* reject ������С�Rej ����Τ� Nak ���ʤ� */
				} else {
					PUTCHAR(type, nakbuf);
					PUTCHAR(6, nakbuf);
					PUTLONG(our_bits, nakbuf);
					rcode = CONFNAK;
				}
			} else
				ppp->ccp.mppe_p_bits = our_bits;
			break;
reject:
#endif
		default:
			pktp -= 2;
			memcpy(rejbuf + lrej, pktp, len);
			lrej += len;
			pktp += len;
			rcode = CONFREJ;
		}
		continue;
	}
reigai:
	switch (rcode) {
	case CONFREJ:
		memcpy(pktp0, rejbuf, lrej);
		*lpktp = lrej;
		break;
	case CONFNAK:
		len = nakbuf - nakbuf0;
		memcpy(pktp0, nakbuf0, len);
		*lpktp = len;
		break;
	}
	if (rejbuf != NULL)
		free(rejbuf);
	if (nakbuf0 != NULL)
		free(nakbuf0);

	return rcode;
#undef	remlen
}

static void
ccp_open(fsm *f)
{
	ppp_ccp_opened(f->ppp);
}

static void
ccp_close(fsm *f)
{
}

static void
ccp_start(fsm *f)
{
}

static void
ccp_stop(fsm *f)
{
#ifdef USE_NPPPD_MPPE
	fsm_log(f, LOG_INFO, "CCP is stopped");
	if (f->ppp->mppe.required)
		ppp_stop(f->ppp, NULL);
#endif
}

static void
ccp_resetci(fsm *f)
{
#ifdef	USE_NPPPD_MPPE
	if (f->ppp->mppe_started == 0)
		f->ppp->ccp.mppe_o_bits =
		    mppe_create_our_bits(&f->ppp->mppe, 0);
	/* ���Ϥ��Ƥ�����ꥻ�åȤϤ��ʤ� */
#endif
}

static int
ccp_cilen(fsm *f)
{
	return f->ppp->mru;
}

/**
 * ConfReq ����ޤ���
 */
static void
ccp_addci(fsm *f, u_char *pktp, int *lpktp)
{
	u_char *pktp0;

	pktp0 = pktp;

	if (f->ppp->ccp.mppe_rej == 0) {
		PUTCHAR(CCP_MPPE, pktp);
		PUTCHAR(6, pktp);
		PUTLONG(f->ppp->ccp.mppe_o_bits, pktp);

		*lpktp = pktp - pktp0;
	} else 
		*lpktp = 0;
}

static int
ccp_ackci(fsm *f, u_char *pktp, int lpkt)
{
	return ccp_nackackci(f, pktp, lpkt, 0, 0);
}


static int
ccp_nakci(fsm *f, u_char *pktp, int lpkt)
{
	return ccp_nackackci(f, pktp, lpkt, 1, 0);
}

static int
ccp_rejci(fsm *f, u_char *pktp, int lpkt)
{
	return ccp_nackackci(f, pktp, lpkt, 0, 1);
}

static int
ccp_nackackci(fsm *f, u_char *pktp, int lpkt, int is_nak, int is_rej)
{
	int type, len;
	u_char *pktp0;
#ifdef	USE_NPPPD_MPPE
	uint32_t peer_bits, our_bits;
#endif
	npppd_ppp *ppp;

	ppp = f->ppp;

	pktp0 = pktp;

#define	remlen()	(lpkt - (pktp - pktp0))
	while (remlen() >= 2) {
		GETCHAR(type, pktp);
		GETCHAR(len, pktp);
		if (len <= 0 || remlen() + 2 < len)
			goto reigai;

		switch (type) {
#ifdef USE_NPPPD_MPPE
		case CCP_MPPE:
			if (len < 6)
				goto reigai;
			if (is_rej) {
				f->ppp->ccp.mppe_rej = 1;
				return 1;
			}
			if (ppp->mppe_started != 0) {
				// �Ť��˺������롣
				return 1;
			}
			GETLONG(peer_bits, pktp);
			/*
			 * RTX-1000 �� ppp ccp mppe-any ����ȡ�
			 *
			 *	IDGW ConfReq (40,56,128) => RTX
			 *	IDGW <= (40,128) ConfNAK    RTX
			 *	IDGW ConfReq (40,56,128) => RTX
			 *	IDGW <= (40,128) ConfNAK    RTX
			 *		:
			 * �Ȥ��ߤ����ꤹ����ConfNak ���줿�顢�����餬���
			 * ���롣
			 */
			our_bits = mppe_create_our_bits(&ppp->mppe, peer_bits);
			if (peer_bits == our_bits || is_nak) 
				ppp->ccp.mppe_o_bits = our_bits;

			break;
#endif
		default:
			goto reigai;
		}
	}
	return 1;
reigai:
	return 0;
}

#define	RESET_REQ	0x0e
#define	RESET_ACK	0x0f

static int
ccp_ext(fsm *f, int code, int id, u_char *pktp, int lpktp)
{
	switch (code) {
	case RESET_REQ:
		fsm_log(f, LOG_DEBUG, "Received ResetReq %d", id);
#ifdef USE_NPPPD_MPPE
		mppe_recv_ccp_reset(&f->ppp->mppe);
#endif
		/*
		 * RFC 3078 �Ǥϡ�Reset Ack ���פȤϽ񤤤Ƥ��ʤ�������������
		 * �� Windows �� Code Reject ���֤��Τǡ��������ʤ���
		 */
		return 1;
	case RESET_ACK:
		fsm_log(f, LOG_DEBUG, "Received ResetAck %d", id);
		return 1;
	}
	return 0;
}
