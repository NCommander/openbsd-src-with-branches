/*	$OpenBSD$ */

#include <signal.h>
#include <time.h>
#include <errno.h>

struct sigevent;

/* ARGSUSED */
int
timer_create(clockid_t clock_id, struct sigevent *evp, timer_t *timerid)
{
	errno = ENOSYS;
	return -1;
}
