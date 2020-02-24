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

/* $Id: dnskey_48.c,v 1.6 2020/02/24 17:43:52 florian Exp $ */

/*
 * Reviewed: Wed Mar 15 16:47:10 PST 2000 by halley.
 */

/* RFC2535 */

#ifndef RDATA_GENERIC_DNSKEY_48_C
#define RDATA_GENERIC_DNSKEY_48_C

#include <dst/dst.h>

#define RRTYPE_DNSKEY_ATTRIBUTES (DNS_RDATATYPEATTR_DNSSEC)

static inline isc_result_t
totext_dnskey(ARGS_TOTEXT) {

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_dnskey);

	return (generic_totext_key(rdata, tctx, target));
}

static inline isc_result_t
fromwire_dnskey(ARGS_FROMWIRE) {

	REQUIRE(type == dns_rdatatype_dnskey);

	return (generic_fromwire_key(rdclass, type, source, dctx,
				     options, target));
}

static inline isc_result_t
towire_dnskey(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_dnskey);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}



static inline isc_result_t
tostruct_dnskey(ARGS_TOSTRUCT) {
	dns_rdata_dnskey_t *dnskey = target;

	REQUIRE(dnskey != NULL);
	REQUIRE(rdata != NULL);
	REQUIRE(rdata->type == dns_rdatatype_dnskey);

	dnskey->common.rdclass = rdata->rdclass;
	dnskey->common.rdtype = rdata->type;
	ISC_LINK_INIT(&dnskey->common, link);

	return (generic_tostruct_key(rdata, target));
}




#endif	/* RDATA_GENERIC_DNSKEY_48_C */
