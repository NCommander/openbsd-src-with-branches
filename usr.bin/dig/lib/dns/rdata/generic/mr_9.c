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

/* $Id: mr_9.c,v 1.5 2020/02/24 12:06:51 florian Exp $ */

/* Reviewed: Wed Mar 15 21:30:35 EST 2000 by tale */

#ifndef RDATA_GENERIC_MR_9_C
#define RDATA_GENERIC_MR_9_C

#define RRTYPE_MR_ATTRIBUTES (0)

static inline isc_result_t
totext_mr(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == dns_rdatatype_mr);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	sub = name_prefix(&name, tctx->origin, &prefix);

	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_mr(ARGS_FROMWIRE) {
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_mr);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, NULL);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_mr(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_mr);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	return (dns_name_towire(&name, cctx, target));
}


static inline isc_result_t
fromstruct_mr(ARGS_FROMSTRUCT) {
	dns_rdata_mr_t *mr = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_mr);
	REQUIRE(source != NULL);
	REQUIRE(mr->common.rdtype == type);
	REQUIRE(mr->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	dns_name_toregion(&mr->mr, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_mr(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_mr_t *mr = target;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_mr);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	mr->common.rdclass = rdata->rdclass;
	mr->common.rdtype = rdata->type;
	ISC_LINK_INIT(&mr->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);
	dns_name_init(&mr->mr, NULL);
	RETERR(name_duporclone(&name, &mr->mr));
	return (ISC_R_SUCCESS);
}




#endif	/* RDATA_GENERIC_MR_9_C */
