/*	$OpenBSD: mquery.c,v 1.1 2003/04/14 04:53:50 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> Public Domain
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>

/*
 * This function provides 64-bit offset padding.
 */
void *
mquery(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	return((void *)(long)__syscall((quad_t)SYS_mquery, addr, len, prot,
	    flags, fd, 0, offset));
}
