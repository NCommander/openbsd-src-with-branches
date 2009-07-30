/*	$OpenBSD$ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(significand)
	fldl	4(%esp)
	fxtract
	fstp	%st(1)
	ret
