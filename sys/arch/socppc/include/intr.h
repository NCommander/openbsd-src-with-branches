/*	$OpenBSD$	*/

#include <powerpc/intr.h>

#ifndef _LOCORE

void softtty(void);

void *intr_establish(int, int, int,  int (*)(void *), void *, const char *);

#endif
