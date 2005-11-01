/*	$OpenBSD: endian.h,v 1.2 2004/01/29 16:17:16 drahn Exp $	*/

#ifdef __ARMEB__
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#define	__STRICT_ALIGNMENT
#include <sys/endian.h>
