/*	$OpenBSD$ */
/*
 * Written by Matthew Dempsky, 2013.
 * Public domain.
 */

#include <unistd.h>

int
fdatasync(int fd)
{
	return (fsync(fd));
}
