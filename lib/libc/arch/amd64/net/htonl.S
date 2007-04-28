/*	$OpenBSD$	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(_C_LABEL(htonl))
	movl    %edi,%eax
	bswapl  %eax
	ret
