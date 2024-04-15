/*	$OpenBSD: ntohs.c,v 1.9 2014/07/21 01:51:10 guenther Exp $ */
/*
 * Public domain.
 */

#include <sys/types.h>
#include <endian.h>

#undef ntohs

uint16_t
ntohs(uint16_t x)
{
	return be16toh(x);
}
