/*	$OpenBSD: ntohl.c,v 1.7 2014/07/21 01:51:10 guenther Exp $ */
/*
 * Public domain.
 */

#include <sys/types.h>
#include <endian.h>

#undef ntohl

uint32_t
ntohl(uint32_t x)
{
	return be32toh(x);
}
