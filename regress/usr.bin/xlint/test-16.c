 /*	$OpenBSD: test-15.c,v 1.2 2005/12/17 20:05:49 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint dealing with cascaded ==
 */
#include <sys/types.h>

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	mode_t mode;

	mode = (mode_t)argc;

	if ((((mode) & 0170000) == 0100000) == 0)
		return 1;

	return 0;
}
