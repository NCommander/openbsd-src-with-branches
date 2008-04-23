/*	$OpenBSD: dump.c,v 1.7 2008/04/21 20:40:55 rainer Exp $	*/
/*	$KAME: dump.c,v 1.27 2002/05/29 14:23:55 itojun Exp $	*/

/*
 * Copyright (C) 2000 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>

#include <netinet/in.h>

/* XXX: the following two are non-standard include files */
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>

#include <arpa/inet.h>

#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>

#include "rtadvd.h"
#include "timer.h"
#include "if.h"
#include "dump.h"
#include "log.h"

static FILE *fp;

extern struct ralist ralist;

static char *ether_str(struct sockaddr_dl *);
static void if_dump(void);

static char *rtpref_str[] = {
	"medium",		/* 00 */
	"high",			/* 01 */
	"rsv",			/* 10 */
	"low"			/* 11 */
};

static char *
ether_str(sdl)
	struct sockaddr_dl *sdl;
{
	static char hbuf[NI_MAXHOST];
	u_char *cp;

	if (sdl->sdl_alen) {
		cp = (u_char *)LLADDR(sdl);
		snprintf(hbuf, sizeof(hbuf), "%x:%x:%x:%x:%x:%x",
			cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
	} else
		snprintf(hbuf, sizeof(hbuf), "NONE");

	return(hbuf);
}

static void
if_dump()
{
	struct rainfo *rai;
	struct prefix *pfx;
	char prefixbuf[INET6_ADDRSTRLEN];
	int first;
	struct timeval now;

	gettimeofday(&now, NULL); /* XXX: unused in most cases */
	SLIST_FOREACH(rai, &ralist, entry) {
		fprintf(fp, "%s:\n", rai->ifname);

		fprintf(fp, "  Status: %s\n",
			(iflist[rai->ifindex]->ifm_flags & IFF_UP) ? "UP" :
			"DOWN");

		/* control information */
		if (rai->lastsent.tv_sec) {
			/* note that ctime() appends CR by itself */
			fprintf(fp, "  Last RA sent: %s",
				ctime((time_t *)&rai->lastsent.tv_sec));
		}
		if (rai->timer) {
			fprintf(fp, "  Next RA will be sent: %s",
				ctime((time_t *)&rai->timer->tm.tv_sec));
		}
		else
			fprintf(fp, "  RA timer is stopped");
		fprintf(fp, "  waits: %d, initcount: %d\n",
			rai->waiting, rai->initcounter);

		/* statistics */
		fprintf(fp, "  statistics: RA(out/in/inconsistent): "
		    "%llu/%llu/%llu, ",
		    (unsigned long long)rai->raoutput,
		    (unsigned long long)rai->rainput,
		    (unsigned long long)rai->rainconsistent);
		fprintf(fp, "RS(input): %llu\n",
		    (unsigned long long)rai->rsinput);

		/* interface information */
		if (rai->advlinkopt)
			fprintf(fp, "  Link-layer address: %s\n",
			    ether_str(rai->sdl));
		fprintf(fp, "  MTU: %d\n", rai->phymtu);

		/* Router configuration variables */
		fprintf(fp, "  DefaultLifetime: %d, MaxAdvInterval: %d, "
		    "MinAdvInterval: %d\n", rai->lifetime, rai->maxinterval,
		    rai->mininterval);
		fprintf(fp, "  Flags: %s%s%s, ",
		    rai->managedflg ? "M" : "", rai->otherflg ? "O" : "",
		    "");
		fprintf(fp, "Preference: %s, ",
			rtpref_str[(rai->rtpref >> 3) & 0xff]);
		fprintf(fp, "MTU: %d\n", rai->linkmtu);
		fprintf(fp, "  ReachableTime: %d, RetransTimer: %d, "
			"CurHopLimit: %d\n", rai->reachabletime,
			rai->retranstimer, rai->hoplimit);
		if (rai->clockskew)
			fprintf(fp, "  Clock skew: %ldsec\n",
			    rai->clockskew);
		first = 1;
		TAILQ_FOREACH(pfx, &rai->prefixes, entry) {
			if (first) {
				fprintf(fp, "  Prefixes:\n");
				first = 0;
			}
			fprintf(fp, "    %s/%d(",
			    inet_ntop(AF_INET6, &pfx->prefix, prefixbuf,
			    sizeof(prefixbuf)), pfx->prefixlen);
			switch (pfx->origin) {
			case PREFIX_FROM_KERNEL:
				fprintf(fp, "KERNEL, ");
				break;
			case PREFIX_FROM_CONFIG:
				fprintf(fp, "CONFIG, ");
				break;
			case PREFIX_FROM_DYNAMIC:
				fprintf(fp, "DYNAMIC, ");
				break;
			}
			if (pfx->validlifetime == ND6_INFINITE_LIFETIME)
				fprintf(fp, "vltime: infinity");
			else
				fprintf(fp, "vltime: %ld",
					(long)pfx->validlifetime);
			if (pfx->vltimeexpire != 0)
				fprintf(fp, "(decr,expire %ld), ", (long)
					pfx->vltimeexpire > now.tv_sec ?
					pfx->vltimeexpire - now.tv_sec : 0);
			else
				fprintf(fp, ", ");
			if (pfx->preflifetime ==  ND6_INFINITE_LIFETIME)
				fprintf(fp, "pltime: infinity");
			else
				fprintf(fp, "pltime: %ld",
					(long)pfx->preflifetime);
			if (pfx->pltimeexpire != 0)
				fprintf(fp, "(decr,expire %ld), ", (long)
					pfx->pltimeexpire > now.tv_sec ?
					pfx->pltimeexpire - now.tv_sec : 0);
			else
				fprintf(fp, ", ");
			fprintf(fp, "flags: %s%s%s",
				pfx->onlinkflg ? "L" : "",
				pfx->autoconfflg ? "A" : "",
				"");
			fprintf(fp, ")\n");
		}
	}
}

void
rtadvd_dump_file(dumpfile)
	char *dumpfile;
{
	if ((fp = fopen(dumpfile, "w")) == NULL) {
		log_warn("open a dump file(%s)", dumpfile);
		return;
	}

	if_dump();

	fclose(fp);
}
