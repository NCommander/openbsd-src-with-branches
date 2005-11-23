/*      $OpenBSD$ */

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test parsing of "inline" keyword.
 */

static inline int
foo(void);

static inline int
foo(void)
{
	return 0;
}

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	int i;
	i = foo();
	i++;
	return 0;
}
