/*	$OpenBSD: s_ceil.c,v 1.10 2016/09/12 19:47:01 guenther Exp $	*/
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

	__asm__ volatile("fstds %%fr0,0(%1)" : "=m" (ofpsr) : "r" (&ofpsr));
	fpsr = (ofpsr & ~((u_int64_t)FP_RM << (9 + 32))) |
	    ((u_int64_t)FP_RP << (9 + 32));
	__asm__ volatile("fldds 0(%0), %%fr0" :: "r" (&fpsr), "m" (fpsr));

	__asm__ volatile("frnd,dbl %0,%0" : "+f" (x));

	__asm__ volatile("fldds 0(%0), %%fr0" :: "r" (&ofpsr), "m" (ofpsr));
	return (x);
}
DEF_STD(ceil);
LDBL_UNUSED_CLONE(ceil);
