/*	$OpenBSD$ */
/*
 * Written in 2017 by Ingo Schwarze <schwarze@openbsd.org>.
 * Released into the public domain.
 */

#include <string.h>

size_t
strxfrm_l(char *dst, const char *src, size_t n,
    locale_t locale __attribute__((__unused__)))
{
	return strxfrm(dst, src, n);
}
