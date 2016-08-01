/*	$OpenBSD: ctl.c,v 1.12 2010/08/12 23:31:29 tedu Exp $	*/
/*	$NetBSD: ctl.c,v 1.3 1994/12/09 02:14:10 jtc Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
 */

/*
 * This file handles haggling with the various talk daemons to
 * get a socket to talk to. sockt is opened and connected in
 * the progress
 */

#include <sys/socket.h>
#include <arpa/inet.h>

#include "talk.h"

struct	sockaddr_in daemon_addr = { sizeof(daemon_addr), AF_INET };
struct	sockaddr_in ctl_addr = { sizeof(ctl_addr), AF_INET };
struct	sockaddr_in my_addr = { sizeof(my_addr), AF_INET };

	/* inet addresses of the two machines */
struct	in_addr my_machine_addr;
struct	in_addr his_machine_addr;

u_short daemon_port;	/* port number of the talk daemon */

int	ctl_sockt;
int	sockt;
int	invitation_waiting;

CTL_MSG msg;

void
open_sockt(void)
{
	socklen_t length;

	my_addr.sin_addr = my_machine_addr;
	my_addr.sin_port = 0;
	sockt = socket(AF_INET, SOCK_STREAM, 0);
	if (sockt == -1)
		quit("Bad socket", 1);
	if (bind(sockt, (struct sockaddr *)&my_addr, sizeof(my_addr)) != 0)
		quit("Binding local socket", 1);
	length = sizeof(my_addr);
	if (getsockname(sockt, (struct sockaddr *)&my_addr, &length) == -1)
		quit("Bad address for socket", 1);
}

/* open the ctl socket */
void
open_ctl(void)
{
	socklen_t length;

	ctl_addr.sin_port = 0;
	ctl_addr.sin_addr = my_machine_addr;
	ctl_sockt = socket(AF_INET, SOCK_DGRAM, 0);
	if (ctl_sockt == -1)
		quit("Bad socket", 1);
	if (bind(ctl_sockt,
	    (struct sockaddr *)&ctl_addr, sizeof(ctl_addr)) != 0)
		quit("Couldn't bind to control socket", 1);
	length = sizeof(ctl_addr);
	if (getsockname(ctl_sockt,
	    (struct sockaddr *)&ctl_addr, &length) == -1)
		quit("Bad address for ctl socket", 1);
}
