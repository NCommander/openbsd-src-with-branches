/*	$OpenBSD: m88k.c,v 1.3 2006/03/25 19:06:36 espie Exp $	*/
/*	$NetBSD: m88k.c,v 1.4 1995/04/19 07:16:07 cgd Exp $	*/

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */
void
findcall(nltype *parentp, unsigned long p_lowpc, unsigned long p_highpc)
{
}
