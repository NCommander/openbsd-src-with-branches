/*	$OpenBSD: fpgetround.c,v 1.1 2002/03/11 02:59:01 miod Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/types.h>
#include <ieeefp.h>

fp_rnd
fpgetround()
{
	u_int32_t fpsr;

	__asm__ __volatile__("fstw %%fr0,0(%1)" : "=m"(fpsr) : "r"(&fpsr));
	return ((fpsr >> 9) & 0x3);
}
