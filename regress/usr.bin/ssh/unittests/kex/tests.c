/* 	$OpenBSD: tests.c,v 1.2 2023/02/02 12:12:52 djm Exp $ */
/*
 * Placed in the public domain
 */

#include "test_helper.h"

void kex_tests(void);
void kex_proposal_tests(void);
void kex_proposal_populate_tests(void);

void
tests(void)
{
	kex_tests();
	kex_proposal_tests();
	kex_proposal_populate_tests();
}
