/*
 * Public domain. 2002, Matthieu Herrb
 *
 * $OpenBSD$
 */

#include <stdio.h>
#include "elfbug.h"

int
uninitialized(void)
{
	printf("uninitialized called\n");
	return 1;
}

int
bar(void)
{
	printf("bar\n");
	return 0;
}

void
fooinit(void)
{
	if (func == uninitialized) {
		func = bar;
	}
}
