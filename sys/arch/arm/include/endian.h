/*	$OpenBSD$	*/

#ifdef __ARMEB__
#define BYTE_ORDER BIG_ENDIAN
#else
#define BYTE_ORDER LITTLE_ENDIAN
#endif
#include <sys/endian.h>
