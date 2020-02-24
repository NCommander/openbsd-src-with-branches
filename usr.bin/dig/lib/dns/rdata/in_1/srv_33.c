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

/* $Id: srv_33.c,v 1.3 2020/02/23 19:54:26 jung Exp $ */

/* Reviewed: Fri Mar 17 13:01:00 PST 2000 by bwelling */

/* RFC2782 */

#ifndef RDATA_IN_1_SRV_33_C
#define RDATA_IN_1_SRV_33_C

#define RRTYPE_SRV_ATTRIBUTES (0)

static inline isc_result_t
totext_in_srv(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;
	char buf[sizeof("64000")];
	unsigned short num;

	REQUIRE(rdata->type == dns_rdatatype_srv);
	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	/*
	 * Priority.
	 */
	dns_rdata_toregion(rdata, &region);
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof buf, "%u", num);
	RETERR(str_totext(buf, target));
	RETERR(str_totext(" ", target));

	/*
	 * Weight.
	 */
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof buf, "%u", num);
	RETERR(str_totext(buf, target));
	RETERR(str_totext(" ", target));

	/*
	 * Port.
	 */
	num = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	snprintf(buf, sizeof buf, "%u", num);
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

	REQUIRE(type == dns_rdatatype_srv);
	REQUIRE(rdclass == dns_rdataclass_in);

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
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_in_srv(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_srv);
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


static inline isc_result_t
fromstruct_in_srv(ARGS_FROMSTRUCT) {
	dns_rdata_in_srv_t *srv = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_srv);
	REQUIRE(rdclass == dns_rdataclass_in);
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

	REQUIRE(rdata->rdclass == dns_rdataclass_in);
	REQUIRE(rdata->type == dns_rdatatype_srv);
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
	RETERR(name_duporclone(&name, &srv->target));
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_in_srv(ARGS_FREESTRUCT) {
	dns_rdata_in_srv_t *srv = source;

	REQUIRE(source != NULL);
	REQUIRE(srv->common.rdclass == dns_rdataclass_in);
	REQUIRE(srv->common.rdtype == dns_rdatatype_srv);

	dns_name_free(&srv->target);
}

static inline isc_boolean_t
checkowner_in_srv(ARGS_CHECKOWNER) {

	REQUIRE(type == dns_rdatatype_srv);
	REQUIRE(rdclass == dns_rdataclass_in);

	UNUSED(name);
	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(wildcard);

	return (ISC_TRUE);
}


#endif	/* RDATA_IN_1_SRV_33_C */
