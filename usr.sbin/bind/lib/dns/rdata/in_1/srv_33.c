/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: srv_33.c,v 1.36 2001/07/16 03:06:49 marka Exp $ */

/* Reviewed: Fri Mar 17 13:01:00 PST 2000 by bwelling */

/* RFC 2782 */

#ifndef RDATA_IN_1_SRV_33_C
#define RDATA_IN_1_SRV_33_C

#define RRTYPE_SRV_ATTRIBUTES (0)

static inline isc_result_t
fromtext_in_srv(ARGS_FROMTEXT) {
	isc_token_t token;
	dns_name_t name;
	isc_buffer_t buffer;

	REQUIRE(type == 33);
	REQUIRE(rdclass == 1);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(callbacks);

	/*
	 * Priority.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffff)
		RETTOK(ISC_R_RANGE);
	RETERR(uint16_tobuffer(token.value.as_ulong, target));

	/*
	 * Weight.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffff)
		RETTOK(ISC_R_RANGE);
	RETERR(uint16_tobuffer(token.value.as_ulong, target));

	/*
	 * Port.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_number,
				      ISC_FALSE));
	if (token.value.as_ulong > 0xffff)
		RETTOK(ISC_R_RANGE);
	RETERR(uint16_tobuffer(token.value.as_ulong, target));

	/*
	 * Target.
	 */
	RETERR(isc_lex_getmastertoken(lexer, &token, isc_tokentype_string,
				      ISC_FALSE));
	dns_name_init(&name, NULL);
	buffer_fromregion(&buffer, &token.value.as_region);
	origin = (origin != NULL) ? origin : dns_rootname;
	RETTOK(dns_name_fromtext(&name, &buffer, origin, downcase, target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
totext_in_srv(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;
	char buf[sizeof "64000"];
	unsigned short num;

	REQUIRE(rdata->type == 33);
	REQUIRE(rdata->rdclass == 1);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	/*
	 * Priority.
	 */
	dns_rdata_toregion(rdata, &region);
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof(buf), "%u", num);
	RETERR(str_totext(buf, target));
	RETERR(str_totext(" ", target));

	/*
	 * Weight.
	 */
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof(buf), "%u", num);
	RETERR(str_totext(buf, target));
	RETERR(str_totext(" ", target));

	/*
	 * Port.
	 */
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof(buf), "%u", num);
	RETERR(str_totext(buf, target));
	RETERR(str_totext(" ", target));

	/*
	 * Target.
	 */
	dns_name_fromregion(&name, &region);
	sub = name_prefix(&name, tctx->origin, &prefix);
	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_in_srv(ARGS_FROMWIRE) {
        dns_name_t name;
	isc_region_t sr;

	REQUIRE(type == 33);
	REQUIRE(rdclass == 1);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

        dns_name_init(&name, NULL);

	/*
	 * Priority, weight, port.
	 */
	isc_buffer_activeregion(source, &sr);
	if (sr.length < 6)
		return (ISC_R_UNEXPECTEDEND);
	RETERR(mem_tobuffer(target, sr.base, 6));
	isc_buffer_forward(source, 6);

	/*
	 * Target.
	 */
	return (dns_name_fromwire(&name, source, dctx, downcase, target));
}

static inline isc_result_t
towire_in_srv(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t sr;

	REQUIRE(rdata->type == 33);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);
	/*
	 * Priority, weight, port.
	 */
	dns_rdata_toregion(rdata, &sr);
	RETERR(mem_tobuffer(target, sr.base, 6));
	isc_region_consume(&sr, 6);

	/*
	 * Target.
	 */
	dns_name_init(&name, offsets);
	dns_name_fromregion(&name, &sr);
	return (dns_name_towire(&name, cctx, target));
}

static inline int
compare_in_srv(ARGS_COMPARE) {
	dns_name_t name1;
	dns_name_t name2;
	isc_region_t region1;
	isc_region_t region2;
	int order;

	REQUIRE(rdata1->type == rdata2->type);
	REQUIRE(rdata1->rdclass == rdata2->rdclass);
	REQUIRE(rdata1->type == 33);
	REQUIRE(rdata1->rdclass == 1);
	REQUIRE(rdata1->length != 0);
	REQUIRE(rdata2->length != 0);

	/*
	 * Priority, weight, port.
	 */
	order = memcmp(rdata1->data, rdata2->data, 6);
	if (order != 0)
		return (order < 0 ? -1 : 1);

	/*
	 * Target.
	 */
	dns_name_init(&name1, NULL);
	dns_name_init(&name2, NULL);

	dns_rdata_toregion(rdata1, &region1);
	dns_rdata_toregion(rdata2, &region2);

	isc_region_consume(&region1, 6);
	isc_region_consume(&region2, 6);

	dns_name_fromregion(&name1, &region1);
	dns_name_fromregion(&name2, &region2);

	return (dns_name_rdatacompare(&name1, &name2));
}

static inline isc_result_t
fromstruct_in_srv(ARGS_FROMSTRUCT) {
	dns_rdata_in_srv_t *srv = source;
	isc_region_t region;

	REQUIRE(type == 33);
	REQUIRE(rdclass == 1);
	REQUIRE(source != NULL);
	REQUIRE(srv->common.rdtype == type);
	REQUIRE(srv->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint16_tobuffer(srv->priority, target));
	RETERR(uint16_tobuffer(srv->weight, target));
	RETERR(uint16_tobuffer(srv->port, target));
	dns_name_toregion(&srv->target, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_in_srv(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_in_srv_t *srv = target;
	dns_name_t name;

	REQUIRE(rdata->rdclass == 1);
	REQUIRE(rdata->type == 33);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	srv->common.rdclass = rdata->rdclass;
	srv->common.rdtype = rdata->type;
	ISC_LINK_INIT(&srv->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);
	srv->priority = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	srv->weight = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	srv->port = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	dns_name_fromregion(&name, &region);
	dns_name_init(&srv->target, NULL);
	RETERR(name_duporclone(&name, mctx, &srv->target));
	srv->mctx = mctx;
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_in_srv(ARGS_FREESTRUCT) {
	dns_rdata_in_srv_t *srv = source;

	REQUIRE(source != NULL);
	REQUIRE(srv->common.rdclass == 1);
	REQUIRE(srv->common.rdtype == 33);

	if (srv->mctx == NULL)
		return;

	dns_name_free(&srv->target, srv->mctx);
	srv->mctx = NULL;
}

static inline isc_result_t
additionaldata_in_srv(ARGS_ADDLDATA) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == 33);
	REQUIRE(rdata->rdclass == 1);

	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &region);
	isc_region_consume(&region, 6);
	dns_name_fromregion(&name, &region);

	return ((add)(arg, &name, dns_rdatatype_a));
}

static inline isc_result_t
digest_in_srv(ARGS_DIGEST) {
	isc_region_t r1, r2;
	dns_name_t name;

	REQUIRE(rdata->type == 33);
	REQUIRE(rdata->rdclass == 1);

	dns_rdata_toregion(rdata, &r1);
	r2 = r1;
	isc_region_consume(&r2, 6);
	r1.length = 6;
	RETERR((digest)(arg, &r1));
	dns_name_init(&name, NULL);
	dns_name_fromregion(&name, &r2);
	return (dns_name_digest(&name, digest, arg));
}

#endif	/* RDATA_IN_1_SRV_33_C */
