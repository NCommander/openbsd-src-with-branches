/*	$OpenBSD: pause.c,v 1.5 2004/05/18 02:05:52 jfb Exp $	*/

/*
 * Written by Todd C. Miller <Todd.Miller@courtesan.com>
 * Public domain.
 */

#include <signal.h>
#include <unistd.h>

/*
 * Backwards compatible pause(3).
 */
int
pause(void)
{
	sigset_t mask;

	return (sigprocmask(SIG_BLOCK, NULL, &mask) ? -1 : sigsuspend(&mask));
}
