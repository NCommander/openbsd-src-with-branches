#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD$"
#endif

#define STRRCHR
#include "rindex.S"
