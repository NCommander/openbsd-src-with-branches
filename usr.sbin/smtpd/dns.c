/*	$OpenBSD: dns.c,v 1.9 2009/02/15 13:12:19 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <event.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"

struct mxrecord {
	char hostname[MAXHOSTNAMELEN];
	u_int16_t priority;
};

static void mxsort(struct mxrecord *, size_t);

static void
mxsort(struct mxrecord *array, size_t len)
{
	u_int32_t i;
	int32_t j;
	struct mxrecord store;

	for (i = 1; i < len; i++) {
		store = array[i];
		for (j = i - 1; j >= 0 && array[j].priority > store.priority;
		    j--) {
			array[j + 1] = array[j];
		}
		array[j + 1] = store;
	}
}

int
getmxbyname(char *name, char ***result)
{
	union {
		u_int8_t bytes[PACKETSZ];
		HEADER header;
	} answer;
	u_int32_t i, j;
	int ret;
	u_int8_t *sp;
	u_int8_t *endp;
	u_int8_t *ptr;
	u_int16_t qdcount;
	u_int8_t expbuf[PACKETSZ];
	u_int16_t type;
	u_int16_t n;
	u_int16_t priority;
	size_t mxnb;
	struct mxrecord mxarray[MXARRAYSIZE];
	size_t chunklen;

	ret = res_query(name, C_IN, T_MX, (u_int8_t *)&answer.bytes,
		sizeof answer);
	if (ret == -1) {
		switch (h_errno) {
		case TRY_AGAIN:
			return (EAI_AGAIN);
		case NO_RECOVERY:
			return (EAI_FAIL);
		case HOST_NOT_FOUND:
			return (EAI_NONAME);
		case NO_DATA:
			return (0);
		}
		fatal("getmxbyname: res_query");
	}

	/* sp stores start of dns packet,
	 * endp stores end of dns packet,
	 */
	sp = (u_int8_t *)&answer.bytes;
	endp = sp + ret;

	/* skip header */
	ptr = sp + HFIXEDSZ;

	for (qdcount = ntohs(answer.header.qdcount);
	     qdcount--;
	     ptr += ret + QFIXEDSZ) {
		ret = dn_skipname(ptr, endp);
		if (ret == -1)
			return 0;
	}

	mxnb = 0;
	for (; ptr < endp;) {
		memset(expbuf, 0, sizeof expbuf);
		ret = dn_expand(sp, endp, ptr, expbuf, sizeof expbuf);
		if (ret == -1)
			break;
		ptr += ret;

		GETSHORT(type, ptr);
		ptr += sizeof(u_int16_t) + sizeof(u_int32_t);
		GETSHORT(n, ptr);

		if (type != T_MX) {
			ptr += n;
			continue;
		}

		GETSHORT(priority, ptr);
		ret = dn_expand(sp, endp, ptr, expbuf, sizeof expbuf);
		if (ret == -1)
			return 0;
		ptr += ret;

		if (mxnb < sizeof(mxarray) / sizeof(struct mxrecord)) {
			if (strlcpy(mxarray[mxnb].hostname, expbuf,
			    sizeof(mxarray[mxnb].hostname)) >=
			    sizeof(mxarray[mxnb].hostname))
				return 0;
			mxarray[mxnb].priority = priority;
		}
		else {
			int tprio = 0;

			for (i = j = 0;
				i < sizeof(mxarray) / sizeof(struct mxrecord);
				++i) {
				if (tprio < mxarray[i].priority) {
					tprio = mxarray[i].priority;
					j = i;
				}
			}

			if (mxarray[j].priority > priority) {
				if (strlcpy(mxarray[j].hostname, expbuf,
				    sizeof(mxarray[j].hostname)) >=
				    sizeof(mxarray[j].hostname))
					return 0;
				mxarray[j].priority = priority;
			}
		}
		++mxnb;
	}

	if (mxnb == 0)
		return 0;

	if (mxnb > sizeof(mxarray) / sizeof(struct mxrecord))
		mxnb = sizeof(mxarray) / sizeof(struct mxrecord);

	/* Rearrange MX records by priority */
	mxsort(mxarray, mxnb);

	chunklen = 0;
	for (i = 0; i < mxnb; ++i)
		chunklen += strlen(mxarray[i].hostname) + 1;
	chunklen += ((mxnb + 1) * sizeof(char *));

	*result = calloc(1, chunklen);
	if (*result == NULL)
		fatal("getmxbyname: calloc");

	ptr = (u_int8_t *)*result + (mxnb + 1) * sizeof(char *);
	for (i = 0; i < mxnb; ++i) {
		strlcpy(ptr, mxarray[i].hostname,
		    strlen(mxarray[i].hostname) + 1);
		(*result)[i] = ptr;
		ptr += strlen(mxarray[i].hostname) + 1;
	}
	(*result)[i] = NULL;

	return mxnb;
}
