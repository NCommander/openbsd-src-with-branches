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

/* $ISC: rdataslab.h,v 1.20 2001/01/09 21:53:19 bwelling Exp $ */

#ifndef DNS_RDATASLAB_H
#define DNS_RDATASLAB_H 1

/*
 * DNS Rdata Slab
 *
 * Implements storage of rdatasets into slabs of memory.
 *
 * MP:
 *	Clients of this module must impose any required synchronization.
 *
 * Reliability:
 *	This module deals with low-level byte streams.  Errors in any of
 *	the functions are likely to crash the server or corrupt memory.
 *
 *	If the caller passes invalid memory references, these functions are
 *	likely to crash the server or corrupt memory.
 *
 * Resources:
 *	None.
 *
 * Security:
 *	None.
 *
 * Standards:
 *	None.
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

#define DNS_RDATASLAB_FORCE 0x1
#define DNS_RDATASLAB_EXACT 0x2

/***
 *** Functions
 ***/

isc_result_t
dns_rdataslab_fromrdataset(dns_rdataset_t *rdataset, isc_mem_t *mctx,
			   isc_region_t *region, unsigned int reservelen);
/*
 * Slabify a rdataset.  The slab area will be allocated and returned
 * in 'region'.
 *
 * Requires:
 *	'rdataset' is valid.
 *
 * Ensures:
 *	'region' will have base pointing to the start of allocated memory,
 *	with the slabified region beginning at region->base + reservelen.
 *	region->length contains the total length allocated.
 *
 * Returns:
 *	ISC_R_SUCCESS		- successful completion
 *	DNS_R_NOMEM		- no memory.
 *	<XXX others>
 */

unsigned int
dns_rdataslab_size(unsigned char *slab, unsigned int reservelen);
/*
 * Return the total size of an rdataslab.
 *
 * Requires:
 *	'slab' points to a slab.
 *
 * Returns:
 *	The number of bytes in the slab, including the reservelen.
 */

isc_result_t
dns_rdataslab_merge(unsigned char *oslab, unsigned char *nslab,
		    unsigned int reservelen, isc_mem_t *mctx,
		    dns_rdataclass_t rdclass, dns_rdatatype_t type,
		    unsigned int flags, unsigned char **tslabp);
/*
 * Merge 'oslab' and 'nslab'.
 */

isc_result_t
dns_rdataslab_subtract(unsigned char *mslab, unsigned char *sslab,
		       unsigned int reservelen, isc_mem_t *mctx,
		       dns_rdataclass_t rdclass, dns_rdatatype_t type,
		       unsigned int flags, unsigned char **tslabp);
/*
 * Subtract 'sslab' from 'mslab'.  If 'exact' is true then all elements
 * of 'sslab' must exist in 'mslab'.
 *
 * XXX
 * valid flags are DNS_RDATASLAB_EXACT
 */

isc_boolean_t
dns_rdataslab_equal(unsigned char *slab1, unsigned char *slab2,
		    unsigned int reservelen);

/* Compare two rdataslabs for equality.  This does _not_ do a full
 * DNSSEC comparison.
 *
 * Requires:
 *	'slab1' and 'slab2' point to slabs.
 *
 * Returns:
 *	ISC_TRUE if the slabs are equal, ISC_FALSE otherwise.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_RDATASLAB_H */
