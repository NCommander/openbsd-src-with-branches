/*	$OpenBSD: pt_tcp.c,v 1.7 2003/06/02 20:06:16 millert Exp $	*/
/*	$NetBSD: pt_tcp.c,v 1.9 1995/05/21 15:33:22 mycroft Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: Id: pt_tcp.c,v 1.1 1992/05/25 21:43:09 jsp Exp
 *	@(#)pt_tcp.c	8.5 (Berkeley) 4/28/95
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "portald.h"

/*
 * Key will be tcp/host/port[/"priv"]
 * Create a TCP socket connected to the
 * requested host and port.
 * Some trailing suffix values have special meanings.
 * An unrecognised suffix is an error.
 */
int
portal_tcp(struct portal_cred *pcr, char *key, char **v, int kso, int *fdp)
{
	char host[MAXHOSTNAMELEN];
	char port[MAXHOSTNAMELEN];
	char *p = key + (v[1] ? strlen(v[1]) : 0);
	char *q;
	struct hostent *hp;
	struct servent *sp;
	struct in_addr **ipp;
	struct in_addr *ip[2];
	struct in_addr ina;
	int s_port;
	int priv = 0;
	struct sockaddr_in sain;

	q = strchr(p, '/');
	if (q == 0 || q - p >= sizeof(host))
		return (EINVAL);
	*q = '\0';
	(void)strlcpy(host, p, sizeof(host));
	p = q + 1;

	q = strchr(p, '/');
	if (q)
		*q = '\0';
	if (strlen(p) >= sizeof(port))
		return (EINVAL);
	(void)strlcpy(port, p, sizeof(port));
	if (q) {
		p = q + 1;
		if (strcmp(p, "priv") == 0) {
			if (pcr->pcr_uid == 0)
				priv = 1;
			else
				return (EPERM);
		} else {
			return (EINVAL);
		}
	}

	if (inet_aton(host, &ina) == 0) {
		hp = gethostbyname(host);
		if (hp == 0)
			return (EINVAL);
		ipp = (struct in_addr **)hp->h_addr_list;
	} else {
		ip[0] = &ina;
		ip[1] = 0;
		ipp = ip;
	}

	sp = getservbyname(port, "tcp");
	if (sp != 0)
		s_port = sp->s_port;
	else {
		s_port = strtoul(port, &p, 0);
		if (s_port == 0 || *p != '\0')
			return (EINVAL);
		s_port = htons(s_port);
	}

	(void)memset(&sain, 0, sizeof(sain));
	sain.sin_len = sizeof(sain);
	sain.sin_family = AF_INET;
	sain.sin_port = s_port;

	while (ipp[0]) {
		int so;

		if (priv)
			so = rresvport(NULL);
		else
			so = socket(AF_INET, SOCK_STREAM, 0);
		if (so < 0) {
			syslog(LOG_ERR, "socket: %m");
			return (errno);
		}

		sain.sin_addr = *ipp[0];
		if (connect(so, (struct sockaddr *) &sain, sizeof(sain)) == 0) {
			*fdp = so;
			return (0);
		}
		(void)close(so);

		ipp++;
	}

	return (errno);
}
