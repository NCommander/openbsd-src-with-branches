/*	$OpenBSD: s_scalbn.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(scalbn)
	movsd	%xmm0,-8(%rsp)
	movl	%edi,-12(%rsp)
	fildl	-12(%rsp)
	fldl	-8(%rsp)
	fscale
	fstp	%st(1)		/* bug fix for fp stack overflow */
	fstpl	-8(%rsp)
	movsd	-8(%rsp),%xmm0
	ret

