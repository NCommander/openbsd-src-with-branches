/* 	$OpenBSD: tests.c,v 1.1 2015/01/15 23:41:29 markus Exp $ */
/*
 * Placed in the public domain
 */

#include "test_helper.h"

void kex_tests(void);
void kex_proposal(void);

void
tests(void)
{
	kex_tests();
	kex_proposal();
}
