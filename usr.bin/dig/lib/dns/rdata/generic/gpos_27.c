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

/* $Id: gpos_27.c,v 1.4 2020/02/24 12:06:13 florian Exp $ */

/* reviewed: Wed Mar 15 16:48:45 PST 2000 by brister */

/* RFC1712 */

#ifndef RDATA_GENERIC_GPOS_27_C
#define RDATA_GENERIC_GPOS_27_C

#define RRTYPE_GPOS_ATTRIBUTES (0)

static inline isc_result_t
totext_gpos(ARGS_TOTEXT) {
	isc_region_t region;
	int i;

	REQUIRE(rdata->type == dns_rdatatype_gpos);
	REQUIRE(rdata->length != 0);

	UNUSED(tctx);

	dns_rdata_toregion(rdata, &region);

	for (i = 0; i < 3; i++) {
		RETERR(txt_totext(&region, ISC_TRUE, target));
		if (i != 2)
			RETERR(str_totext(" ", target));
	}

	return (ISC_R_SUCCESS);
}

static inline isc_result_t
fromwire_gpos(ARGS_FROMWIRE) {
	int i;

	REQUIRE(type == dns_rdatatype_gpos);

	UNUSED(type);
	UNUSED(dctx);
	UNUSED(rdclass);
	UNUSED(options);

	for (i = 0; i < 3; i++)
		RETERR(txt_fromwire(source, target));
	return (ISC_R_SUCCESS);
}

static inline isc_result_t
towire_gpos(ARGS_TOWIRE) {

	REQUIRE(rdata->type == dns_rdatatype_gpos);
	REQUIRE(rdata->length != 0);

	UNUSED(cctx);

	return (mem_tobuffer(target, rdata->data, rdata->length));
}


static inline isc_result_t
fromstruct_gpos(ARGS_FROMSTRUCT) {
	dns_rdata_gpos_t *gpos = source;

	REQUIRE(type == dns_rdatatype_gpos);
	REQUIRE(source != NULL);
	REQUIRE(gpos->common.rdtype == type);
	REQUIRE(gpos->common.rdclass == rdclass);

	UNUSED(type);
	UNUSED(rdclass);

	RETERR(uint8_tobuffer(gpos->long_len, target));
	RETERR(mem_tobuffer(target, gpos->longitude, gpos->long_len));
	RETERR(uint8_tobuffer(gpos->lat_len, target));
	RETERR(mem_tobuffer(target, gpos->latitude, gpos->lat_len));
	RETERR(uint8_tobuffer(gpos->alt_len, target));
	return (mem_tobuffer(target, gpos->altitude, gpos->alt_len));
}

static inline isc_result_t
tostruct_gpos(ARGS_TOSTRUCT) {
	dns_rdata_gpos_t *gpos = target;
	isc_region_t region;

	REQUIRE(rdata->type == dns_rdatatype_gpos);
	REQUIRE(target != NULL);
	REQUIRE(rdata->length != 0);

	gpos->common.rdclass = rdata->rdclass;
	gpos->common.rdtype = rdata->type;
	ISC_LINK_INIT(&gpos->common, link);

	dns_rdata_toregion(rdata, &region);
	gpos->long_len = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	gpos->longitude = mem_maybedup(region.base, gpos->long_len);
	if (gpos->longitude == NULL)
		return (ISC_R_NOMEMORY);
	isc_region_consume(&region, gpos->long_len);

	gpos->lat_len = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	gpos->latitude = mem_maybedup(region.base, gpos->lat_len);
	if (gpos->latitude == NULL)
		goto cleanup_longitude;
	isc_region_consume(&region, gpos->lat_len);

	gpos->alt_len = uint8_fromregion(&region);
	isc_region_consume(&region, 1);
	if (gpos->lat_len > 0) {
		gpos->altitude =
			mem_maybedup(region.base, gpos->alt_len);
		if (gpos->altitude == NULL)
			goto cleanup_latitude;
	} else
		gpos->altitude = NULL;

	return (ISC_R_SUCCESS);

 cleanup_latitude:
	free(gpos->longitude);

 cleanup_longitude:
	free(gpos->latitude);
	return (ISC_R_NOMEMORY);
}

static inline void
freestruct_gpos(ARGS_FREESTRUCT) {
	dns_rdata_gpos_t *gpos = source;

	REQUIRE(source != NULL);
	REQUIRE(gpos->common.rdtype == dns_rdatatype_gpos);


	free(gpos->longitude);
	free(gpos->latitude);
	free(gpos->altitude);
}



#endif	/* RDATA_GENERIC_GPOS_27_C */
