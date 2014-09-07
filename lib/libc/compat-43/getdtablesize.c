/*	$OpenBSD$ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <unistd.h>

int
getdtablesize(void)
{
	return sysconf(_SC_OPEN_MAX);
}
