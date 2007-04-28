/*	$OpenBSD$ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(__ieee754_scalb)
	fldl	12(%esp)
	fldl	4(%esp)
	fscale
	fstp	%st(1)			/* bug fix for fp stack overflow */
	ret
