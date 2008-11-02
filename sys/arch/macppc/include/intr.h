/*	$OpenBSD: intr.h,v 1.2 2001/09/28 04:13:12 drahn Exp $	*/

#include <powerpc/intr.h>

#ifndef _LOCORE
void softtty(void);

void openpic_send_ipi(int);
void openpic_set_priority(int, int);
#endif
