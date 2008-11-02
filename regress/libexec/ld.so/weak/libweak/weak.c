/*	$OpenBSD$	*/

/*
 * Public domain. 2002, Federico Schwindt <fgsch@openbsd.org>.
 */

#include <sys/cdefs.h>
#include "defs.h"

int
weak_func()
{
	return (WEAK_REF);
}

__weak_alias(func,weak_func);
