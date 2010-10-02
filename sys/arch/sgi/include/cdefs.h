/*	$OpenBSD: cdefs.h,v 1.1 2004/08/06 21:12:18 pefo Exp $ */

/* Use Mips generic include file */

#include <mips64/cdefs.h>

#if defined(lint) && !defined(__MIPSEB__)
#define __MIPSEB__
#undef __MIPSEL__
#endif
