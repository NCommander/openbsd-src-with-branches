/*	$OpenBSD$	*/

#include "SYS.h"

ENTRY(sigsetjmp)
	bv	r0(rp)
	nop
EXIT(sigsetjmp)

ENTRY(siglongjmp)
	bv	r0(rp)
	nop
EXIT(siglongjmp)

	.end
