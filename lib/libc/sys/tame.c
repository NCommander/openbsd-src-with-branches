/*	$OpenBSD: tame.c,v 1.2 2015/10/09 04:38:54 deraadt Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> Public Domain
 */

#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>

int tame(const char *req, const char **paths);

int
tame(const char *req, const char **paths)
{
	return (pledge(req, paths));
}
