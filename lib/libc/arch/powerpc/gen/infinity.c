/*	$OpenBSD: infinity.c,v 1.2 2000/03/01 17:31:22 todd Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: infinity.c,v 1.2 2000/03/01 17:31:22 todd Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a PowerPC */
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };
