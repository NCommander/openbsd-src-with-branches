/*
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD$"
#endif /* LIBC_SCCS */

#include <machine/asm.h>

ENTRY(fpgetround)
	fmovel fpcr,d0
	bfextu d0{#26:#2},d0
	rts
