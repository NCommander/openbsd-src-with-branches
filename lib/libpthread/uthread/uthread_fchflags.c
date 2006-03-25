/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * $OpenBSD$
 */

#include <sys/stat.h>
#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
fchflags(int fd, unsigned int flags)
{
	int             ret;

	if ((ret = _FD_LOCK(fd, FD_WRITE, NULL)) == 0) {
		ret = _thread_sys_fchflags(fd, flags);
		_FD_UNLOCK(fd, FD_WRITE);
	}
	return (ret);
}
#endif
