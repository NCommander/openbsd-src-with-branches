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

/* $Id: lib.c,v 1.8 2020/01/20 18:51:52 florian Exp $ */

/*! \file */

#include <config.h>

#include <stddef.h>

#include <isc/hash.h>


#include <isc/once.h>
#include <isc/util.h>



#include <dns/lib.h>
#include <dns/result.h>

#include <dst/dst.h>


/***
 *** Globals
 ***/

unsigned int			dns_pps = 0U;

/***
 *** Functions
 ***/

static isc_once_t init_once = ISC_ONCE_INIT;
static isc_boolean_t initialize_done = ISC_FALSE;
static unsigned int references = 0;

static void
initialize(void) {
	isc_result_t result;

	REQUIRE(initialize_done == ISC_FALSE);

	dns_result_register();
	result = isc_hash_create(DNS_NAME_MAXWIRE);
	if (result != ISC_R_SUCCESS)
		return;

	result = dst_lib_init();
	if (result != ISC_R_SUCCESS)
		goto cleanup_hash;

	initialize_done = ISC_TRUE;
	return;

  cleanup_hash:
	isc_hash_destroy();
}

isc_result_t
dns_lib_init(void) {
	isc_result_t result;

	/*
	 * Since this routine is expected to be used by a normal application,
	 * it should be better to return an error, instead of an emergency
	 * abort, on any failure.
	 */
	result = isc_once_do(&init_once, initialize);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (!initialize_done)
		return (ISC_R_FAILURE);

	references++;

	return (ISC_R_SUCCESS);
}

void
dns_lib_shutdown(void) {
	isc_boolean_t cleanup_ok = ISC_FALSE;

	if (--references == 0)
		cleanup_ok = ISC_TRUE;

	if (!cleanup_ok)
		return;

	dst_lib_destroy();
	isc_hash_destroy();
}
