/*	$OpenBSD: dns.c,v 1.34 2011/03/26 21:40:14 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <event.h>
#include <imsg.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asr.h"
#include "dnsdefs.h"
#include "dnsutil.h"
#include "smtpd.h"
#include "log.h"

struct dnssession *dnssession_init(struct smtpd *, struct dns *);
void	dnssession_destroy(struct smtpd *, struct dnssession *);
void	dnssession_mx_insert(struct dnssession *, struct mx *);
void	dns_asr_event_set(struct dnssession *, struct asr_result *, void(*)(int, short, void*));
void	dns_asr_handler(int, short, void *);
void	dns_asr_mx_handler(int, short, void *);
void	dns_asr_dispatch_cname(struct dnssession *);
void	lookup_host(struct imsgev *, struct dns *, int, int);
void	lookup_mx(struct imsgev *, struct dns *);
void	lookup_ptr(struct imsgev *, struct dns *);

struct asr *asr = NULL;

/*
 * User interface.
 */

void
dns_query_host(struct smtpd *env, char *host, int port, u_int64_t id)
{
	struct dns	 query;

	bzero(&query, sizeof(query));
	strlcpy(query.host, host, sizeof(query.host));
	query.port = port;
	query.id = id;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_DNS_HOST, 0, 0, -1,
	    &query, sizeof(query));
}

void
dns_query_mx(struct smtpd *env, char *host, int port, u_int64_t id)
{
	struct dns	 query;

	bzero(&query, sizeof(query));
	strlcpy(query.host, host, sizeof(query.host));
	query.port = port;
	query.id = id;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_DNS_MX, 0, 0, -1, &query,
	    sizeof(query));
}

void
dns_query_ptr(struct smtpd *env, struct sockaddr_storage *ss, u_int64_t id)
{
	struct dns	 query;

	bzero(&query, sizeof(query));
	query.ss = *ss;
	query.id = id;

	imsg_compose_event(env->sc_ievs[PROC_LKA], IMSG_DNS_PTR, 0, 0, -1, &query,
	    sizeof(query));
}

/* LKA interface */
void
dns_async(struct smtpd *env, struct imsgev *asker, int type, struct dns *query)
{
	struct dnssession *dnssession;

	if (asr == NULL && (asr = asr_resolver(NULL)) == NULL) {
		log_warnx("dns_async: cannot create resolver");
		goto noasr;
	}

	query->env   = env;
	query->type  = type;
	query->asker = asker;
	dnssession = dnssession_init(env, query);
	
	switch (type) {
	case IMSG_DNS_HOST:
		dnssession->aq = asr_query_host(asr, query->host, AF_UNSPEC);
		break;
	case IMSG_DNS_PTR:
		dnssession->aq = asr_query_cname(asr,
		    (struct sockaddr*)&query->ss, query->ss.ss_len);
		break;
	case IMSG_DNS_MX:
		dnssession->aq = asr_query_dns(asr, T_MX, C_IN, query->host, 0);
		break;
	default:
		goto err;
	}

	/* query and set up event to handle answer */
	if (dnssession->aq == NULL)
		goto err;
	dns_asr_handler(-1, -1, dnssession);
	return;

err:
	log_debug("dns_async: ASR error while attempting to resolve `%s'",
	    query->host);
	dnssession_destroy(env, dnssession);

noasr:
	query->error = EAI_AGAIN;
	if (type != IMSG_DNS_PTR)
		type = IMSG_DNS_HOST_END;
	imsg_compose_event(asker, type, 0, 0, -1, query, sizeof(*query));
}

void
dns_asr_event_set(struct dnssession *dnssession, struct asr_result *ar,
		  void (*handler)(int, short, void*))
{
	struct timeval tv = { 0, 0 };
	
	tv.tv_usec = ar->ar_timeout * 1000;
	event_set(&dnssession->ev, ar->ar_fd,
	    ar->ar_cond == ASR_READ ? EV_READ : EV_WRITE,
	    handler, dnssession);
	event_add(&dnssession->ev, &tv);
}

void
dns_asr_handler(int fd, short event, void *arg)
{
	struct dnssession *dnssession = arg;
	struct dns *query = &dnssession->query;
	struct smtpd *env = query->env;
	struct packed pack;
	struct header	h;
	struct query	q;
	struct rr rr;
	struct asr_result ar;
	char *p;
	int cnt;
	int ret;

	if (dnssession->query.type == IMSG_DNS_PTR) {
		dns_asr_dispatch_cname(dnssession);
		return;
	}

	switch ((ret = asr_run(dnssession->aq, &ar))) {
	case ASR_COND:
		dns_asr_event_set(dnssession, &ar, dns_asr_handler);
		return;

	case ASR_YIELD:
	case ASR_DONE:
		break;
	}

	query->error = EAI_AGAIN;

	if (ret == ASR_YIELD) {
		free(ar.ar_cname);
		memcpy(&query->ss, &ar.ar_sa.sa, ar.ar_sa.sa.sa_len);
		query->error = 0;
		imsg_compose_event(query->asker, IMSG_DNS_HOST, 0, 0, -1, query,
		    sizeof(*query));
		dns_asr_handler(-1, -1, dnssession);
		return;
	}

	/* ASR_DONE */
	if (ar.ar_err) {
		query->error = ar.ar_err;
		goto err;
	}

	if (query->type == IMSG_DNS_HOST) {
		query->error = 0;
		imsg_compose_event(query->asker, IMSG_DNS_HOST_END, 0, 0, -1,
		    query, sizeof(*query));
		dnssession_destroy(env, dnssession);
		return;
	}

	packed_init(&pack, ar.ar_data, ar.ar_datalen);
	if (unpack_header(&pack, &h) < 0 || unpack_query(&pack, &q) < 0)
		goto err;

	if (h.ancount == 0) {
		if (query->type == IMSG_DNS_MX) {
			/* we were looking for MX and got no answer,
			 * fallback to host.
			 */
			query->type = IMSG_DNS_HOST;
			dnssession->aq = asr_query_host(asr, query->host,
			    AF_UNSPEC);
			if (dnssession->aq == NULL)
				goto err;
			dns_asr_handler(-1, -1, dnssession);
			return;
		}
		query->error = EAI_NONAME;
		goto err;
	}

	if (query->type == IMSG_DNS_PTR) {
		if (h.ancount > 1) {
			log_debug("dns_asr_handler: PTR query returned several answers.");
			log_debug("dns_asr_handler: keeping only first result.");
		}
		if (unpack_rr(&pack, &rr) < 0)
			goto err;

		print_dname(rr.rr.ptr.ptrname, query->host, sizeof (query->host));
		if ((p = strrchr(query->host, '.')) != NULL)
			*p = '\0';
		free(ar.ar_data);
		
		query->error = 0;
		imsg_compose_event(query->asker, IMSG_DNS_PTR, 0, 0, -1, query,
		    sizeof(*query));
		dnssession_destroy(env, dnssession);
		return;
	}

	if (query->type == IMSG_DNS_MX) {
		struct mx mx;
		
		cnt = h.ancount;
		for (; cnt; cnt--) {
			if (unpack_rr(&pack, &rr) < 0)
				goto err;

			print_dname(rr.rr.mx.exchange, mx.host, sizeof (mx.host));
			if ((p = strrchr(mx.host, '.')) != NULL)
				*p = '\0';
			mx.prio =  rr.rr.mx.preference;

			/* sorted insert that will not overflow MAX_MX_COUNT */
			dnssession_mx_insert(dnssession, &mx);
		}
		free(ar.ar_data);
		ar.ar_data = NULL;

		/* The T_MX scenario is a bit trickier than T_PTR and T_A lookups.
		 * Rather than forwarding the answers to the process that queried,
		 * we retrieve a set of MX hosts ... that need to be resolved. The
		 * loop above sorts them by priority, all we have left to do is to
		 * perform T_A lookups on all of them sequentially and provide the
		 * process that queried with the answers.
		 *
		 * To make it easier, we do this in another handler.
		 *
		 * -- gilles@
		 */
		dnssession->mxfound = 0;
		dnssession->mxcurrent = 0;
		dnssession->aq = asr_query_host(asr,
		    dnssession->mxarray[dnssession->mxcurrent].host, AF_UNSPEC);
		if (dnssession->aq == NULL)
			goto err;

		dns_asr_mx_handler(-1, -1, dnssession);
		return;
	}
	return;

err:
	free(ar.ar_data);
	if (query->type != IMSG_DNS_PTR)
		query->type = IMSG_DNS_HOST_END;
	imsg_compose_event(query->asker, query->type, 0, 0, -1, query,
	    sizeof(*query));
	dnssession_destroy(env, dnssession);
}


/* only handle MX requests */
void
dns_asr_mx_handler(int fd, short event, void *arg)
{
	struct dnssession *dnssession = arg;
	struct dns *query = &dnssession->query;
	struct smtpd *env = query->env;
	struct asr_result ar;
	int ret;

	switch ((ret = asr_run(dnssession->aq, &ar))) {
	case ASR_COND:
		dns_asr_event_set(dnssession, &ar, dns_asr_mx_handler);
		return;

	case ASR_YIELD:
		free(ar.ar_cname);
		memcpy(&query->ss, &ar.ar_sa.sa, ar.ar_sa.sa.sa_len);
		query->error = 0;
		imsg_compose_event(query->asker, IMSG_DNS_HOST, 0, 0, -1, query,
		    sizeof(*query));
		dnssession->mxfound++;
		dns_asr_mx_handler(-1, -1, dnssession);
		return;

	case ASR_DONE:
		break;
	}

	if (++dnssession->mxcurrent == dnssession->mxarraysz)
		goto end;

	dnssession->aq = asr_query_host(asr,
	    dnssession->mxarray[dnssession->mxcurrent].host, AF_UNSPEC);
	if (dnssession->aq == NULL)
		goto end;
	dns_asr_mx_handler(-1, -1, dnssession);
	return;

end:
	if (dnssession->mxfound == 0)
		query->error = EAI_NONAME;
	else
		query->error = 0;
	imsg_compose_event(query->asker, IMSG_DNS_HOST_END, 0, 0, -1, query,
	    sizeof(*query));
	dnssession_destroy(env, dnssession);
	return;
}

void
dns_asr_dispatch_cname(struct dnssession *dnssession)
{
	struct dns		*query = &dnssession->query;
	struct asr_result	 ar;

	switch (asr_run(dnssession->aq, &ar)) {
	case ASR_COND:
		dns_asr_event_set(dnssession, &ar, dns_asr_handler);
		return;
	case ASR_YIELD:
		/* Only return the first answer */
		query->error = 0;
		strlcpy(query->host, ar.ar_cname, sizeof (query->host));
		asr_abort(dnssession->aq);
		free(ar.ar_cname);
		break;
	case ASR_DONE:
		/* This is necessarily an error */
		query->error = ar.ar_err;
		break;
	}
	imsg_compose_event(query->asker, IMSG_DNS_PTR, 0, 0, -1, query,
	    sizeof(*query));
	dnssession_destroy(query->env, dnssession);
}

struct dnssession *
dnssession_init(struct smtpd *env, struct dns *query)
{
	struct dnssession *dnssession;

	dnssession = calloc(1, sizeof(struct dnssession));
	if (dnssession == NULL)
		fatal("dnssession_init: calloc");

	dnssession->id = query->id;
	dnssession->query = *query;
	SPLAY_INSERT(dnstree, &env->dns_sessions, dnssession);
	return dnssession;
}

void
dnssession_destroy(struct smtpd *env, struct dnssession *dnssession)
{
	SPLAY_REMOVE(dnstree, &env->dns_sessions, dnssession);
	event_del(&dnssession->ev);
	free(dnssession);
}

void
dnssession_mx_insert(struct dnssession *dnssession, struct mx *mx)
{
	size_t i, j;

	for (i = 0; i < dnssession->mxarraysz; i++)
		if (mx->prio < dnssession->mxarray[i].prio)
			break;

	if (i == MAX_MX_COUNT)
		return;

	if (dnssession->mxarraysz < MAX_MX_COUNT)
		dnssession->mxarraysz++;

	for (j = dnssession->mxarraysz - 1; j > i; j--)
		dnssession->mxarray[j] = dnssession->mxarray[j - 1];

        dnssession->mxarray[i] = *mx;
}

int
dnssession_cmp(struct dnssession *s1, struct dnssession *s2)
{
	/*
	 * do not return u_int64_t's
	 */
	if (s1->id < s2->id)
		return (-1);

	if (s1->id > s2->id)
		return (1);

	return (0);
}

SPLAY_GENERATE(dnstree, dnssession, nodes, dnssession_cmp);
