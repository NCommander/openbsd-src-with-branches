/*	$OpenBSD$ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "libbug.h"

/* returns 0 if no characters ready to read */
int
getchar()
{
	int ret;

	MVMEPROM_NOARG();
	MVMEPROM_CALL(MVMEPROM_INCHR);
	MVMEPROM_RETURN_BYTE(ret);
}
