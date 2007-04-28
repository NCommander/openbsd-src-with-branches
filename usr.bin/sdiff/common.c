/*	$OpenBSD: common.c,v 1.3 2006/05/10 14:32:51 ray Exp $	*/

/*
 * Written by Raymond Lai <ray@cyth.net>.
 * Public domain.
 */

#include <err.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"

void
cleanup(const char *filename)
{
	if (unlink(filename))
		err(2, "could not delete: %s", filename);
	exit(2);
}
