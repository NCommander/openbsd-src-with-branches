/*	$OpenBSD$ */
/*
 * Ted Unangst wrote this file and placed it into the public domain.
 */
#include <sys/mman.h>

int
posix_madvise(void *addr, size_t len, int behav)
{
	return (_thread_sys_madvise(addr, len, behav));
}
