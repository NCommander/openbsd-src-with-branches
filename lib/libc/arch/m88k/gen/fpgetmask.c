/*	$OpenBSD$	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 * Ported to 88k by Nivas Madhur.
 */

#include <ieeefp.h>

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD$";
#endif /* LIBC_SCCS and not lint */

fp_except
fpgetmask()
{
	int x;

	__asm__ volatile ("fldcr %0,fcr63" : "=r" (x));
	return (x & 0x1f);
}
