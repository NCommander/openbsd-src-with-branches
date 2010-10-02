 /*	$OpenBSD$	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Regression test for lint1 crash on expressions of the type:
 *
 *    char *foo = { "literal" }
 */

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	const char *foo = { "bar" };
	return 0;
}
