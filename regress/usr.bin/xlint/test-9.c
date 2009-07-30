/*      $OpenBSD: test-9.c,v 1.1 2005/12/01 05:19:25 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test warning on inequality comparison of unsigned value with
 * 0.
 */

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	unsigned int i;
	for (i = 100; i >= 0; i--)
		continue;

	return 0;
}
