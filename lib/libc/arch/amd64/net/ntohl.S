/*	$OpenBSD$	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(ntohl)
	movl    %edi,%eax
	bswapl  %eax
	ret
END(ntohl)
