/*	$OpenBSD$	*/
/*
 * IEEE-compatible infinity.c -- public domain.
 */

#include <endian.h>
#include <math.h>

char __infinity[] __attribute__((__aligned__(sizeof(double)))) =
#if BYTE_ORDER == BIG_ENDIAN
	{ 0x7f, 0xf0,    0,    0, 0, 0,    0,    0};
#else
	{    0,    0,    0,    0, 0, 0, 0xf0, 0x7f};
#endif
