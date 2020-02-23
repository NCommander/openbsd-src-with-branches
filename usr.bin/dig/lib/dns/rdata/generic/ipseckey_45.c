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

/* $Id: ipseckey_45.c,v 1.2 2020/02/20 18:08:51 florian Exp $ */

#ifndef RDATA_GENERIC_IPSECKEY_45_C
#define RDATA_GENERIC_IPSECKEY_45_C

#include <string.h>

#include <isc/net.h>

#define RRTYPE_IPSECKEY_ATTRIBUTES (0)

static inline isc_result_t
totext_ipseckey(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	char buf[sizeof("255 ")];
	unsigned short num;
	unsigned short gateway;

	REQUIRE(rdata->type == dns_rdatatype_ipseckey);
	REQUIRE(rdata->length >= 3);

	dns_name_init(&name, NULL);

	if (rdata->data[1] > 3U)
		return (ISC_R_NOTIMPLEMENTED);

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext("( ", target));

	/*
	 * Precedence.
	 */
	dns_rdata_toregion(rdata, &region);
	num = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	snprintf(buf, sizeof(buf), "%u ", num);
	RETERR(str_totext(buf, target));

	/*
	 * Gateway type.
	 */
	gateway = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	snprintf(buf, sizeof(buf), "%u ", gateway);
	RETERR(str_totext(buf, target));

	/*
	 * Algorithm.
	 */
	num = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	snprintf(buf, sizeof(buf), "%u ", num);
	RETERR(str_totext(buf, target));

	/*
	 * Gateway.
	 */
	switch (gateway) {
	case 0:
		RETERR(str_totext(".", target));
		break;

	case 1:
		RETERR(inet_totext(AF_INET, &region, target));
		isc_region_consume(&region, 4);
		break;

	case 2:
		RETERR(inet_totext(AF_INET6, &region, target));
		isc_region_consume(&region, 16);
		break;

	case 3:
		dns_name_fromregion(&name, &region);
		RETERR(dns_name_totext(&name, ISC_FALSE, target));
		isc_region_consume(&region, name_length(&name));
		break;
	}

	/*
	 * Key.
	 */
	if (region.length > 0U) {
		RETERR(str_totext(tctx->linebreak, target));
		if (tctx->width == 0)   /* No splitting */
			RETERR(isc_base64_totext(&region, 60, "", target));
		else
			RETERR(isc_base64_totext(&region, tctx->width - 2,
						 tctx->linebreak, target));
	}

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" )", target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_ipseckey(ARGS_FROMWIRE) {
	dns_name_t name;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_ipseckey);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	dns_name_init(&name, NULL);

	isc_buffer_activeregion(source, &region);
	if (region.length < 3)
		return (ISC_R_UNEXPECTEDEND);

	switch (region.base[1]) {
	case 0:
		isc_buffer_forward(source, region.length);
		return (mem_tobuffer(target, region.base, region.length));

	case 1:
		if (region.length < 7)
			return (ISC_R_UNEXPECTEDEND);
		isc_buffer_forward(source, region.length);
		return (mem_tobuffer(target, region.base, region.length));

	case 2:
		if (region.length < 19)
			return (ISC_R_UNEXPECTEDEND);
		isc_buffer_forward(source, region.length);
		return (mem_tobuffer(target, region.base, region.length));

	case 3:
		RETERR(mem_tobuffer(target, region.base, 3));
		isc_buffer_forward(source, 3);
		RETERR(dns_name_fromwire(&name, source, dctx, options, target));
		isc_buffer_activeregion(source, &region);
		isc_buffer_forward(source, region.length);
		return(mem_tobuffer(target, region.base, region.length));

	default:
		return (ISC_R_NOTIMPLEMENTED);
	}
}

static inline isc_result_t
towire_ipseckey(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_ipseckey);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &region);
	return (mem_tobuffer(target, region.base, region.length));
}

static inline int
compare_ipseckey(ARGS_COMPARE) {
	isc_region_t region1;
	isc_region_t region2;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_ipseckey);
	REQUIRE(rdata1->length >= 3);
	REQUIRE(rdata2->length >= 3);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);

	return (isc_region_compare(&region1, &region2));
}

static inline isc_result_t
fromstruct_ipseckey(ARGS_FROMSTRUCT) {
	dns_rdata_ipseckey_t *ipseckey = source;
	isc_region_t region;
	uint32_t n;

	REQUIRE(type == dns_rdatatype_ipseckey);
	REQUIRE(source != NULL);
	REQUIRE(ipseckey->common.rdtype == type);
	REQUIRE(ipseckey->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	if (ipseckey->gateway_type > 3U)
		return (ISC_R_NOTIMPLEMENTED);

	RETERR(uint8_tobuffer(ipseckey->precedence, target));
	RETERR(uint8_tobuffer(ipseckey->gateway_type, target));
	RETERR(uint8_tobuffer(ipseckey->algorithm, target));

	switch  (ipseckey->gateway_type) {
	case 0:
		break;

	case 1:
		n = ntohl(ipseckey->in_addr.s_addr);
		RETERR(uint32_tobuffer(n, target));
		break;

	case 2:
		RETERR(mem_tobuffer(target, ipseckey->in6_addr.s6_addr, 16));
		break;

	case 3:
		dns_name_toregion(&ipseckey->gateway, &region);
		RETERR(isc_buffer_copyregion(target, &region));
		break;
	}

	return (mem_tobuffer(target, ipseckey->key, ipseckey->keylength));
}

static inline isc_result_t
tostruct_ipseckey(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_ipseckey_t *ipseckey = target;
	dns_name_t name;
	uint32_t n;

	REQUIRE(rdata->type == dns_rdatatype_ipseckey);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length >= 3);

	if (rdata->data[1] > 3U)
		return (ISC_R_NOTIMPLEMENTED);

	ipseckey->common.rdclass = rdata->rdclass;
	ipseckey->common.rdtype = rdata->type;
	ISC_LINK_INIT(&ipseckey->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);

	ipseckey->precedence = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	ipseckey->gateway_type = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	ipseckey->algorithm = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	switch (ipseckey->gateway_type) {
	case 0:
		break;

	case 1:
		n = uint32_fromregion(&region);
		ipseckey->in_addr.s_addr = htonl(n);
		isc_region_consume(&region, 4);
		break;

	case 2:
		memmove(ipseckey->in6_addr.s6_addr, region.base, 16);
		isc_region_consume(&region, 16);
		break;

	case 3:
		dns_name_init(&ipseckey->gateway, NULL);
		dns_name_fromregion(&name, &region);
		RETERR(name_duporclone(&name, &ipseckey->gateway));
		isc_region_consume(&region, name_length(&name));
		break;
	}

	ipseckey->keylength = region.length;
	if (ipseckey->keylength != 0U) {
		ipseckey->key = mem_maybedup(region.base,
					     ipseckey->keylength);
		if (ipseckey->key == NULL) {
			if (ipseckey->gateway_type == 3)
				dns_name_free(&ipseckey->gateway);
			return (ISC_R_NOMEMORY);
		}
	} else
		ipseckey->key = NULL;

	return (ISC_R_SUCCESS);
}

static inline void
freestruct_ipseckey(ARGS_FREESTRUCT) {
	dns_rdata_ipseckey_t *ipseckey = source;

	REQUIRE(source != NULL);
	REQUIRE(ipseckey->common.rdtype == dns_rdatatype_ipseckey);

	if (ipseckey->gateway_type == 3)
		dns_name_free(&ipseckey->gateway);

	if (ipseckey->key != NULL)
		free(ipseckey->key);

}

static inline isc_boolean_t
checkowner_ipseckey(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_ipseckey);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}

static inline int
casecompare_ipseckey(ARGS_COMPARE) {
	isc_region_t region1;
	isc_region_t region2;
	dns_name_t name1;
	dns_name_t name2;
	int order;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == dns_rdatatype_ipseckey);
	REQUIRE(rdata1->length >= 3);
	REQUIRE(rdata2->length >= 3);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);

	if (memcmp(region1.base, region2.base, 3) != 0 || region1.base[1] != 3)
		return (isc_region_compare(&region1, &region2));

	dns_name_init(&name1, NULL);
	dns_name_init(&name2, NULL);

	isc_region_consume(&region1, 3);
	isc_region_consume(&region2, 3);

	dns_name_fromregion(&name1, &region1);
	dns_name_fromregion(&name2, &region2);

	order = dns_name_rdatacompare(&name1, &name2);
	if (order != 0)
		return (order);

	isc_region_consume(&region1, name_length(&name1));
	isc_region_consume(&region2, name_length(&name2));

	return (isc_region_compare(&region1, &region2));
}

#endif	/* RDATA_GENERIC_IPSECKEY_45_C */
