/*
 * David Leonard <d@openbsd.org>, 1999. Public Domain.
 *
 * $OpenBSD$
 */

#include <unistd.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

long
fpathconf(int fd, int name)
{
	int             ret;

	if ((ret = _FD_LOCK(fd, FD_READ, NULL)) == 0) {
		ret = _thread_sys_fpathconf(fd, name);
		_FD_UNLOCK(fd, FD_READ);
	}
	return (ret);
}
#endif
