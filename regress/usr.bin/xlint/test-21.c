 /*	$OpenBSD$*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Regression test lint1 crash on function prototypes having functions
 * as arguments.
 */
struct foo
{
	int a;
};

int bar(int, unsigned int(int, const struct foo *, int));

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	return 0;
}
