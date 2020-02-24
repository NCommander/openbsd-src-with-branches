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

/* $Id: mx_15.c,v 1.5 2020/02/24 12:06:51 florian Exp $ */

/* reviewed: Wed Mar 15 18:05:46 PST 2000 by brister */

#ifndef RDATA_GENERIC_MX_15_C
#define RDATA_GENERIC_MX_15_C

#include <string.h>

#include <isc/net.h>

#define RRTYPE_MX_ATTRIBUTES (0)

static inline isc_result_t
totext_mx(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;
	char buf[sizeof("64000")];
	unsigned short num;

	REQUIRE(rdata->type == dns_rdatatype_mx);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof(buf), "%u", num);
	RETERR(str_totext(buf, target));

	RETERR(str_totext(" ", target));

	dns_name_fromregion(&name, &region);
	sub = name_prefix(&name, tctx->origin, &prefix);
	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_mx(ARGS_FROMWIRE) {
	dns_name_t name;
	isc_region_t sregion;

	REQUIRE(type == dns_rdatatype_mx);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, NULL);

	isc_buffer_activeregion(source, &sregion);
	if (sregion.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(mem_tobuffer(target, sregion.base, 2));
	isc_buffer_forward(source, 2);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_mx(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_mx);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);

	dns_rdata_toregion(rdata, &region);
	RETERR(mem_tobuffer(target, region.base, 2));
	isc_region_consume(&region, 2);

	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &region);

	return (dns_name_towire(&name, cctx, target));
}


static inline isc_result_t
fromstruct_mx(ARGS_FROMSTRUCT) {
	dns_rdata_mx_t *mx = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_mx);
	REQUIRE(source != NULL);
	REQUIRE(mx->common.rdtype == type);
	REQUIRE(mx->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint16_tobuffer(mx->pref, target));
	dns_name_toregion(&mx->mx, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_mx(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_mx_t *mx = target;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_mx);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	mx->common.rdclass = rdata->rdclass;
	mx->common.rdtype = rdata->type;
	ISC_LINK_INIT(&mx->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);
	mx->pref = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	dns_name_fromregion(&name, &region);
	dns_name_init(&mx->mx, NULL);
	RETERR(name_duporclone(&name, &mx->mx));
	return (ISC_R_SUCCESS);
}




#endif	/* RDATA_GENERIC_MX_15_C */
