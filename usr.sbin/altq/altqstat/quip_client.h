/*	$OpenBSD$	*/
/*	$KAME: quip_client.h,v 1.2 2000/10/18 09:15:17 kjc Exp $	*/
/*
 * Copyright (C) 1999-2000
 *	Sony Computer Science Laboratories, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY SONY CSL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL SONY CSL OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _QUIP_CLIENT_H_
#define _QUIP_CLIENT_H_

/* unix domain socket for quip */
#define QUIP_PATH	"/var/run/altq_quip"

extern int quip_echo;

int quip_openserver(void);
int quip_closeserver(void);
void quip_sendrequest(FILE *fp, const char *request);
int quip_recvresponse(FILE *fp, char *header, char *body, int *blen);
void quip_rawmode(void);
char *quip_selectinterface(char *ifname);
char *quip_selectqdisc(char *ifname, char *qdisc_name);
void quip_chandle2name(const char *ifname, u_long handle, char *name);
void quip_printqdisc(const char *ifname);
void quip_printfilter(const char *ifname, const u_long handle);
void quip_printconfig(void);

#endif /* _QUIP_CLIENT_H_ */
