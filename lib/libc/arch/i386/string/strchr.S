#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD$"
#endif

#define STRCHR
#include "index.S"
