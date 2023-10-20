/*	$OpenBSD: rde_community_test.c,v 1.7 2023/06/17 08:01:22 claudio Exp $ */

/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
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
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rde.h"
#include "log.h"

#include "rde_community_test.h"

struct rde_memstats rdemem;
struct rde_community comm;

static void
dump(uint8_t *b, size_t len)
{
	size_t l;

	printf("\n\t{\n\t\t.data = \"");
	for (l = 0; l < len; l++) {
		printf("\\x%02x", b[l]);
		if (l % 12 == 0 && l != 0)
			printf("\"\n\t\t    \"");
	}
	printf("\",\n\t\t.size = %zu\n\t},\n", len);
}

static int
test_parsing(size_t num, uint8_t *in, size_t inlen)
{
	const char *func = "community";
	struct ibuf *buf;
	uint8_t flags, type, attr[256];
	size_t skip = 2;
	uint16_t attr_len;
	int r;

	communities_clean(&comm);

	flags = in[0];
	type = in[1];
	if (flags & ATTR_EXTLEN) {
		memcpy(&attr_len, in + 2, sizeof(attr_len));
		attr_len = ntohs(attr_len);
		skip += 2;
	} else {
		attr_len = in[2];
		skip += 1;
	}

	switch (type) {
	case ATTR_COMMUNITIES:
		r = community_add(&comm, flags, in + skip, attr_len);
		break;
	case ATTR_EXT_COMMUNITIES:
		r = community_ext_add(&comm, flags, 0, in + skip, attr_len);
		break;
	case ATTR_LARGE_COMMUNITIES:
		r = community_large_add(&comm, flags, in + skip, attr_len);
		break;
	}
	if (r == -1) {
		printf("Test %zu: %s_add failed\n", num, func);
		return -1;
	}

	if ((buf = ibuf_dynamic(0, 4096)) == NULL) {
		printf("Test %zu: ibuf_dynamic failed\n", num);
		return -1;
	}

	if (community_writebuf(&comm, type, 0, buf) == -1) {
		printf("Test %zu: community_writebuf failed\n", num);
		return -1;
	}

	if (ibuf_size(buf) != inlen) {
		printf("Test %zu: %s_write return value %zd != %zd\n",
		    num, func, ibuf_size(buf), inlen);
		return -1;
	}
	if (memcmp(ibuf_data(buf), in, inlen) != 0) {
		printf("Test %zu: %s_write unexpected encoding: ", num, func);
		dump(ibuf_data(buf), ibuf_size(buf));
		printf("expected: ");
		dump(in, inlen);
		return -1;
	}

	return 0;
}

static int
test_filter(size_t num, struct testfilter *f)
{
	size_t l;
	int r;
	struct rde_peer *p = &peer;

	communities_clean(&comm);

	if (f->peer != NULL)
		p = f->peer;

	for (l = 0; f->in[l] != -1; l++) {
		r = community_set(&comm, &filters[f->in[l]], p);
		if (r != 1) {
			printf("Test %zu: community_set %zu "
			    "unexpected return %d != 1\n",
			    num, l, r);
			return -1;
		}
	}

	if (f->match != -1) {
		r = community_match(&comm, &filters[f->match], p);
		if (r != f->mout) {
			printf("Test %zu: community_match "
			    "unexpected return %d != %d\n", num, r, f->mout);
			return -1;
		}
	}

	if (f->delete != -1) {
		community_delete(&comm, &filters[f->delete], p);

		if (community_match(&comm, &filters[f->delete], p) != 0) {
			printf("Test %zu: community_delete still around\n",
			    num);
			return -1;
		}
	}

	if (f->ncomm != 0) {
		if (community_count(&comm, COMMUNITY_TYPE_BASIC) !=
		    f->ncomm - 1) {
			printf("Test %zu: community_count unexpected "
			    "return %d != %d\n", num, r, f->ncomm - 1);
			return -1;
		}
	}

	if (f->next != 0) {
		if (community_count(&comm, COMMUNITY_TYPE_EXT) !=
		    f->next - 1) {
			printf("Test %zu: ext community_count unexpected "
			    "return %d != %d\n", num, r, f->next - 1);
			return -1;
		}
	}

	if (f->nlarge != 0) {
		if (community_count(&comm, COMMUNITY_TYPE_LARGE) !=
		    f->nlarge - 1) {
			printf("Test %zu: large community_count unexpected "
			    "return %d != %d\n", num, r, f->nlarge - 1);
			return -1;
		}
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	size_t t;
	int error = 0;

	for (t = 0; t < sizeof(vectors) / sizeof(*vectors); t++) {
		if (test_parsing(t, vectors[t].data, vectors[t].size) == -1)
			error = 1;
	}

	for (t = 0; t < sizeof(testfilters) / sizeof(*testfilters); t++) {
		if (test_filter(t, &testfilters[t]) == -1)
			error = 1;
	}

	if (!error)
		printf("OK\n");
	return error;
}

__dead void
fatalx(const char *emsg, ...)
{
	va_list ap;
	va_start(ap, emsg);
	verrx(2, emsg, ap);
}

__dead void
fatal(const char *emsg, ...)
{
	va_list ap;
	va_start(ap, emsg);
	verr(2, emsg, ap);
}

void
log_warnx(const char *emsg, ...)
{
	va_list  ap;
	va_start(ap, emsg);
	vwarnx(emsg, ap);
	va_end(ap);
}

int
attr_writebuf(struct ibuf *buf, uint8_t flags, uint8_t type, void *data,
    uint16_t data_len)
{
	u_char  hdr[4];

	flags &= ~ATTR_DEFMASK;
	if (data_len > 255) {
		flags |= ATTR_EXTLEN;
		hdr[2] = (data_len >> 8) & 0xff;
		hdr[3] = data_len & 0xff;
	} else {
		hdr[2] = data_len & 0xff;
	}

	hdr[0] = flags;
	hdr[1] = type;

	if (ibuf_add(buf, hdr, flags & ATTR_EXTLEN ? 4 : 3) == -1)
		return (-1);
	if (data != NULL && ibuf_add(buf, data, data_len) == -1)
		return (-1);
	return (0);

}
