#if defined(SYSLIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: timer_create.c,v 1.2 1997/04/26 08:49:33 tholo Exp $";
#endif /* SYSLIBC_SCCS and not lint */

#include <signal.h>
#include <time.h>
#include <errno.h>

/* ARGSUSED */
int
timer_create(clock_id, evp, timerid)
	clockid_t clock_id;
	struct sigevent *evp;
	timer_t *timerid;
{
	errno = ENOSYS;
	return -1;
}
