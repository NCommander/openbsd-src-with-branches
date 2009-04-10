/*	$OpenBSD$	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <machine/asm.h>

ENTRY(lrintf)
	cvtss2si %xmm0, %rax
	ret

