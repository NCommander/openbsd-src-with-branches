/*	$OpenBSD: e_fmod.S,v 1.4 2016/09/12 19:47:01 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"


ENTRY(fmod)
	RETGUARD_SETUP(fmod, r11)
	XMM_TWO_ARG_DOUBLE_PROLOGUE
	fldl	ARG_DOUBLE_TWO
	fldl	ARG_DOUBLE_ONE
1:	fprem
	fstsw	%ax
	btw	$10,%ax
	jc	1b
	fstp	%st(1)
	XMM_DOUBLE_EPILOGUE
	RETGUARD_CHECK(fmod, r11)
	ret
END(fmod)
