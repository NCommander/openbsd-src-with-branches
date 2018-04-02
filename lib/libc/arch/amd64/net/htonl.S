/*	$OpenBSD$	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(htonl)
	movl    %edi,%eax
	bswapl  %eax
	ret
END(htonl)
