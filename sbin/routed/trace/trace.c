/*	$NetBSD: trace.c,v 1.9 1995/06/20 22:28:11 christos Exp $	*/

/*-
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)trace.c	8.1 (Berkeley) 6/5/93";
#else
static char rcsid[] = "$NetBSD: trace.c,v 1.9 1995/06/20 22:28:11 christos Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <protocols/routed.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct	sockaddr_in myaddr;
char	packet[MAXPACKETSIZE];

main(argc, argv)
	int argc;
	char **argv;
{
	int size, s;
	struct sockaddr from;
	struct sockaddr_in router;
	register struct rip *msg = (struct rip *)packet;
	struct hostent *hp;
	struct servent *sp;
	
	if (argc < 3) {
usage:
		printf("usage: trace cmd machines,\n");
		printf("cmd either \"on filename\", or \"off\"\n");
		exit(1);
	}
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket");
		exit(2);
	}
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(IPPORT_RESERVED-1);
	if (bind(s, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
		perror("bind");
		exit(2);
	}

	argv++, argc--;
	msg->rip_cmd = strcmp(*argv, "on") == 0 ?
		RIPCMD_TRACEON : RIPCMD_TRACEOFF;
	msg->rip_vers = RIP_VERSION_1;
	argv++, argc--;
	size = sizeof (int);
	if (msg->rip_cmd == RIPCMD_TRACEON) {
		strcpy(msg->rip_tracefile, *argv);
		size += strlen(*argv);
		argv++, argc--;
	}
	if (argc == 0)
		goto usage;
	memset(&router, 0, sizeof (router));
	router.sin_family = AF_INET;
	sp = getservbyname("router", "udp");
	if (sp == 0) {
		printf("udp/router: service unknown\n");
		exit(1);
	}
	router.sin_port = sp->s_port;
	while (argc > 0) {
		router.sin_family = AF_INET;
		if (inet_aton(*argv, &router.sin_addr) == 0) {
			hp = gethostbyname(*argv);
			if (hp == NULL) {
				fprintf(stderr, "trace: %s: ", *argv);
				herror((char *)NULL);
				continue;
			}
			memcpy(&router.sin_addr, hp->h_addr, hp->h_length);
		}
		if (sendto(s, packet, size, 0,
		    (struct sockaddr *)&router, sizeof(router)) < 0)
			perror(*argv);
		argv++, argc--;
	}
}
