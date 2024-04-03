/*	$OpenBSD: e_sqrtl.S,v 1.2 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>
#include "abi.h"

ENTRY(sqrtl)
	RETGUARD_SETUP(sqrtl, r11)
	fldt	8(%rsp)
	fsqrt
	RETGUARD_CHECK(sqrtl, r11)
	ret
END_STD(sqrtl)
