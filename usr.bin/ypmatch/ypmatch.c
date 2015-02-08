/*	$OpenBSD: ypmatch.c,v 1.15 2015/01/16 06:40:15 deraadt Exp $ */
/*	$NetBSD: ypmatch.c,v 1.8 1996/05/07 01:24:52 jtc Exp $	*/

/*
 * Copyright (c) 1992, 1993, 1996 Theo de Raadt <deraadt@theos.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

void	usage(void);

struct ypalias {
	char *alias, *name;
} ypaliases[] = {
	{ "passwd", "passwd.byname" },
	{ "group", "group.byname" },
	{ "networks", "networks.byaddr" },
	{ "hosts", "hosts.byname" },
	{ "protocols", "protocols.bynumber" },
	{ "services", "services.byname" },
	{ "aliases", "mail.aliases" },
	{ "ethers", "ethers.byname" },
};

void
usage(void)
{
	fprintf(stderr,
	    "usage: ypmatch [-kt] [-d domain] key ... mapname\n"
	    "       ypmatch -x\n");
	fprintf(stderr,
	    "where\n"
	    "\tmapname may be either a mapname or a nickname for a map.\n"
	    "\t-k prints keys as well as values.\n"
	    "\t-t inhibits map nickname translation.\n"
	    "\t-x dumps the map nickname translation table.\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *domainname, *inkey, *inmap, *outbuf;
	extern char *optarg;
	extern int optind;
	int outbuflen, key, notrans, rval;
	int c, r, i;

	domainname = NULL;
	notrans = key = 0;
	while ((c=getopt(argc, argv, "xd:kt")) != -1)
		switch (c) {
		case 'x':
			for(i=0; i<sizeof ypaliases/sizeof ypaliases[0]; i++)
				printf("Use \"%s\" for \"%s\"\n",
					ypaliases[i].alias,
					ypaliases[i].name);
			exit(0);
		case 'd':
			domainname = optarg;
			break;
		case 't':
			notrans = 1;
			break;
		case 'k':
			key = 1;
			break;
		default:
			usage();
		}

	if ((argc-optind) < 2 )
		usage();

	if (!domainname) {
		yp_get_default_domain(&domainname);
	}

	inmap = argv[argc-1];
	if (!notrans) {
		for(i=0; i<sizeof ypaliases/sizeof ypaliases[0]; i++)
			if (strcmp(inmap, ypaliases[i].alias) == 0)
				inmap = ypaliases[i].name;
	}

	rval = 0;
	for(; optind < argc-1; optind++) {
		inkey = argv[optind];

		r = yp_match(domainname, inmap, inkey,
			strlen(inkey), &outbuf, &outbuflen);
		switch (r) {
		case 0:
			if (key)
				printf("%s: ", inkey);
			printf("%*.*s\n", outbuflen, outbuflen, outbuf);
			break;
		case YPERR_YPBIND:
			fprintf(stderr, "yp_match: not running ypbind\n");
			exit(1);
		default:
			fprintf(stderr, "Can't match key %s in map %s. Reason: %s\n",
			    inkey, inmap, yperr_string(r));
			rval = 1;
			break;
		}
	}
	exit(rval);
}
