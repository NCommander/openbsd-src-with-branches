/*	$OpenBSD$ */

#ifndef __MACHINE_BUGIO_H__
#define __MACHINE_BUGIO_H__

#include <sys/cdefs.h>

#include <machine/prom.h>

void buginit	__P((void));
int buginstat	__P((void));
char buginchr	__P((void));
void bugoutchr	__P((unsigned char));
void bugoutstr	__P((char *, char *));
void bugrtcrd	__P((struct mvmeprom_time *));
void bugreturn	__P((void));
void bugbrdid	__P((struct mvmeprom_brdid *));

#endif /* __MACHINE_BUGIO_H__ */
