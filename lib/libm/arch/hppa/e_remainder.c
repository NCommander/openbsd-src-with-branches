/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD$";
#endif

#include "math.h"

double
__ieee754_remainder(double x, double p)
{
	__asm__ __volatile__("frem,dbl %0,%1,%0" : "+f" (x) : "f" (p));

	return (x);
}
