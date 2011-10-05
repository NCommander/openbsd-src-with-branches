/* $OpenBSD: cdefs.h,v 1.1.1.1 2009/07/31 09:28:41 miod Exp $ */
/* public domain */
#include <mips64/cdefs.h>

#if defined(lint) && !defined(__MIPSEB__)
#define __MIPSEB__
#undef __MIPSEL__
#endif
