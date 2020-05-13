/*	$OpenBSD: s_lrint.S,v 1.2 2016/09/12 19:47:01 guenther Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <machine/asm.h>

ENTRY(lrint)
	RETGUARD_SETUP(lrint, r11)
	cvtsd2si %xmm0, %rax
	RETGUARD_CHECK(lrint, r11)
	ret
END(lrint)
