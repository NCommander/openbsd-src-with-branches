#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD$"
#endif /* LIBC_SCCS */

#define STRRCHR
#include "rindex.S"
