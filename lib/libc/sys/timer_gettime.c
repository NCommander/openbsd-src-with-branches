/*	$OpenBSD: timer_gettime.c,v 1.6 2005/08/08 08:05:37 espie Exp $ */

#include <time.h>
#include <errno.h>

int	timer_gettime(timer_t, struct itimerspec *);
PROTO_DEPRECATED(timer_gettime);

int
timer_gettime(timer_t timerid, struct itimerspec *value)
{
	errno = ENOSYS;
	return -1;
}
