/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: null_10.c,v 1.5 2020/02/24 12:06:51 florian Exp $ */

/* Reviewed: Thu Mar 16 13:57:50 PST 2000 by explorer */

#ifndef RDATA_GENERIC_NULL_10_C
#define RDATA_GENERIC_NULL_10_C

#define RRTYPE_NULL_ATTRIBUTES (0)

static inline isc_result_t
totext_null(ARGS_TOTEXT) {
	REQUIRE(rdata->type == dns_rdatatype_null);

	return (unknown_totext(rdata, tctx, target));
}

static inline isc_result_t
fromwire_null(ARGS_FROMWIRE) {
	isc_region_t sr;

	REQUIRE(type == dns_rdatatype_null);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);
	isc_buffer_forward(source, sr.length);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_null(ARGS_TOWIRE) {
	REQUIRE(rdata->type == dns_rdatatype_null);

	UNUSED(cctx);

	return (mem_tobuffer(target, rdata->data, rdata->length));
}


static inline isc_result_t
fromstruct_null(ARGS_FROMSTRUCT) {
	dns_rdata_null_t *null = source;

	REQUIRE(type == dns_rdatatype_null);
	REQUIRE(source != NULL);
	REQUIRE(null->common.rdtype == type);
	REQUIRE(null->common.rdclass == rdclass);
	REQUIRE(null->data != NULL || null->length == 0);

	UNUSED(type);
	UNUSED(rdclass);

	return (mem_tobuffer(target, null->data, null->length));
}

static inline isc_result_t
tostruct_null(ARGS_TOSTRUCT) {
	dns_rdata_null_t *null = target;
	isc_region_t r;

	REQUIRE(rdata->type == dns_rdatatype_null);
	REQUIRE(target != NULL);

	null->common.rdclass = rdata->rdclass;
	null->common.rdtype = rdata->type;
	ISC_LINK_INIT(&null->common, link);

	dns_rdata_toregion(rdata, &r);
	null->length = r.length;
	null->data = mem_maybedup(r.base, r.length);
	if (null->data == NULL)
		return (ISC_R_NOMEMORY);

	return (ISC_R_SUCCESS);
}




#endif	/* RDATA_GENERIC_NULL_10_C */
