/*	$OpenBSD$ */

#include <signal.h>
#include <time.h>
#include <errno.h>

struct itimerspec;

/* ARGSUSED */
int
timer_gettime(timer_t timerid, struct itimerspec *value)
{
	errno = ENOSYS;
	return -1;
}
