/*	$OpenBSD$	*/
/*
 * Public domain.
 * Written by Ted Unangst
 */

#include <lib/libsa/stand.h>

/*
 * explicit_bzero - don't let the compiler optimize away bzero
 */
void
explicit_bzero(void *p, size_t n)
{
	bzero(p, n);
}
