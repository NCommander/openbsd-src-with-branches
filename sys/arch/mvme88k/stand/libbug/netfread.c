/*	$OpenBSD: netfread.c,v 1.3 2004/01/24 21:12:38 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "prom.h"

/* returns 0: success, nonzero: error */
int
mvmeprom_netfread(arg)
	struct mvmeprom_netfread *arg;
{
	asm volatile ("or r2,r0,%0": : "r" (arg));
	MVMEPROM_CALL(MVMEPROM_NETFREAD);
	return (arg->status);
}
