#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD$"
#endif

#define MEMMOVE
#include "bcopy.S"
