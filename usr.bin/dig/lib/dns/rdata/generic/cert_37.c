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

/* $Id: cert_37.c,v 1.6 2020/02/24 17:43:52 florian Exp $ */

/* Reviewed: Wed Mar 15 21:14:32 EST 2000 by tale */

/* RFC2538 */

#ifndef RDATA_GENERIC_CERT_37_C
#define RDATA_GENERIC_CERT_37_C

#define RRTYPE_CERT_ATTRIBUTES (0)

static inline isc_result_t
totext_cert(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("64000 ")];
	unsigned int n;

	REQUIRE(rdata->type == dns_rdatatype_cert);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Type.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	RETERR(dns_cert_totext((dns_cert_t)n, target));
	RETERR(str_totext(" ", target));

	/*
	 * Key tag.
	 */
	n = uint16_fromregion(&sr);
	isc_region_consume(&sr, 2);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(str_totext(buf, target));

	/*
	 * Algorithm.
	 */
	RETERR(dns_secalg_totext(sr.base[0], target));
	isc_region_consume(&sr, 1);

	/*
	 * Cert.
	 */
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" (", target));
	RETERR(str_totext(tctx->linebreak, target));
	if (tctx->width == 0)   /* No splitting */
		RETERR(isc_base64_totext(&sr, 60, "", target));
	else
		RETERR(isc_base64_totext(&sr, tctx->width - 2,
					 tctx->linebreak, target));
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" )", target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_cert(ARGS_FROMWIRE) {
	isc_region_t sr;

	REQUIRE(type == dns_rdatatype_cert);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);
	if (sr.length < 5)
		return (ISC_R_UNEXPECTEDEND);

	isc_buffer_forward(source, sr.length);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_cert(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_cert);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}



static inline isc_result_t
tostruct_cert(ARGS_TOSTRUCT) {
	dns_rdata_cert_t *cert = target;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_cert);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	cert->common.rdclass = rdata->rdclass;
	cert->common.rdtype = rdata->type;
	ISC_LINK_INIT(&cert->common, link);

	dns_rdata_toregion(rdata, &region);

	cert->type = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	cert->key_tag = uint16_fromregion(&region);
	isc_region_consume(&region, 2);
	cert->algorithm = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	cert->length = region.length;

	cert->certificate = mem_maybedup(region.base, region.length);
	if (cert->certificate == NULL)
		return (ISC_R_NOMEMORY);

	return (ISC_R_SUCCESS);
}



#endif	/* RDATA_GENERIC_CERT_37_C */
