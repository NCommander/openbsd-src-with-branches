/*	$OpenBSD: s_llrint.S,v 1.2 2016/09/12 19:47:01 guenther Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <machine/asm.h>

ENTRY(llrint)
	RETGUARD_SETUP(llrint, r11)
	cvtsd2si %xmm0, %rax
	RETGUARD_CHECK(llrint, r11)
	ret
END(llrint)
