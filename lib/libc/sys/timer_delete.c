/*	$OpenBSD$ */

#include <signal.h>
#include <time.h>
#include <errno.h>

/* ARGSUSED */
int
timer_delete(timer_t timerid)
{
	errno = ENOSYS;
	return -1;
}
