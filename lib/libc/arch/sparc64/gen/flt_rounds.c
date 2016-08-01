/*	$OpenBSD: flt_rounds.c,v 1.3 2012/06/25 17:01:11 deraadt Exp $	*/
/*	$NetBSD: flt_rounds.c,v 1.1 1998/09/11 04:56:23 eeh Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/types.h>
#include <float.h>

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	2,	/* round to positive infinity */
	3	/* round to negative infinity */
};

int
__flt_rounds()
{
	int x;

	__asm__("st %%fsr,%0" : "=m" (*&x));
	return map[(x >> 30) & 0x03];
}
DEF_STRONG(__flt_rounds);
