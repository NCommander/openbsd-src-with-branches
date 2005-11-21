/*      $OpenBSD$ */

/*
 * This program is in the public domain.
 *
 * Test the ARGSUSED feature of lint.
 */

int
unusedargs(int unused)
{
	return 0;
}

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	unusedargs(1);
	return 0;
}
