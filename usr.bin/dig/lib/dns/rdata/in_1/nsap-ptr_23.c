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

/* $Id: nsap-ptr_23.c,v 1.5 2020/02/24 12:06:51 florian Exp $ */

/* Reviewed: Fri Mar 17 10:16:02 PST 2000 by gson */

/* RFC1348.  Obsoleted in RFC 1706 - use PTR instead. */

#ifndef RDATA_IN_1_NSAP_PTR_23_C
#define RDATA_IN_1_NSAP_PTR_23_C

#define RRTYPE_NSAP_PTR_ATTRIBUTES (0)

static inline isc_result_t
totext_in_nsap_ptr(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == dns_rdatatype_nsap_ptr);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	sub = name_prefix(&name, tctx->origin, &prefix);

	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_in_nsap_ptr(ARGS_FROMWIRE) {
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_nsap_ptr);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	dns_name_init(&name, NULL);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_in_nsap_ptr(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_nsap_ptr);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);
	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	return (dns_name_towire(&name, cctx, target));
}


static inline isc_result_t
fromstruct_in_nsap_ptr(ARGS_FROMSTRUCT) {
	dns_rdata_in_nsap_ptr_t *nsap_ptr = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_nsap_ptr);
	REQUIRE(rdclass == dns_rdataclass_in);
	REQUIRE(source != NULL);
	REQUIRE(nsap_ptr->common.rdtype == type);
	REQUIRE(nsap_ptr->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	dns_name_toregion(&nsap_ptr->owner, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_in_nsap_ptr(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_in_nsap_ptr_t *nsap_ptr = target;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_nsap_ptr);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	nsap_ptr->common.rdclass = rdata->rdclass;
	nsap_ptr->common.rdtype = rdata->type;
	ISC_LINK_INIT(&nsap_ptr->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);
	dns_name_init(&nsap_ptr->owner, NULL);
	RETERR(name_duporclone(&name, &nsap_ptr->owner));
	return (ISC_R_SUCCESS);
}




#endif	/* RDATA_IN_1_NSAP_PTR_23_C */
