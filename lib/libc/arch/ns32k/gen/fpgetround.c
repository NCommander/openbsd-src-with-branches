/*
 * Written by J.T. Conklin, Apr 28, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD$";
#endif /* LIBC_SCCS and not lint */

#include <ieeefp.h>

fp_rnd
fpgetround()
{
	int x;

	__asm__("sfsr %0" : "=r" (x));
	return (x >> 7) & 0x03;
}
