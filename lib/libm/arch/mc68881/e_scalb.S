/*	$OpenBSD$ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(__ieee754_scalb)
	fmoved	sp@(4),fp0
	fbeq	Ldone
	fscaled	sp@(12),fp0
Ldone:
	fmoved	fp0,sp@-
	movel	sp@+,d0
	movel	sp@+,d1
	rts
