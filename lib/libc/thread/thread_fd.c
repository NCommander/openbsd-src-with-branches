/*	$OpenBSD: thread_fd.c,v 1.4 2001/09/04 22:17:45 fgsch Exp $	*/

#include <sys/time.h>
#include <pthread.h>
#include "thread_private.h"

WEAK_PROTOTYPE(_thread_fd_lock);
WEAK_PROTOTYPE(_thread_fd_unlock);

WEAK_ALIAS(_thread_fd_lock);
WEAK_ALIAS(_thread_fd_unlock);

int     
WEAK_NAME(_thread_fd_lock)(int fd, int lock_type, struct timespec *timeout,
			   const char *fname, int lineno)
{
	return 0;
}

void
WEAK_NAME(_thread_fd_unlock)(int fd, int lock_type, const char *fname,
			     int lineno)
{
}

