/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD$";
#endif /* LIBC_SCCS and not lint */

#include <ieeefp.h>

fp_except
fpgetmask()
{
	int x;

	__asm__("st %%fsr,%0" : "=m" (*&x));
	return (x >> 23) & 0x1f;
}
