/*	$OpenBSD: e_sqrtf.S,v 1.5 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>
#include "abi.h"

ENTRY(sqrtf)
	RETGUARD_SETUP(sqrtf, r11)
	sqrtss	%xmm0,%xmm0
	RETGUARD_CHECK(sqrtf, r11)
	ret
END_STD(sqrtf)
