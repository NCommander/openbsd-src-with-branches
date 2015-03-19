/*	$OpenBSD: infinity.c,v 1.3 2003/01/07 22:00:50 miod Exp $	*/

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on m88k */
char __infinity[] __attribute__((__aligned__(sizeof(double)))) =
    { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };
