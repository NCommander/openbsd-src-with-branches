/*
 * Written by Michael Shalayeff. Public Domain
 */

#include <sys/types.h>
#include <machine/ieeefp.h>
#include "math.h"

double
ceil(double x)
{
	u_int64_t ofpsr, fpsr;

	__asm__ __volatile__("fstds %%fr0,0(%0)" :: "r" (&ofpsr) : "memory");
	fpsr = (ofpsr & ~((u_int64_t)FP_RM << (9 + 32))) |
	    ((u_int64_t)FP_RP << (9 + 32));
	__asm__ __volatile__("fldds 0(%0), %%fr0" :: "r" (&fpsr) : "memory");

	__asm__ __volatile__("frnd,dbl %0,%0" : "+f" (x));

	__asm__ __volatile__("fldds 0(%0), %%fr0" :: "r" (&ofpsr) : "memory");
	return (x);
}

__weak_alias(ceill, ceil);
