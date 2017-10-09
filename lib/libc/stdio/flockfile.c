/*	$OpenBSD: flockfile.c,v 1.8 2012/09/01 01:08:16 fgsch Exp $	*/

#include <stdio.h>
#include "local.h"

void
flockfile(FILE *fp)
{
	FLOCKFILE(fp);
}
DEF_WEAK(flockfile);


int
ftrylockfile(FILE *fp)
{
	if (_thread_cb.tc_ftrylockfile != NULL)
		return (_thread_cb.tc_ftrylockfile(fp));

	return 0;
}
DEF_WEAK(ftrylockfile);

void
funlockfile(FILE *fp)
{
	FUNLOCKFILE(fp);
}
DEF_WEAK(funlockfile);
