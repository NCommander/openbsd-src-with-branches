/*
 * $OpenBSD$
 */

#include <signal.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/*
 * placeholder for sigaltstack XXX impl to be done
 */

int
sigaltstack(const struct sigaltstack *ss, struct sigaltstack *oss)
{
	errno = EINVAL;
	return (-1);
}
#endif /* _THREAD_SAFE */
