/*	$OpenBSD: e_sqrtl.S,v 1.1 2009/11/05 20:14:32 kettenis Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include "DEFS.h"

ENTRY(sqrtl)
	fldt	4(%esp)
	fsqrt
	ret
END_STD(sqrtl)
