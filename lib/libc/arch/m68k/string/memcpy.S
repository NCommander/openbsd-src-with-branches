#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD$"
#endif /* LIBC_SCCS */

#define MEMCOPY
#include "bcopy.S"
