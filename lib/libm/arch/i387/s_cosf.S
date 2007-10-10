/*	$OpenBSD$ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

/* A float's domain isn't large enough to require argument reduction. */
ENTRY(cosf)
	flds	4(%esp)
	fcos
	ret	
