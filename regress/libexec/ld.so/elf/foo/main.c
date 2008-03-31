/*
 * Public domain. 2002, Matthieu Herrb
 *
 * $OpenBSD$
 */
#include <stdio.h>
#include "elfbug.h"

int (*func)(void) = uninitialized;

int
main(int argc, char *argv[])
{
	fooinit();
	return (*func)();
}

