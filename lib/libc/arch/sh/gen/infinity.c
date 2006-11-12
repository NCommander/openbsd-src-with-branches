/*	$OpenBSD: infinity.c,v 1.1.1.1 2006/10/10 22:07:10 miod Exp $	*/

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a SH4 FPU (double precision) */
char __infinity[] __attribute__((__aligned__(sizeof(double)))) =
#if _BYTE_ORDER == _LITTLE_ENDIAN
    { 0, 0, 0, 0, 0, 0, (char)0xf0, 0x7f };
#else
    { 0x7f, (char)0xf0, 0, 0, 0, 0, 0, 0 }
#endif
