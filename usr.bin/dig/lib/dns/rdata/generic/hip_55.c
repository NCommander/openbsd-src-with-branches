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

/* $Id: hip_55.c,v 1.5 2020/02/24 12:06:13 florian Exp $ */

/* reviewed: TBC */

/* RFC 5205 */

#ifndef RDATA_GENERIC_HIP_5_C
#define RDATA_GENERIC_HIP_5_C

#define RRTYPE_HIP_ATTRIBUTES (0)

static inline isc_result_t
totext_hip(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	unsigned int length, key_len, hit_len;
	unsigned char algorithm;
	char buf[sizeof("225 ")];

	REQUIRE(rdata->type == dns_rdatatype_hip);
	REQUIRE(rdata->length != 0);

	dns_rdata_toregion(rdata, &region);

	hit_len = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	algorithm = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	key_len = uint16_fromregion(&region);
	isc_region_consume(&region, 2);

	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext("( ", target));

	/*
	 * Algorithm
	 */
	snprintf(buf, sizeof(buf), "%u ", algorithm);
	RETERR(str_totext(buf, target));

	/*
	 * HIT.
	 */
	INSIST(hit_len < region.length);
	length = region.length;
	region.length = hit_len;
	RETERR(isc_hex_totext(&region, 1, "", target));
	region.length = length - hit_len;
	RETERR(str_totext(tctx->linebreak, target));

	/*
	 * Public KEY.
	 */
	INSIST(key_len <= region.length);
	length = region.length;
	region.length = key_len;
	RETERR(isc_base64_totext(&region, 1, "", target));
	region.length = length - key_len;
	RETERR(str_totext(tctx->linebreak, target));

	/*
	 * Rendezvous Servers.
	 */
	dns_name_init(&name, NULL);
	while (region.length > 0) {
		dns_name_fromregion(&name, &region);

		RETERR(dns_name_totext(&name, ISC_FALSE, target));
		isc_region_consume(&region, name.length);
		if (region.length > 0)
			RETERR(str_totext(tctx->linebreak, target));
	}
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" )", target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_hip(ARGS_FROMWIRE) {
	isc_region_t region, rr;
	dns_name_t name;
	uint8_t hit_len;
	uint16_t key_len;

	REQUIRE(type == dns_rdatatype_hip);

	UNUSED(type);
	UNUSED(rdclass);

	isc_buffer_activeregion(source, &region);
	if (region.length < 4U)
		RETERR(DNS_R_FORMERR);

	rr = region;
	hit_len = uint8_fromregion(&region);
	if (hit_len == 0)
		RETERR(DNS_R_FORMERR);
	isc_region_consume(&region, 2);  	/* hit length + algorithm */
	key_len = uint16_fromregion(&region);
	if (key_len == 0)
		RETERR(DNS_R_FORMERR);
	isc_region_consume(&region, 2);
	if (region.length < (unsigned) (hit_len + key_len))
		RETERR(DNS_R_FORMERR);

	RETERR(mem_tobuffer(target, rr.base, 4 + hit_len + key_len));
	isc_buffer_forward(source, 4 + hit_len + key_len);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);
	while (isc_buffer_activelength(source) > 0) {
		dns_name_init(&name, NULL);
		RETERR(dns_name_fromwire(&name, source, dctx, options, target));
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_hip(ARGS_TOWIRE) {
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_hip);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &region);
	return (mem_tobuffer(target, region.base, region.length));
}


static inline isc_result_t
fromstruct_hip(ARGS_FROMSTRUCT) {
	dns_rdata_hip_t *hip = source;
	dns_rdata_hip_t myhip;
	isc_result_t result;

	REQUIRE(type == dns_rdatatype_hip);
	REQUIRE(source != NULL);
	REQUIRE(hip->common.rdtype == type);
	REQUIRE(hip->common.rdclass == rdclass);
	REQUIRE(hip->hit_len > 0 && hip->hit != NULL);
	REQUIRE(hip->key_len > 0 && hip->key != NULL);
	REQUIRE((hip->servers == NULL && hip->servers_len == 0) ||
		 (hip->servers != NULL && hip->servers_len != 0));

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint8_tobuffer(hip->hit_len, target));
	RETERR(uint8_tobuffer(hip->algorithm, target));
	RETERR(uint16_tobuffer(hip->key_len, target));
	RETERR(mem_tobuffer(target, hip->hit, hip->hit_len));
	RETERR(mem_tobuffer(target, hip->key, hip->key_len));

	myhip = *hip;
	for (result = dns_rdata_hip_first(&myhip);
	     result == ISC_R_SUCCESS;
	     result = dns_rdata_hip_next(&myhip))
		/* empty */;

	return(mem_tobuffer(target, hip->servers, hip->servers_len));
}

static inline isc_result_t
tostruct_hip(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_hip_t *hip = target;

	REQUIRE(rdata->type == dns_rdatatype_hip);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	hip->common.rdclass = rdata->rdclass;
	hip->common.rdtype = rdata->type;
	ISC_LINK_INIT(&hip->common, link);

	dns_rdata_toregion(rdata, &region);

	hip->hit_len = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	hip->algorithm = uint8_fromregion(&region);
	isc_region_consume(&region, 1);

	hip->key_len = uint16_fromregion(&region);
	isc_region_consume(&region, 2);

	hip->hit = hip->key = hip->servers = NULL;

	hip->hit = mem_maybedup(region.base, hip->hit_len);
	if (hip->hit == NULL)
		goto cleanup;
	isc_region_consume(&region, hip->hit_len);

	INSIST(hip->key_len <= region.length);

	hip->key = mem_maybedup(region.base, hip->key_len);
	if (hip->key == NULL)
		goto cleanup;
	isc_region_consume(&region, hip->key_len);

	hip->servers_len = region.length;
	if (hip->servers_len != 0) {
		hip->servers = mem_maybedup(region.base, region.length);
		if (hip->servers == NULL)
			goto cleanup;
	}

	hip->offset = hip->servers_len;
	return (ISC_R_SUCCESS);

 cleanup:
	if (hip->hit != NULL)
		free(hip->hit);
	if (hip->key != NULL)
		free(hip->key);
	if (hip->servers != NULL)
		free(hip->servers);
	return (ISC_R_NOMEMORY);

}

static inline void
freestruct_hip(ARGS_FREESTRUCT) {
	dns_rdata_hip_t *hip = source;

	REQUIRE(source != NULL);


	free(hip->hit);
	free(hip->key);
	free(hip->servers);
}


isc_result_t
dns_rdata_hip_first(dns_rdata_hip_t *hip) {
	if (hip->servers_len == 0)
		return (ISC_R_NOMORE);
	hip->offset = 0;
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_rdata_hip_next(dns_rdata_hip_t *hip) {
	isc_region_t region;
	dns_name_t name;

	if (hip->offset >= hip->servers_len)
		return (ISC_R_NOMORE);

	region.base = hip->servers + hip->offset;
	region.length = hip->servers_len - hip->offset;
	dns_name_init(&name, NULL);
	dns_name_fromregion(&name, &region);
	hip->offset += name.length;
	INSIST(hip->offset <= hip->servers_len);
	return (ISC_R_SUCCESS);
}


#endif	/* RDATA_GENERIC_HIP_5_C */
