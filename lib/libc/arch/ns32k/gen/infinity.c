/*	$OpenBSD$	*/

#ifndef lint
static char rcsid[] = "$OpenBSD$";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a ns32k */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
