/*	$OpenBSD$ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "libbug.h"

/* returns 0: success, nonzero: error */
int
mvmeprom_diskrd(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;

	MVMEPROM_ARG1(arg);
	MVMEPROM_CALL(MVMEPROM_DSKRD);
	MVMEPROM_STATRET(ret);
}
