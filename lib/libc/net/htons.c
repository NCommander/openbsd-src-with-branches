/*	$OpenBSD: htons.c,v 1.9 2014/07/21 01:51:10 guenther Exp $ */
/*
 * Public domain.
 */

#include <sys/types.h>
#include <endian.h>

#undef htons

uint16_t
htons(uint16_t x)
{
	return htobe16(x);
}
