/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD$";
#endif

#include "math.h"

float
__ieee754_remainder(float x, float p)
{
	__asm__ __volatile__("frem,sgl %0,%1,%0" : "+f" (x) : "f" (p));

	return (x);
}
