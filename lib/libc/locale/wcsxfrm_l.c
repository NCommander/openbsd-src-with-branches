/*	$OpenBSD$ */
/*
 * Written in 2017 by Ingo Schwarze <schwarze@openbsd.org>.
 * Released into the public domain.
 */

#include <wchar.h>

size_t 
wcsxfrm_l(wchar_t *dest, const wchar_t *src, size_t n,
    locale_t locale __attribute__((__unused__)))
{
	return wcsxfrm(dest, src, n);
}
