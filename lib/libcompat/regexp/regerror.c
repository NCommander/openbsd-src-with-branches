/*	$OpenBSD$	*/
#ifndef lint
static char *rcsid = "$OpenBSD$";
#endif /* not lint */

#include <regexp.h>
#include <stdio.h>

void
v8_regerror(s)
const char *s;
{
	warnx(s);
	return;
}
