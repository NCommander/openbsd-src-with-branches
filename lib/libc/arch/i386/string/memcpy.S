#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD$"
#endif

#define MEMCOPY
#include "bcopy.S"
