/* 	$OpenBSD$ */
/*
 * Regress test for known_hosts-related API.
 *
 * Placed in the public domain
 */

void tests(void);
void test_iterate(void); /* test_iterate.c */

void
tests(void)
{
	test_iterate();
}

