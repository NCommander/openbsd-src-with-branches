/*	$OpenBSD: regerror.c,v 1.2 1996/07/24 05:39:11 downsj Exp $	*/
#ifndef lint
static char *rcsid = "$OpenBSD: regerror.c,v 1.2 1996/07/24 05:39:11 downsj Exp $";
#endif /* not lint */

#include <regexp.h>
#include <stdio.h>

static void (*_new_regerror)() = NULL;

void
v8_regerror(s)
	const char *s;
{
	if (_new_regerror != NULL)
		_new_regerror(s);
	else
		warnx(s);
	return;
}

void
v8_setregerror(f)
	void (*f)();
{
	_new_regerror = f;
}
