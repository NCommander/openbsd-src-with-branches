/*	$OpenBSD: endian.h,v 1.3 2005/12/13 00:35:23 millert Exp $	*/

#ifndef _MACHINE_ENDIAN_H_
#define _MACHINE_ENDIAN_H_

#ifdef __ARMEB__
#define _BYTE_ORDER _BIG_ENDIAN
#else
#define _BYTE_ORDER _LITTLE_ENDIAN
#endif
#define	__STRICT_ALIGNMENT
#include <sys/endian.h>

#endif /* _MACHINE_ENDIAN_H_ */
