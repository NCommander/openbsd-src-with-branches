/*	$OpenBSD: memmove.S,v 1.1 1995/08/13 00:40:48 cgd Exp $	*/
/*	$NetBSD: memmove.S,v 1.1 1995/08/13 00:40:48 cgd Exp $	*/

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: memmove.S,v 1.2 1996/08/19 08:11:01 tholo Exp $"
#endif

#define MEMMOVE
#include "bcopy.S"
