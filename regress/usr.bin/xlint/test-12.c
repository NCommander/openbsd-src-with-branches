/*	$OpenBSD: test-10.c,v 1.2 2005/12/09 03:34:34 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint warnings regarding assignment in conditional context.
 */
#include <limits.h>

/* ARGSUSED */
int
main(int argc, char *argv[])
{
	int a = 0;

	if (a = argc)		/* should warn */
		return 1;

	a++;

	if ((a = argc))		/* should not warn */
		return 1;

	return 0;
}


