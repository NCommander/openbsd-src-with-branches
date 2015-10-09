/*	$OpenBSD: mquery.c,v 1.8 2015/09/11 13:26:20 guenther Exp $	*/
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
DEF_WEAK(mquery);
