#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD$"
#endif /* LIBC_SCCS */

#define STRCHR
#include "index.S"
