/*	$OpenBSD: s_ilogbf.S,v 1.4 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>
#include "abi.h"

ENTRY(ilogbf)
	RETGUARD_SETUP(ilogbf, r11)
	movss	%xmm0,-4(%rsp)
	flds	-4(%rsp)
	fxtract
	fstp	%st
	fistpl	-4(%rsp)
	movl	-4(%rsp),%eax
	RETGUARD_CHECK(ilogbf, r11)
	ret
END_STD(ilogbf)
