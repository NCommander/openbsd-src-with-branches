/*	$OpenBSD: endian.h,v 1.2 2005/01/20 15:04:54 drahn Exp $	*/

#ifdef __ARMEB__
#define _BYTE_ORDER _BIG_ENDIAN
#else
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif
#define	__STRICT_ALIGNMENT
#include <sys/endian.h>
