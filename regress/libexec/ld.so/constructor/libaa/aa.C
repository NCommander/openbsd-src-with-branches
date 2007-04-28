/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD$
 */

#include "iostream.h"
#include "aa.h"
int a;


AA::AA(char *arg)
{
	a = 1;
}

AA foo("A");
