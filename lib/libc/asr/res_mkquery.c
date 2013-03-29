/*	$OpenBSD: res_mkquery.c,v 1.3 2012/11/24 15:12:48 eric Exp $	*/
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
#include <netinet/in.h>
#include <arpa/nameser.h> /* for MAXDNAME */

#include <errno.h>
#include <resolv.h>
#include <string.h>

#include "asr.h"
#include "asr_private.h"

/* This function is apparently needed by some ports. */
int
res_mkquery(int op, const char *dname, int class, int type,
    const unsigned char *data, int datalen, const unsigned char *newrr,
    unsigned char *buf, int buflen)
{
	struct asr_ctx	*ac;
	struct pack	 p;
	struct header	 h;
	char		 fqdn[MAXDNAME];
	char		 dn[MAXDNAME];

	/* we currently only support QUERY */
	if (op != QUERY || data)
		return (-1);

	if (dname[0] == '\0' || dname[strlen(dname) - 1] != '.') {
		strlcpy(fqdn, dname, sizeof fqdn);
		if (strlcat(fqdn, ".", sizeof fqdn) >= sizeof fqdn)
			return (-1);
		dname = fqdn;
	}

	if (dname_from_fqdn(dname, dn, sizeof(dn)) == -1)
		return (-1);

	ac = asr_use_resolver(NULL);

	memset(&h, 0, sizeof h);
	h.id = res_randomid();
	if (ac->ac_options & RES_RECURSE)
		h.flags |= RD_MASK;
	h.qdcount = 1;

	pack_init(&p, buf, buflen);
	pack_header(&p, &h);
	pack_query(&p, type, class, dn);

	asr_ctx_unref(ac);

	if (p.err)
		return (-1);

	return (p.offset);
}

/*
 * This function is not documented, but used by sendmail.
 * Put here because it uses asr_private.h too.
 */
int
res_querydomain(const char *name,
    const char *domain,
    int class,
    int type,
    u_char *answer,
    int anslen)
{
	char	fqdn[MAXDNAME], ndom[MAXDNAME];
	size_t	n;

	/* we really want domain to end with a dot for now */
	if (domain && ((n = strlen(domain)) == 0 || domain[n - 1 ] != '.')) {
		strlcpy(ndom, domain, sizeof ndom);
		strlcat(ndom, ".", sizeof ndom);
		domain = ndom;
	}

	if (asr_make_fqdn(name, domain, fqdn, sizeof fqdn) == 0) {
		h_errno = NO_RECOVERY;
		errno = EINVAL;
		return (-1);
	}

	return (res_query(fqdn, class, type, answer, anslen));
}
