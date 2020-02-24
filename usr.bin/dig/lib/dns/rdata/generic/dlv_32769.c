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

/* $Id: dlv_32769.c,v 1.4 2020/02/24 12:06:13 florian Exp $ */

/* RFC3658 */

#ifndef RDATA_GENERIC_DLV_32769_C
#define RDATA_GENERIC_DLV_32769_C

#define RRTYPE_DLV_ATTRIBUTES 0

#include <isc/sha1.h>
#include <isc/sha2.h>

#include <dns/ds.h>

static inline isc_result_t
totext_dlv(ARGS_TOTEXT) {

	REQUIRE(rdata->type == dns_rdatatype_dlv);

	return (generic_totext_ds(rdata, tctx, target));
}

static inline isc_result_t
fromwire_dlv(ARGS_FROMWIRE) {

	REQUIRE(type == dns_rdatatype_dlv);

	return (generic_fromwire_ds(rdclass, type, source, dctx, options,
				    target));
}

static inline isc_result_t
towire_dlv(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_dlv);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}


static inline isc_result_t
fromstruct_dlv(ARGS_FROMSTRUCT) {

	REQUIRE(type == dns_rdatatype_dlv);

	return (generic_fromstruct_ds(rdclass, type, source, target));
}

static inline isc_result_t
tostruct_dlv(ARGS_TOSTRUCT) {
	dns_rdata_dlv_t *dlv = target;

	REQUIRE(rdata->type == dns_rdatatype_dlv);

	dlv->common.rdclass = rdata->rdclass;
	dlv->common.rdtype = rdata->type;
	ISC_LINK_INIT(&dlv->common, link);

	return (generic_tostruct_ds(rdata, target));
}

static inline void
freestruct_dlv(ARGS_FREESTRUCT) {
	dns_rdata_dlv_t *dlv = source;

	REQUIRE(dlv != NULL);
	REQUIRE(dlv->common.rdtype == dns_rdatatype_dlv);

	free(dlv->digest);
}



#endif	/* RDATA_GENERIC_DLV_32769_C */
