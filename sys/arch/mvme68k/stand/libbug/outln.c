/*	$OpenBSD$ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "libbug.h"

void
mvmeprom_outln(start, end)
	char *start, *end;
{
	MVMEPROM_ARG1(end);
	MVMEPROM_ARG2(start);
	MVMEPROM_CALL(MVMEPROM_OUTSTRCRLF);
}
