/*	$OpenBSD: sigsetjmp.S,v 1.1 1999/09/14 00:21:15 mickey Exp $	*/

#include "SYS.h"

#ifdef SYSLIBC_SCCS
	.text
	.asciz "$OpenBSD$"
	.align	4
#endif

ENTRY(sigsetjmp)
	bv	r0(rp)
	nop
EXIT(sigsetjmp)

ENTRY(siglongjmp)
	bv	r0(rp)
	nop
EXIT(siglongjmp)

	.end
