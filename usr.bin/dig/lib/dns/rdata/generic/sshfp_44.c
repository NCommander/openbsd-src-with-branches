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

/* $Id: sshfp_44.c,v 1.5 2020/02/24 12:06:51 florian Exp $ */

/* RFC 4255 */

#ifndef RDATA_GENERIC_SSHFP_44_C
#define RDATA_GENERIC_SSHFP_44_C

#define RRTYPE_SSHFP_ATTRIBUTES (0)

static inline isc_result_t
totext_sshfp(ARGS_TOTEXT) {
	isc_region_t sr;
	char buf[sizeof("64000 ")];
	unsigned int n;

	REQUIRE(rdata->type == dns_rdatatype_sshfp);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &sr);

	/*
	 * Algorithm.
	 */
	n = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	snprintf(buf, sizeof(buf), "%u ", n);
	RETERR(str_totext(buf, target));

	/*
	 * Digest type.
	 */
	n = uint8_fromregion(&sr);
	isc_region_consume(&sr, 1);
	snprintf(buf, sizeof(buf), "%u", n);
	RETERR(str_totext(buf, target));

	/*
	 * Digest.
	 */
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" (", target));
	RETERR(str_totext(tctx->linebreak, target));
	if (tctx->width == 0) /* No splitting */
		RETERR(isc_hex_totext(&sr, 0, "", target));
	else
		RETERR(isc_hex_totext(&sr, tctx->width - 2,
				      tctx->linebreak, target));
	if ((tctx->flags & DNS_STYLEFLAG_MULTILINE) != 0)
		RETERR(str_totext(" )", target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_sshfp(ARGS_FROMWIRE) {
	isc_region_t sr;

	REQUIRE(type == dns_rdatatype_sshfp);

	UNUSED(type);
	UNUSED(rdclass);
	UNUSED(dctx);
	UNUSED(options);

	isc_buffer_activeregion(source, &sr);
	if (sr.length < 4)
		return (ISC_R_UNEXPECTEDEND);

	isc_buffer_forward(source, sr.length);
	return (mem_tobuffer(target, sr.base, sr.length));
}

static inline isc_result_t
towire_sshfp(ARGS_TOWIRE) {
	isc_region_t sr;

	REQUIRE(rdata->type == dns_rdatatype_sshfp);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	dns_rdata_toregion(rdata, &sr);
	return (mem_tobuffer(target, sr.base, sr.length));
}


static inline isc_result_t
fromstruct_sshfp(ARGS_FROMSTRUCT) {
	dns_rdata_sshfp_t *sshfp = source;

	REQUIRE(type == dns_rdatatype_sshfp);
	REQUIRE(source != NULL);
	REQUIRE(sshfp->common.rdtype == type);
	REQUIRE(sshfp->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint8_tobuffer(sshfp->algorithm, target));
	RETERR(uint8_tobuffer(sshfp->digest_type, target));

	return (mem_tobuffer(target, sshfp->digest, sshfp->length));
}

static inline isc_result_t
tostruct_sshfp(ARGS_TOSTRUCT) {
	dns_rdata_sshfp_t *sshfp = target;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_sshfp);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	sshfp->common.rdclass = rdata->rdclass;
	sshfp->common.rdtype = rdata->type;
	ISC_LINK_INIT(&sshfp->common, link);

	dns_rdata_toregion(rdata, &region);

	sshfp->algorithm = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	sshfp->digest_type = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	sshfp->length = region.length;

	sshfp->digest = mem_maybedup(region.base, region.length);
	if (sshfp->digest == NULL)
		return (ISC_R_NOMEMORY);

	return (ISC_R_SUCCESS);
}




#endif	/* RDATA_GENERIC_SSHFP_44_C */
