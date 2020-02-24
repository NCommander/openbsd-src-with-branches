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

/* $Id: a_1.c,v 1.4 2020/02/24 12:06:14 florian Exp $ */

/* Reviewed: Thu Mar 16 16:52:50 PST 2000 by bwelling */

#ifndef RDATA_IN_1_A_1_C
#define RDATA_IN_1_A_1_C

#include <string.h>

#include <isc/net.h>

#define RRTYPE_A_ATTRIBUTES (0)

static inline isc_result_t
totext_in_a(ARGS_TOTEXT) {
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_a);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length == 4);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &region);
	return (inet_totext(AF_INET, &region, target));
}

static inline isc_result_t
fromwire_in_a(ARGS_FROMWIRE) {
	isc_region_t sregion;
	isc_region_t tregion;

	REQUIRE(type == dns_rdatatype_a);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(options);
	UNUSED(rdclass);

	isc_buffer_activeregion(source, &sregion);
	isc_buffer_availableregion(target, &tregion);
	if (sregion.length < 4)
		return (ISC_R_UNEXPECTEDEND);
	if (tregion.length < 4)
		return (ISC_R_NOSPACE);

	memmove(tregion.base, sregion.base, 4);
	isc_buffer_forward(source, 4);
	isc_buffer_add(target, 4);
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_in_a(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_a);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length == 4);

	UNUSED(cctx);

	isc_buffer_availableregion(target, &region);
	if (region.length < rdata->length)
		return (ISC_R_NOSPACE);
	memmove(region.base, rdata->data, rdata->length);
	isc_buffer_add(target, 4);
	return (ISC_R_SUCCESS);
}


static inline isc_result_t
fromstruct_in_a(ARGS_FROMSTRUCT) {
	dns_rdata_in_a_t *a = source;
	uint32_t n;

	REQUIRE(type == dns_rdatatype_a);
	REQUIRE(rdclass == dns_rdataclass_in);
	REQUIRE(source != NULL);
	REQUIRE(a->common.rdtype == type);
	REQUIRE(a->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	n = ntohl(a->in_addr.s_addr);

	return (uint32_tobuffer(n, target));
}


static inline isc_result_t
tostruct_in_a(ARGS_TOSTRUCT) {
	dns_rdata_in_a_t *a = target;
	uint32_t n;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_a);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length == 4);

	a->common.rdclass = rdata->rdclass;
	a->common.rdtype = rdata->type;
	ISC_LINK_INIT(&a->common, link);

	dns_rdata_toregion(rdata, &region);
	n = uint32_fromregion(&region);
	a->in_addr.s_addr = htonl(n);

	return (ISC_R_SUCCESS);
}

static inline void
freestruct_in_a(ARGS_FREESTRUCT) {
	dns_rdata_in_a_t *a = source;

	REQUIRE(source != NULL);
	REQUIRE(a->common.rdtype == dns_rdatatype_a);
	REQUIRE(a->common.rdclass == dns_rdataclass_in);

	UNUSED(a);
}



#endif	/* RDATA_IN_1_A_1_C */
