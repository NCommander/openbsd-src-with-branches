/*	$OpenBSD: bugio.h,v 1.12 2002/03/05 22:11:40 miod Exp $ */

#ifndef __MACHINE_BUGIO_H__
#define __MACHINE_BUGIO_H__

#include <sys/cdefs.h>

#include <machine/prom.h>

void buginit(void);
int buginstat(void);
char buginchr(void);
void bugoutchr(unsigned char);
void bugoutstr(char *, char *);
void bugrtcrd(struct mvmeprom_time *);
void bugreturn(void);
void bugbrdid(struct mvmeprom_brdid *);

#endif /* __MACHINE_BUGIO_H__ */
