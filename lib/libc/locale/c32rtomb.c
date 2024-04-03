/*	$OpenBSD$ */
/*
 * Written by Ingo Schwarze <schwarze@openbsd.org>
 * and placed in the public domain on March 19, 2022.
 */

#include <uchar.h>
#include <wchar.h>

size_t
c32rtomb(char *s, char32_t c32, mbstate_t *ps)
{
	static mbstate_t mbs;

	if (ps == NULL)
		ps = &mbs;
	return wcrtomb(s, c32, ps);
}
