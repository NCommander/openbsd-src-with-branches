/*
 * Written by J.T. Conklin, Apr 28, 1995
 * Public domain.
 */

#include <sys/types.h>
#include <machine/float.h>

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: flt_rounds.c,v 1.2 1996/08/19 08:16:40 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	2,	/* round to positive infinity */
	3	/* round to negative infinity */
};

int
__flt_rounds()
{
	int x;

	__asm__("sfsr %0" : "=r" (x));
	return map[(x >> 7) & 0x03];
}
