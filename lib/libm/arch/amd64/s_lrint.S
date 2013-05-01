/*	$OpenBSD$	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <machine/asm.h>

ENTRY(lrint)
	cvtsd2si %xmm0, %rax
	ret

