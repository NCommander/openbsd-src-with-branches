/*	$OpenBSD: mquery.c,v 1.6 2005/04/06 16:56:45 millert Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> Public Domain
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include "thread_private.h"

register_t __syscall(quad_t, ...);

STUB_PROTOTYPE(mquery);

STUB_ALIAS(mquery);

/*
 * This function provides 64-bit offset padding.
 */
void *
STUB_NAME(mquery)(void *addr, size_t len, int prot, int flags, int fd,
    off_t offset)
{
	return((void *)__syscall((quad_t)SYS_mquery, addr, len, prot,
	    flags, fd, 0, offset));
}
