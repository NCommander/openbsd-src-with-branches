/*	$OpenBSD$	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(htons)
	movl    %edi,%eax
	xchgb   %ah,%al
	ret
END(htons)
