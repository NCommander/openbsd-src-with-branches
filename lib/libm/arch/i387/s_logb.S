/*	$OpenBSD$ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(logb)
	fldl	4(%esp)
	fxtract
	fstp	%st
	ret
