/* infinity.c */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD$";
#endif /* LIBC_SCCS and not lint */

#include <math.h>

/* bytes for +Infinity on a ns32k */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
