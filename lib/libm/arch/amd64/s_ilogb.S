/*	$OpenBSD: s_ilogb.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(ilogb)
	movsd	%xmm0,-8(%rsp)
	fldl	-8(%rsp)
	fxtract
	fstp	%st
	fistpl	-8(%rsp)
	movl	-8(%rsp),%eax
	ret
