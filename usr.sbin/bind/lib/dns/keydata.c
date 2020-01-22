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

/* $Id: keydata.c,v 1.5 2020/01/20 18:51:52 florian Exp $ */

/*! \file */


#include <stdlib.h>

#include <isc/buffer.h>
#include <string.h>
#include <isc/util.h>

#include <dns/rdata.h>
#include <dns/rdatastruct.h>
#include <dns/keydata.h>

isc_result_t
dns_keydata_todnskey(dns_rdata_keydata_t *keydata,
		     dns_rdata_dnskey_t *dnskey)
{
	REQUIRE(keydata != NULL && dnskey != NULL);

	dnskey->common.rdtype = dns_rdatatype_dnskey;
	dnskey->common.rdclass = keydata->common.rdclass;
	dnskey->flags = keydata->flags;
	dnskey->protocol = keydata->protocol;
	dnskey->algorithm = keydata->algorithm;

	dnskey->datalen = keydata->datalen;

	dnskey->data = malloc(dnskey->datalen);
	if (dnskey->data == NULL)
		return (ISC_R_NOMEMORY);
	memmove(dnskey->data, keydata->data, dnskey->datalen);

	return (ISC_R_SUCCESS);
}

isc_result_t
dns_keydata_fromdnskey(dns_rdata_keydata_t *keydata,
		       dns_rdata_dnskey_t *dnskey,
		       uint32_t refresh, uint32_t addhd,
		       uint32_t removehd)
{
	REQUIRE(keydata != NULL && dnskey != NULL);

	keydata->common.rdtype = dns_rdatatype_keydata;
	keydata->common.rdclass = dnskey->common.rdclass;
	keydata->refresh = refresh;
	keydata->addhd = addhd;
	keydata->removehd = removehd;
	keydata->flags = dnskey->flags;
	keydata->protocol = dnskey->protocol;
	keydata->algorithm = dnskey->algorithm;

	keydata->datalen = dnskey->datalen;
	keydata->data = malloc(keydata->datalen);
	if (keydata->data == NULL)
		return (ISC_R_NOMEMORY);
	memmove(keydata->data, dnskey->data, keydata->datalen);

	return (ISC_R_SUCCESS);
}
