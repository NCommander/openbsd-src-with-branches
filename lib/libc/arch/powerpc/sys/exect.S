/*
 * COPYRIGHT
 */
#if defined(SYSLIBC_SCCS)
	.text
	.asciz "$OpenBSD:$"
#endif /* SYSLIBC_SCCS */

#include "SYS.h"

	.text
PSEUDO_PREFIX(exect,execve)
	sc

	/* check for error */
	cmpwi 0, 0
	beq+ exect_ok		/* OK so this is stupid but I haven't read b */
	b cerror

	/* update, curbrk and return */
exect_ok:
	blr
