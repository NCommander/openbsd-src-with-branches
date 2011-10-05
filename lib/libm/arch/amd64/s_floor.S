/*	$OpenBSD: s_floor.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(floor)
	movsd	%xmm0, -8(%rsp)
	fstcw	-12(%rsp)
	movw	-12(%rsp),%dx
	orw	$0x0400,%dx
	andw	$0xf7ff,%dx
	movw	%dx,-16(%rsp)
	fldcw	-16(%rsp)
	fldl	-8(%rsp)
	frndint
	fldcw	-12(%rsp)
	fstpl	-8(%rsp)
	movsd	-8(%rsp),%xmm0
	ret
