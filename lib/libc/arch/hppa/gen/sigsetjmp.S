/*	$OpenBSD: sigsetjmp.S,v 1.2 2001/03/29 01:43:51 mickey Exp $	*/

#include "SYS.h"

#ifdef SYSLIBC_SCCS
	.text
	.asciz "$OpenBSD: sigsetjmp.S,v 1.2 2001/03/29 01:43:51 mickey Exp $"
	.align	4
#endif

ENTRY(sigsetjmp,0)
	bv	r0(rp)
	nop
EXIT(sigsetjmp)

ENTRY(siglongjmp,0)
	bv	r0(rp)
	nop
EXIT(siglongjmp)

	.end
