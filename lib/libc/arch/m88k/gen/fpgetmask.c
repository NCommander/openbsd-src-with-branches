/*	$OpenBSD: fpgetmask.c,v 1.2 2003/01/07 22:01:29 miod Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 * Ported to 88k by Nivas Madhur.
 */

#include <ieeefp.h>

fp_except
fpgetmask()
{
	int x;

	__asm__ volatile ("fldcr %0,fcr63" : "=r" (x));
	return (x & 0x1f);
}
