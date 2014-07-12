/*	$OpenBSD: endian.h,v 1.6 2011/11/08 17:06:51 deraadt Exp $	*/

#ifndef _ARM_ENDIAN_H_
#define _ARM_ENDIAN_H_

#define _BYTE_ORDER _LITTLE_ENDIAN
#define	__STRICT_ALIGNMENT

#ifndef __FROM_SYS__ENDIAN
#include <sys/endian.h>
#endif
#endif /* _ARM_ENDIAN_H_ */
