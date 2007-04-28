/*	$OpenBSD$ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(scalbn)
	fildl	12(%esp)
	fldl	4(%esp)
	fscale
	fstp	%st(1)		/* bug fix for fp stack overflow */
	ret
