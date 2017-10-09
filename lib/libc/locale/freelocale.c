/*	$OpenBSD$ */
/*
 * Written in 2017 by Ingo Schwarze <schwarze@openbsd.org>.
 * Released into the public domain.
 */

#include <locale.h>

void
freelocale(locale_t oldloc __attribute((__unused__)))
{
	/* Nothing to do here. */
}
