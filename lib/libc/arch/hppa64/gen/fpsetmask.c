/*	$OpenBSD: fpsetmask.c,v 1.1 2005/04/01 10:54:27 mickey Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/types.h>
#include <ieeefp.h>

fp_except
fpsetmask(mask)
	fp_except mask;
{
	u_int64_t fpsr;
	fp_except old;

	__asm__ volatile("fstd %%fr0,0(%1)" : "=m"(fpsr) : "r"(&fpsr));
	old = (fpsr >> 32) & 0x1f;
	fpsr = (fpsr & 0xffffffe000000000LL) | ((u_int64_t)(mask & 0x1f) << 32);
	__asm__ volatile("fldd 0(%0),%%fr0" : : "r"(&fpsr));
	return (old);
}
