/*	$OpenBSD$ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(__ieee754_sqrtf)
#ifdef __i386__
	flds	4(%esp)
	fsqrt
#else
	sqrtss	%xmm0,%xmm0
#endif
	ret
