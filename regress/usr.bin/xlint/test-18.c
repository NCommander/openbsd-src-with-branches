 /*	$OpenBSD$	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint dealing with LINTUSED comments.
 */

/* LINTUSED */
int g;

int u;

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	/* LINTUSED */
	int a, b;
	int c;

	return 0;
}
