/*	$OpenBSD$	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint warnings regarding suspicious sizeof use.
 */

typedef struct bar {
	int a;
} bar_t;

/* ARGSUSED */
int
main(int argc, char *argv[])
{
	bar_t bars[10];
	unsigned int a;
	
	a = sizeof(argc + 1);	/* warn */
	a = sizeof(1);		/* warn */
	a = sizeof(bars[1]);	/* ok */
	a = sizeof(bar_t);	/* ok */

	a++;
	return 0;
}


