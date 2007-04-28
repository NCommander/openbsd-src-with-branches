/*	$OpenBSD$ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(finitef)
	movl	4(%esp),%eax
	andl	$0x7f800000, %eax
	cmpl	$0x7f800000, %eax
	setne	%al
	andl	$0x000000ff, %eax
	ret
