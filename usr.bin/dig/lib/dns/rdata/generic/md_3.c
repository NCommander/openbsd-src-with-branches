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

/* $Id: md_3.c,v 1.4 2020/02/24 12:06:13 florian Exp $ */

/* Reviewed: Wed Mar 15 17:48:20 PST 2000 by bwelling */

#ifndef RDATA_GENERIC_MD_3_C
#define RDATA_GENERIC_MD_3_C

#define RRTYPE_MD_ATTRIBUTES (0)

static inline isc_result_t
totext_md(ARGS_TOTEXT) {
	isc_region_t region;
	dns_name_t name;
	dns_name_t prefix;
	isc_boolean_t sub;

	REQUIRE(rdata->type == dns_rdatatype_md);
	REQUIRE(rdata->length != 0);

	dns_name_init(&name, NULL);
	dns_name_init(&prefix, NULL);

	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	sub = name_prefix(&name, tctx->origin, &prefix);

	return (dns_name_totext(&prefix, sub, target));
}

static inline isc_result_t
fromwire_md(ARGS_FROMWIRE) {
	dns_name_t name;

	REQUIRE(type == dns_rdatatype_md);

	UNUSED(type);
	UNUSED(rdclass);

	dns_decompress_setmethods(dctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, NULL);
	return (dns_name_fromwire(&name, source, dctx, options, target));
}

static inline isc_result_t
towire_md(ARGS_TOWIRE) {
	dns_name_t name;
	dns_offsets_t offsets;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_md);
	REQUIRE(rdata->length != 0);

	dns_compress_setmethods(cctx, DNS_COMPRESS_GLOBAL14);

	dns_name_init(&name, offsets);
	dns_rdata_toregion(rdata, &region);
	dns_name_fromregion(&name, &region);

	return (dns_name_towire(&name, cctx, target));
}


static inline isc_result_t
fromstruct_md(ARGS_FROMSTRUCT) {
	dns_rdata_md_t *md = source;
	isc_region_t region;

	REQUIRE(type == dns_rdatatype_md);
	REQUIRE(source != NULL);
	REQUIRE(md->common.rdtype == type);
	REQUIRE(md->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	dns_name_toregion(&md->md, &region);
	return (isc_buffer_copyregion(target, &region));
}

static inline isc_result_t
tostruct_md(ARGS_TOSTRUCT) {
	dns_rdata_md_t *md = target;
	isc_region_t r;
	dns_name_t name;

	REQUIRE(rdata->type == dns_rdatatype_md);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	md->common.rdclass = rdata->rdclass;
	md->common.rdtype = rdata->type;
	ISC_LINK_INIT(&md->common, link);

	dns_name_init(&name, NULL);
	dns_rdata_toregion(rdata, &r);
	dns_name_fromregion(&name, &r);
	dns_name_init(&md->md, NULL);
	RETERR(name_duporclone(&name, &md->md));
	return (ISC_R_SUCCESS);
}

static inline void
freestruct_md(ARGS_FREESTRUCT) {
	dns_rdata_md_t *md = source;

	REQUIRE(source != NULL);
	REQUIRE(md->common.rdtype == dns_rdatatype_md);

	dns_name_free(&md->md);
}



#endif	/* RDATA_GENERIC_MD_3_C */
