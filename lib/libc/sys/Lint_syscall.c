/*	$OpenBSD: Lint_syscall.c,v 1.1 1998/02/08 22:45:14 tholo Exp $	*/
/*	$NetBSD: Lint_syscall.c,v 1.1 1997/11/06 00:53:22 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>
#include <stdarg.h>

/*ARGSUSED*/
int
syscall(int arg1, ...)
{
	return (0);
}
