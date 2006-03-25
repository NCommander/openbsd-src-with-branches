/*	$OpenBSD$ */
/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a sparc */
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };
