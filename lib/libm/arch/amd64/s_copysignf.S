/*	$OpenBSD: s_copysignf.S,v 1.2 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@NetBSD.org>.
 * Public domain.
 */

#include <machine/asm.h>

/*
 * XXXfvdl split this file.
 */

.Lneg:
	.long 0x7fffffff
.Lpos:
	.long 0x80000000

ENTRY(copysignf)
#if 0
	/*
	 * XXXfvdl gas doesn't grok this.
	 * but it's legal according to the p4 manual.
	 */
	movss    .Lpos(%rip),%xmm2
	movss    .Lneg(%rip),%xmm3
	pandq   %xmm2,%xmm1
	pandq   %xmm3,%xmm0
	porq    %xmm1,%xmm0
#else
	movss	%xmm0,-4(%rsp)
	movss	%xmm1,-8(%rsp)
	movl	-8(%rsp),%edx
	andl	$0x80000000,%edx
	movl	-4(%rsp),%eax
	andl	$0x7fffffff,%eax
	orl	%edx,%eax
	movl	%eax,-4(%rsp)
	movss	-4(%rsp),%xmm0
#endif
	ret
