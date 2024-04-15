/*	$OpenBSD: htonl.c,v 1.7 2014/07/21 01:51:10 guenther Exp $ */
/*
 * Public domain.
 */

#include <sys/types.h>
#include <endian.h>

#undef htonl

uint32_t
htonl(uint32_t x)
{
	return htobe32(x);
}
