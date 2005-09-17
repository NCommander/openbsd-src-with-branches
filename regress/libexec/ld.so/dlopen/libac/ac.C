/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: ac.C,v 1.2 2005/09/13 21:03:46 drahn Exp $
 */

#include <iostream>
#include <stdlib.h>
#include "ac.h"

extern int a;

extern "C" {
char *libname = "libac";
};

extern "C" void
lib_entry()
{
	std::cout << "called into ac " << libname << " libname " << "\n";
}

AC::AC(char *str)
{
	_name = str;
}

AC::~AC()
{
	std::cout << "dtors AC " << _name << "\n";
}
AC ac("local");
