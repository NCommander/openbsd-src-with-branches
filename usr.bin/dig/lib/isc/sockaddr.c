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

/* $Id: sockaddr.c,v 1.11 2020/09/15 08:19:29 florian Exp $ */

/*! \file */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

#include <isc/buffer.h>

#include <isc/region.h>
#include <isc/sockaddr.h>
#include <string.h>
#include <isc/util.h>

int
isc_sockaddr_equal(const isc_sockaddr_t *a, const isc_sockaddr_t *b) {
	return (isc_sockaddr_compare(a, b, ISC_SOCKADDR_CMPADDR|
					   ISC_SOCKADDR_CMPPORT|
					   ISC_SOCKADDR_CMPSCOPE));
}

int
isc_sockaddr_eqaddr(const isc_sockaddr_t *a, const isc_sockaddr_t *b) {
	return (isc_sockaddr_compare(a, b, ISC_SOCKADDR_CMPADDR|
					   ISC_SOCKADDR_CMPSCOPE));
}

int
isc_sockaddr_compare(const isc_sockaddr_t *a, const isc_sockaddr_t *b,
		     unsigned int flags)
{
	REQUIRE(a != NULL && b != NULL);

	if (a->type.ss.ss_len != b->type.ss.ss_len)
		return (0);

	/*
	 * We don't just memcmp because the sin_zero field isn't always
	 * zero.
	 */

	if (a->type.sa.sa_family != b->type.sa.sa_family)
		return (0);
	switch (a->type.sa.sa_family) {
	case AF_INET:
		if ((flags & ISC_SOCKADDR_CMPADDR) != 0 &&
		    memcmp(&a->type.sin.sin_addr, &b->type.sin.sin_addr,
			   sizeof(a->type.sin.sin_addr)) != 0)
			return (0);
		if ((flags & ISC_SOCKADDR_CMPPORT) != 0 &&
		    a->type.sin.sin_port != b->type.sin.sin_port)
			return (0);
		break;
	case AF_INET6:
		if ((flags & ISC_SOCKADDR_CMPADDR) != 0 &&
		    memcmp(&a->type.sin6.sin6_addr, &b->type.sin6.sin6_addr,
			   sizeof(a->type.sin6.sin6_addr)) != 0)
			return (0);
		/*
		 * If ISC_SOCKADDR_CMPSCOPEZERO is set then don't return
		 * 0 if one of the scopes in zero.
		 */
		if ((flags & ISC_SOCKADDR_CMPSCOPE) != 0 &&
		    a->type.sin6.sin6_scope_id != b->type.sin6.sin6_scope_id &&
		    ((flags & ISC_SOCKADDR_CMPSCOPEZERO) == 0 ||
		      (a->type.sin6.sin6_scope_id != 0 &&
		       b->type.sin6.sin6_scope_id != 0)))
			return (0);
		if ((flags & ISC_SOCKADDR_CMPPORT) != 0 &&
		    a->type.sin6.sin6_port != b->type.sin6.sin6_port)
			return (0);
		break;
	default:
		if (memcmp(&a->type, &b->type, a->type.ss.ss_len) != 0)
			return (0);
	}
	return (1);
}

isc_result_t
isc_sockaddr_totext(const isc_sockaddr_t *sockaddr, isc_buffer_t *target) {
	char pbuf[sizeof("65000")];
	unsigned int plen;
	isc_region_t avail;
	int error;
	char tmp[NI_MAXHOST];

	REQUIRE(sockaddr != NULL);

	/*
	 * Do the port first, giving us the opportunity to check for
	 * unsupported address families.
	 */
	switch (sockaddr->type.sa.sa_family) {
	case AF_INET:
		snprintf(pbuf, sizeof(pbuf), "%u", ntohs(sockaddr->type.sin.sin_port));
		break;
	case AF_INET6:
		snprintf(pbuf, sizeof(pbuf), "%u", ntohs(sockaddr->type.sin6.sin6_port));
		break;
	default:
		return (ISC_R_FAILURE);
	}

	plen = strlen(pbuf);
	INSIST(plen < sizeof(pbuf));

	error = getnameinfo(&sockaddr->type.sa, sockaddr->type.sa.sa_len, tmp,
	    sizeof(tmp), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV);
	if (strlen(tmp) > isc_buffer_availablelength(target))
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(target, tmp, strlen(tmp));

	if (1 + plen + 1 > isc_buffer_availablelength(target))
		return (ISC_R_NOSPACE);

	isc_buffer_putmem(target, (const unsigned char *)"#", 1);
	isc_buffer_putmem(target, (const unsigned char *)pbuf, plen);

	/*
	 * Null terminate after used region.
	 */
	isc_buffer_availableregion(target, &avail);
	INSIST(avail.length >= 1);
	avail.base[0] = '\0';

	return (ISC_R_SUCCESS);
}

void
isc_sockaddr_format(const isc_sockaddr_t *sa, char *array, unsigned int size) {
	isc_result_t result;
	isc_buffer_t buf;

	if (size == 0U)
		return;

	isc_buffer_init(&buf, array, size);
	result = isc_sockaddr_totext(sa, &buf);
	if (result != ISC_R_SUCCESS) {
		snprintf(array, size, "<unknown address, family %u>",
			 sa->type.sa.sa_family);
		array[size - 1] = '\0';
	}
}

void
isc_sockaddr_any(isc_sockaddr_t *sockaddr)
{
	memset(sockaddr, 0, sizeof(*sockaddr));
	sockaddr->type.sin.sin_family = AF_INET;
	sockaddr->type.sin.sin_len = sizeof(sockaddr->type.sin);
	sockaddr->type.sin.sin_addr.s_addr = INADDR_ANY;
	sockaddr->type.sin.sin_port = 0;
}

void
isc_sockaddr_any6(isc_sockaddr_t *sockaddr)
{
	memset(sockaddr, 0, sizeof(*sockaddr));
	sockaddr->type.sin6.sin6_family = AF_INET6;
	sockaddr->type.sin6.sin6_len = sizeof(sockaddr->type.sin6);
	sockaddr->type.sin6.sin6_addr = in6addr_any;
	sockaddr->type.sin6.sin6_port = 0;
}

void
isc_sockaddr_fromin(isc_sockaddr_t *sockaddr, const struct in_addr *ina,
		    in_port_t port)
{
	memset(sockaddr, 0, sizeof(*sockaddr));
	sockaddr->type.sin.sin_family = AF_INET;
	sockaddr->type.sin.sin_len = sizeof(sockaddr->type.sin);
	sockaddr->type.sin.sin_addr = *ina;
	sockaddr->type.sin.sin_port = htons(port);
}

void
isc_sockaddr_anyofpf(isc_sockaddr_t *sockaddr, int pf) {
     switch (pf) {
     case AF_INET:
	     isc_sockaddr_any(sockaddr);
	     break;
     case AF_INET6:
	     isc_sockaddr_any6(sockaddr);
	     break;
     default:
	     INSIST(0);
     }
}

void
isc_sockaddr_fromin6(isc_sockaddr_t *sockaddr, const struct in6_addr *ina6,
		     in_port_t port)
{
	memset(sockaddr, 0, sizeof(*sockaddr));
	sockaddr->type.sin6.sin6_family = AF_INET6;
	sockaddr->type.sin6.sin6_len = sizeof(sockaddr->type.sin6);
	sockaddr->type.sin6.sin6_addr = *ina6;
	sockaddr->type.sin6.sin6_port = htons(port);
}

int
isc_sockaddr_pf(const isc_sockaddr_t *sockaddr) {

	/*
	 * Get the protocol family of 'sockaddr'.
	 */

	return (sockaddr->type.sa.sa_family);
}

in_port_t
isc_sockaddr_getport(const isc_sockaddr_t *sockaddr) {
	in_port_t port = 0;

	switch (sockaddr->type.sa.sa_family) {
	case AF_INET:
		port = ntohs(sockaddr->type.sin.sin_port);
		break;
	case AF_INET6:
		port = ntohs(sockaddr->type.sin6.sin6_port);
		break;
	default:
		FATAL_ERROR(__FILE__, __LINE__,
			    "unknown address family: %d",
			    (int)sockaddr->type.sa.sa_family);
	}

	return (port);
}

int
isc_sockaddr_ismulticast(const isc_sockaddr_t *sockaddr) {
	switch (sockaddr->type.sa.sa_family) {
	case AF_INET:
		return (IN_MULTICAST(&sockaddr->type.sin.sin_addr.s_addr));
	case AF_INET6:
		return (IN6_IS_ADDR_MULTICAST(&sockaddr->type.sin6.sin6_addr));
	default:
		return (0);
	}
}

int
isc_sockaddr_issitelocal(const isc_sockaddr_t *sockaddr) {
	if (sockaddr->type.sa.sa_family == AF_INET6)
		return (IN6_IS_ADDR_SITELOCAL(&sockaddr->type.sin6.sin6_addr));
	return (0);
}

int
isc_sockaddr_islinklocal(const isc_sockaddr_t *sockaddr) {
	if (sockaddr->type.sa.sa_family == AF_INET6)
		return (IN6_IS_ADDR_LINKLOCAL(&sockaddr->type.sin6.sin6_addr));
	return (0);
}
