/*	$OpenBSD$	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_err, err);
#else

#define	_err	err
#define	rcsid	_rcsid
#include "err.c"

#endif
