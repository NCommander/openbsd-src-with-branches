/*
 * COPYRIGHT
 */
#if defined(SYSLIBC_SCCS)
	.text
	.asciz "$OpenBSD: exect.S,v 1.2 1998/08/07 01:44:01 rahnds Exp $"
#endif /* SYSLIBC_SCCS */

#include "SYS.h"

	.text
PREFIX2(exect,execve)
	sc

	/* check for error */
	cmpwi	0, 0
	beq+	_ASM_LABEL(exect_ok) 
	b	PIC_PLT(_ASM_LABEL(cerror))

	/* update, curbrk and return */
exect_ok:
	blr
