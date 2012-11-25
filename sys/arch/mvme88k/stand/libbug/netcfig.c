/*	$OpenBSD$	*/
/* public domain */

#include <sys/types.h>
#include <machine/prom.h>

#include "prom.h"

int
mvmeprom_netcfig(struct mvmeprom_netcfig *cfg)
{
	asm volatile ("or r2,r0,%0": : "r" (cfg));
	MVMEPROM_CALL(MVMEPROM_NETCFIG);
	return cfg->status;
}
