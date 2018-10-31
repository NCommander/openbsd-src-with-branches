/*	$OpenBSD: s_significandf.S,v 1.4 2018/03/12 04:27:48 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

ENTRY(significandf)
	RETGUARD_SETUP(significandf, r11)
	XMM_ONE_ARG_FLOAT_PROLOGUE
	flds	ARG_FLOAT_ONE
	fxtract
	fstp	%st(1)
	XMM_FLOAT_EPILOGUE
	RETGUARD_CHECK(significandf, r11)
	ret
END(significandf)
	.weak	significandf
