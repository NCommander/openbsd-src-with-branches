/*	$OpenBSD$ */
/*
 * Written in 2017 by Ingo Schwarze <schwarze@openbsd.org>.
 * Released into the public domain.
 */

#include <time.h>

size_t
strftime_l(char *s, size_t maxsize, const char *format, const struct tm *t,
    locale_t locale __attribute__((__unused__)))
{
	return strftime(s, maxsize, format, t);
}
