/*	$OpenBSD$	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(_C_LABEL(htonl))
	movl    %edi,%eax
	xchgb   %ah,%al
	ret

