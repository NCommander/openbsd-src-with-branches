/*	$OpenBSD: getnetnamadr_async.c,v 1.5 2012/11/24 13:59:53 eric Exp $	*/
/*
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asr.h"
#include "asr_private.h"

#define MAXALIASES	16

#define NETENT_PTR(n) ((char**)(((char*)n) + sizeof (*n)))
#define NETENT_POS(n)  NETENT_PTR(n)[0]
#define NETENT_STOP(n)	NETENT_PTR(n)[1]
#define NETENT_LEFT(n) (NETENT_STOP(n) - NETENT_POS(n))

ssize_t addr_as_fqdn(const char *, int, char *, size_t);

static int getnetnamadr_async_run(struct async *, struct async_res *);
static struct netent *netent_alloc(int);
static int netent_set_cname(struct netent *, const char *, int);
static int netent_add_alias(struct netent *, const char *, int);
static struct netent *netent_file_match(FILE *, int, const char *);
static struct netent *netent_from_packet(int, char *, size_t);

struct async *
getnetbyname_async(const char *name, struct asr *asr)
{
	struct asr_ctx	*ac;
	struct async	*as;

	/* The current resolver segfaults. */
	if (name == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	ac = asr_use_resolver(asr);
	if ((as = async_new(ac, ASR_GETNETBYNAME)) == NULL)
		goto abort; /* errno set */
	as->as_run = getnetnamadr_async_run;

	as->as.netnamadr.family = AF_INET;
	as->as.netnamadr.name = strdup(name);
	if (as->as.netnamadr.name == NULL)
		goto abort; /* errno set */

	asr_ctx_unref(ac);
	return (as);

    abort:
	if (as)
		async_free(as);
	asr_ctx_unref(ac);
	return (NULL);
}

struct async *
getnetbyaddr_async(in_addr_t net, int family, struct asr *asr)
{
	struct asr_ctx	*ac;
	struct async	*as;

	ac = asr_use_resolver(asr);
	if ((as = async_new(ac, ASR_GETNETBYADDR)) == NULL)
		goto abort; /* errno set */
	as->as_run = getnetnamadr_async_run;

	as->as.netnamadr.family = family;
	as->as.netnamadr.addr = net;

	asr_ctx_unref(ac);
	return (as);

    abort:
	if (as)
		async_free(as);
	asr_ctx_unref(ac);
	return (NULL);
}

static int
getnetnamadr_async_run(struct async *as, struct async_res *ar)
{
	int		 r, type;
	FILE		*f;
	char		 dname[MAXDNAME], *name, *data;
	in_addr_t	 in;

    next:
	switch (as->as_state) {

	case ASR_STATE_INIT:

		if (as->as.netnamadr.family != AF_INET) {
			ar->ar_h_errno = NETDB_INTERNAL;
			ar->ar_errno = EAFNOSUPPORT;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		async_set_state(as, ASR_STATE_NEXT_DB);
		break;

	case ASR_STATE_NEXT_DB:

		if (asr_iter_db(as) == -1) {
			async_set_state(as, ASR_STATE_NOT_FOUND);
			break;
		}

		switch (AS_DB(as)) {
		case ASR_DB_DNS:

			if (as->as_type == ASR_GETNETBYNAME) {
				type = T_A;
				/*
				 * I think we want to do the former, but our
				 * resolver is doing the following, so let's
				 * preserve bugward-compatibility there.
				 */
				type = T_PTR;
				name = as->as.netnamadr.name;
				as->as.netnamadr.subq = res_search_async_ctx(
				    name, C_IN, type, NULL, 0, as->as_ctx);
			} else {
				type = T_PTR;
				name = dname;

				in = htonl(as->as.netnamadr.addr);
				addr_as_fqdn((char*)&in,
				    as->as.netnamadr.family,
				    dname, sizeof(dname));
				as->as.netnamadr.subq = res_query_async_ctx(
				    name, C_IN, type, NULL, 0, as->as_ctx);
			}

			if (as->as.netnamadr.subq == NULL) {
				ar->ar_errno = errno;
				ar->ar_h_errno = NETDB_INTERNAL;
				async_set_state(as, ASR_STATE_HALT);
			}
			async_set_state(as, ASR_STATE_SUBQUERY);
			break;

		case ASR_DB_FILE:

			if ((f = fopen("/etc/networks", "r")) == NULL)
				break;

			if (as->as_type == ASR_GETNETBYNAME)
				data = as->as.netnamadr.name;
			else
				data = (void*)&as->as.netnamadr.addr;

			ar->ar_netent = netent_file_match(f, as->as_type, data);
			fclose(f);

			if (ar->ar_netent == NULL) {
				if (errno) {
					ar->ar_errno = errno;
					ar->ar_h_errno = NETDB_INTERNAL;
					async_set_state(as, ASR_STATE_HALT);
				}
				/* otherwise not found */
				break;
			}

			ar->ar_h_errno = NETDB_SUCCESS;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}
		break;

	case ASR_STATE_SUBQUERY:

		if ((r = async_run(as->as.netnamadr.subq, ar)) == ASYNC_COND)
			return (ASYNC_COND);
		as->as.netnamadr.subq = NULL;

		if (ar->ar_datalen == -1) {
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		/* Got packet, but no answer */
		if (ar->ar_count == 0) {
			free(ar->ar_data);
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		ar->ar_netent = netent_from_packet(as->as_type, ar->ar_data,
		    ar->ar_datalen);
		free(ar->ar_data);

		if (ar->ar_netent == NULL) {
			ar->ar_errno = errno;
			ar->ar_h_errno = NETDB_INTERNAL;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		if (as->as_type == ASR_GETNETBYADDR)
			ar->ar_netent->n_net = as->as.netnamadr.addr;

		/*
		 * No address found in the dns packet. The blocking version
		 * reports this as an error.
		 */
		if (as->as_type == ASR_GETNETBYNAME &&
		    ar->ar_netent->n_net == 0) {
			 /* XXX wrong */
			free(ar->ar_netent);
			async_set_state(as, ASR_STATE_NEXT_DB);
		} else {
			ar->ar_h_errno = NETDB_SUCCESS;
			async_set_state(as, ASR_STATE_HALT);
		}
		break;

	case ASR_STATE_NOT_FOUND:

		ar->ar_errno = 0;
		ar->ar_h_errno = HOST_NOT_FOUND;
		async_set_state(as, ASR_STATE_HALT);
		break;

	case ASR_STATE_HALT:

		if (ar->ar_h_errno)
			ar->ar_netent = NULL;
		else
			ar->ar_errno = 0;
		return (ASYNC_DONE);

	default:
		ar->ar_errno = EOPNOTSUPP;
		ar->ar_h_errno = NETDB_INTERNAL;
		ar->ar_gai_errno = EAI_SYSTEM;
		async_set_state(as, ASR_STATE_HALT);
		break;
	}
	goto next;
}

static struct netent *
netent_file_match(FILE *f, int reqtype, const char *data)
{
	struct netent	*e;
	char		*tokens[MAXTOKEN];
	int		 n, i;
	in_addr_t	 net;

	for (;;) {
		n = asr_parse_namedb_line(f, tokens, MAXTOKEN);
		if (n == -1) {
			errno = 0; /* ignore errors reading the file */
			return (NULL);
		}

		if (reqtype == ASR_GETNETBYADDR) {
			net = inet_network(tokens[1]);
			if (memcmp(&net, data, sizeof net) == 0)
				goto found;
		} else {
			for (i = 0; i < n; i++) {
				if (i == 1)
					continue;
				if (strcasecmp(data, tokens[i]))
					continue;
				goto found;
			}
		}
	}

found:
	if ((e = netent_alloc(AF_INET)) == NULL)
		return (NULL);
	if (netent_set_cname(e, tokens[0], 0) == -1)
		goto fail;
	for (i = 2; i < n; i ++)
		if (netent_add_alias(e, tokens[i], 0) == -1)
			goto fail;
	e->n_net = inet_network(tokens[1]);
	return (e);
fail:
	free(e);
	return (NULL);
}

static struct netent *
netent_from_packet(int reqtype, char *pkt, size_t pktlen)
{
	struct netent	*n;
	struct unpack	 p;
	struct header	 hdr;
	struct query	 q;
	struct rr	 rr;

	if ((n = netent_alloc(AF_INET)) == NULL)
		return (NULL);

	unpack_init(&p, pkt, pktlen);
	unpack_header(&p, &hdr);
	for (; hdr.qdcount; hdr.qdcount--)
		unpack_query(&p, &q);
	for (; hdr.ancount; hdr.ancount--) {
		unpack_rr(&p, &rr);
		if (rr.rr_class != C_IN)
			continue;
		switch (rr.rr_type) {
		case T_CNAME:
			if (reqtype == ASR_GETNETBYNAME) {
				if (netent_add_alias(n, rr.rr_dname, 1) == -1)
					goto fail;
			} else {
				if (netent_set_cname(n, rr.rr_dname, 1) == -1)
					goto fail;
			}
			break;

		case T_PTR:
			if (reqtype != ASR_GETNETBYADDR)
				continue;
			if (netent_set_cname(n, rr.rr.ptr.ptrname, 1) == -1)
				goto fail;
			/* XXX See if we need to have MULTI_PTRS_ARE_ALIASES */
			break;

		case T_A:
			if (n->n_addrtype != AF_INET)
				break;
			if (netent_set_cname(n, rr.rr_dname, 1) ==  -1)
				goto fail;
			n->n_net = ntohl(rr.rr.in_a.addr.s_addr);
			break;
		}
	}

	return (n);
fail:
	free(n);
	return (NULL);
}

static struct netent *
netent_alloc(int family)
{
	struct netent	*n;
	size_t		 alloc;

	alloc = sizeof(*n) + (2 + MAXALIASES) * sizeof(char*) + 1024;
	if ((n = calloc(1, alloc)) == NULL)
		return (NULL);

	n->n_addrtype = family;
	n->n_aliases = NETENT_PTR(n) + 2;

	NETENT_STOP(n) = (char*)(n) + alloc;
	NETENT_POS(n) = (char *)(n->n_aliases + MAXALIASES);

	return (n);
}

static int
netent_set_cname(struct netent *n, const char *name, int isdname)
{
	char	buf[MAXDNAME];

	if (n->n_name)
		return (0);

	if (isdname) {
		asr_strdname(name, buf, sizeof buf);
		buf[strlen(buf) - 1] = '\0';
		name = buf;
	}
	if (strlen(name) + 1 >= NETENT_LEFT(n))
		return (1);

	strlcpy(NETENT_POS(n), name, NETENT_LEFT(n));
	n->n_name = NETENT_POS(n);
	NETENT_POS(n) += strlen(name) + 1;

	return (0);
}

static int
netent_add_alias(struct netent *n, const char *name, int isdname)
{
	char	buf[MAXDNAME];
	size_t	i;

	for (i = 0; i < MAXALIASES - 1; i++)
		if (n->n_aliases[i] == NULL)
			break;
	if (i == MAXALIASES - 1)
		return (0);

	if (isdname) {
		asr_strdname(name, buf, sizeof buf);
		buf[strlen(buf)-1] = '\0';
		name = buf;
	}
	if (strlen(name) + 1 >= NETENT_LEFT(n))
		return (1);

	strlcpy(NETENT_POS(n), name, NETENT_LEFT(n));
	n->n_aliases[i] = NETENT_POS(n);
	NETENT_POS(n) += strlen(name) + 1;

	return (0);
}
