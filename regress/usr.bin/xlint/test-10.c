/*	$OpenBSD$	*/

/*
 * Placed in the public domain by Alexander von Gernler <grunk@openbsd.org>
 *
 * Test if lint keywords in #define macros are preserved
 */

/* ARGSUSED */
void
foo(int bar) {
}

#define	S(x)	do { foo(x); } while (/* CONSTCOND */ 0)
#define	T(x)	do { foo(x); } while (0)

/* ARGSUSED */
int
main(int argc, char *argv[]) {
	S(1);
	T(1);

	do { foo(1); } while (/* CONSTCOND */ 0);
	do { foo(1); } while (0);

	return (0);
}
