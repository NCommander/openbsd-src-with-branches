/*	$OpenBSD: arm.c,v 1.1 2004/01/21 16:43:40 drahn Exp $	*/
/*	$NetBSD: arm32.c,v 1.1 1996/04/01 21:51:22 mark Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: arm.c,v 1.1 2004/01/21 16:43:40 drahn Exp $";
#endif /* not lint */

#include "gprof.h"

/*
 * gprof -c isn't currently supported...
 */

/* XXX */
void
findcall( parentp , p_lowpc , p_highpc )
    nltype		*parentp;
    unsigned long	p_lowpc;
    unsigned long	p_highpc;
{
}
