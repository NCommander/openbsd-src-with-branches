/*	$OpenBSD$	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(ntohs)
	movl    %edi,%eax
	xchgb   %ah,%al
	ret
END(ntohs)
