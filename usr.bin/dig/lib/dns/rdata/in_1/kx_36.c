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

/* $Id: kx_36.c,v 1.3 2020/02/23 19:54:26 jung Exp $ */

/* Reviewed: Thu Mar 16 17:24:54 PST 2000 by explorer */

/* RFC2230 */

#ifndef RDATA_IN_1_KX_36_C
#define RDATA_IN_1_KX_36_C

#define RRTYPE_KX_ATTRIBUTES (0)

static inline isc_result_t
totext_in_kx(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;
	char buf[sizeof("64000")];
	unsigned short num;

	REQUIRE(rdata->type == dns_rdatatype_kx);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
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
fromwire_in_kx(ARGS_FROMWIRE) {
	dns_name_t name;
	isc_region_t sregion;

	REQUIRE(type == dns_rdatatype_kx);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	dns_name_init(&name, NULL);

	isc_buffer_activeregion(source, &sregion);
	if (sregion.length < 2)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(mem_tobuffer(target, sregion.base, 2));
	isc_buffer_forward(source, 2);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_in_kx(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_kx);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);
	dns_rdata_toregion(rdata, &region);
	RETERR(mem_tobuffer(target, region.base, 2));
	isc_region_consume(&region, 2);

	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &region);

	return (dns_name_towire(&name, cctx, target));
}


static inline isc_result_t
fromstruct_in_kx(ARGS_FROMSTRUCT) {
	dns_rdata_in_kx_t *kx = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_kx);
	REQUIRE(rdclass == dns_rdataclass_in);
	REQUIRE(source != NULL);
	REQUIRE(kx->common.rdtype == type);
	REQUIRE(kx->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint16_tobuffer(kx->preference, target));
	dns_name_toregion(&kx->exchange, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_in_kx(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_in_kx_t *kx = target;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_kx);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	kx->common.rdclass = rdata->rdclass;
	kx->common.rdtype = rdata->type;
	ISC_LINK_INIT(&kx->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);

	kx->preference = uint16_fromregion(&region);
	isc_region_consume(&region, 2);

	dns_name_fromregion(&name, &region);
	dns_name_init(&kx->exchange, NULL);
	RETERR(name_duporclone(&name, &kx->exchange));
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_in_kx(ARGS_FREESTRUCT) {
	dns_rdata_in_kx_t *kx = source;

	REQUIRE(source != NULL);
	REQUIRE(kx->common.rdclass == dns_rdataclass_in);
	REQUIRE(kx->common.rdtype == dns_rdatatype_kx);

	dns_name_free(&kx->exchange);
}

static inline isc_boolean_t
checkowner_in_kx(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_kx);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}


#endif	/* RDATA_IN_1_KX_36_C */
