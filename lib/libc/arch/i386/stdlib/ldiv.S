/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#if defined(LIBC_SCCS)
RCSID("$NetBSD: ldiv.S,v 1.5 1995/04/28 22:59:52 jtc Exp $")
#endif

ENTRY(ldiv)
        movl    4(%esp),%eax
        movl    8(%esp),%ecx
        cdq
        idiv    %ecx
        movl    %eax,4(%esp)
        movl    %edx,8(%esp)
        ret
