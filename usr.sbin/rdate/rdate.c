/*
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: rdate.c,v 1.1 1994/06/02 22:55:07 deraadt Exp $
 */

/*
 * rdate.c: Set the date from the specified host
 * 
 * 	Uses the rfc868 time protocol at socket 37.
 *	Time is returned as the number of seconds since
 *	midnight January 1st 1900.
 */
#ifndef lint
static char rcsid[] = "$Id: rdate.c,v 1.1 1994/06/02 22:55:07 deraadt Exp $";
#endif				/* lint */

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

/* seconds from midnight Jan 1900 - 1970 */
#if __STDC__
#define DIFFERENCE 2208988800UL
#else
#define DIFFERENCE 2208988800
#endif

int
main(argc, argv)
	int             argc;
	char           *argv[];
{
	int             pr = 0, silent = 0, s;
	time_t          tim;
	char           *hname;
	struct hostent *hp;
	struct protoent *pp, ppp;
	struct servent *sp, ssp;
	struct sockaddr_in sa;
	extern char    *__progname;
	extern int      optind;
	int             c;

	while ((c = getopt(argc, argv, "ps")) != -1)
		switch (c) {
		case 'p':
			pr++;
			break;

		case 's':
			silent++;
			break;

		default:
			goto usage;
		}

	if (argc - 1 != optind) {
usage:
		(void) fprintf(stderr, "Usage: %s [-ps] host\n", __progname);
		return (1);
	}
	hname = argv[optind];

	if ((hp = gethostbyname(hname)) == NULL) {
		fprintf(stderr, "%s: ", __progname);
		herror(hname);
		exit(1);
	}

	if ((sp = getservbyname("time", "tcp")) == NULL) {
		sp = &ssp;
		sp->s_port = 37;
		sp->s_proto = "tcp";
	}
	if ((pp = getprotobyname(sp->s_proto)) == NULL) {
		pp = &ppp;
		pp->p_proto = 6;
	}
	if ((s = socket(AF_INET, SOCK_STREAM, pp->p_proto)) == -1)
		err(1, "Could not create socket");

	bzero(&sa, sizeof sa);
	sa.sin_family = AF_INET;
	sa.sin_port = sp->s_port;

	memcpy(&(sa.sin_addr.s_addr), hp->h_addr, hp->h_length);

	if (connect(s, (struct sockaddr *) & sa, sizeof(sa)) == -1)
		err(1, "Could not connect socket");

	if (read(s, &tim, sizeof(time_t)) != sizeof(time_t))
		err(1, "Could not read data");

	(void) close(s);
	tim = ntohl(tim) - DIFFERENCE;

	if (!pr) {
		struct timeval  tv;
		tv.tv_sec = tim;
		tv.tv_usec = 0;
		if (settimeofday(&tv, NULL) == -1)
			err(1, "Could not set time of day");
	}
	if (!silent)
		(void) fputs(ctime(&tim), stdout);
	return 0;
}
