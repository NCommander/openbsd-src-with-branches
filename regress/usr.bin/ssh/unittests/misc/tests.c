/* 	$OpenBSD: tests.c,v 1.4 2021/01/15 02:58:11 dtucker Exp $ */
/*
 * Regress test for misc helper functions.
 *
 * Placed in the public domain.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "test_helper.h"

#include "log.h"
#include "misc.h"

void test_parse(void);
void test_convtime(void);
void test_expand(void);

void
tests(void)
{
	test_parse();
	test_convtime();
	test_expand();
}
