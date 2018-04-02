/*	$OpenBSD: s_logbf.S,v 1.3 2005/08/02 11:17:31 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include "DEFS.h"

ENTRY(logbf)
	flds	4(%esp)
	fxtract
	fstp	%st
	ret
END_STD(logbf)
