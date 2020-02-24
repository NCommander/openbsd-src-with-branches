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

/* $Id: nxt_30.c,v 1.6 2020/02/24 17:43:52 florian Exp $ */

/* reviewed: Wed Mar 15 18:21:15 PST 2000 by brister */

/* RFC2535 */

#ifndef RDATA_GENERIC_NXT_30_C
#define RDATA_GENERIC_NXT_30_C

/*
 * The attributes do not include DNS_RDATATYPEATTR_SINGLETON
 * because we must be able to handle a parent/child NXT pair.
 */
#define RRTYPE_NXT_ATTRIBUTES (0)

static inline isc_result_t
totext_nxt(ARGS_TOTEXT) {
	isc_region_t sr;
	unsigned int i, j;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == dns_rdatatype_nxt);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);
	dns_rdata_toregion(rdata, &sr);
	dns_name_fromregion(&name, &sr);
	isc_region_consume(&sr, name_length(&name));
	sub = name_prefix(&name, tctx->origin, &prefix);
	RETERR(dns_name_totext(&prefix, sub, target));

	for (i = 0; i < sr.length; i++) {
		if (sr.base[i] != 0)
			for (j = 0; j < 8; j++)
				if ((sr.base[i] & (0x80 >> j)) != 0) {
					dns_rdatatype_t t = i * 8 + j;
					RETERR(str_totext(" ", target));
					if (dns_rdatatype_isknown(t)) {
						RETERR(dns_rdatatype_totext(t,
								      target));
					} else {
						char buf[sizeof("65535")];
						snprintf(buf, sizeof(buf),
							 "%u", t);
						RETERR(str_totext(buf,
								  target));
					}
				}
	}
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_nxt(ARGS_FROMWIRE) {
	isc_region_t sr;
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_nxt);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_NONE);

	dns_name_init(&name, NULL);
	RETERR(dns_name_fromwire(&name, source, dctx, options, target));

	isc_buffer_activeregion(source, &sr);
	if (sr.length > 0 && (sr.base[0] & 0x80) == 0 &&
	    ((sr.length > 16) || sr.base[sr.length - 1] == 0))
		return (DNS_R_BADBITMAP);
	RETERR(mem_tobuffer(target, sr.base, sr.length));
	isc_buffer_forward(source, sr.length);
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_nxt(ARGS_TOWIRE) {
	isc_region_t sr;
	dns_name_t name;
	dns_offsets_t offsets;

	REQUIRE(rdata->type == dns_rdatatype_nxt);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_NONE);
	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &sr);
	dns_name_fromregion(&name, &sr);
	isc_region_consume(&sr, name_length(&name));
	RETERR(dns_name_towire(&name, cctx, target));

	return (mem_tobuffer(target, sr.base, sr.length));
}



static inline isc_result_t
tostruct_nxt(ARGS_TOSTRUCT) {
	isc_region_t region;
	dns_rdata_nxt_t *nxt = target;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_nxt);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	nxt->common.rdclass = rdata->rdclass;
	nxt->common.rdtype = rdata->type;
	ISC_LINK_INIT(&nxt->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);
	isc_region_consume(&region, name_length(&name));
	dns_name_init(&nxt->next, NULL);
	RETERR(name_duporclone(&name, &nxt->next));

	nxt->len = region.length;
	nxt->typebits = mem_maybedup(region.base, region.length);
	if (nxt->typebits == NULL)
		goto cleanup;

	return (ISC_R_SUCCESS);

 cleanup:
	dns_name_free(&nxt->next);
	return (ISC_R_NOMEMORY);
}



#endif	/* RDATA_GENERIC_NXT_30_C */
