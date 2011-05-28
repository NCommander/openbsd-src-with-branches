/*	$OpenBSD: e_scalb.S,v 1.3 2008/09/07 20:36:08 martynas Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(scalbf)
	fmoves	sp@(4),fp0
	fbeq	Ldone
	fscales	sp@(8),fp0
Ldone:
	fmoves	fp0,sp@-
	movel	sp@+,d0
	rts
