/*	$OpenBSD$	*/

/*
 * Written by Todd C. Miller <Todd.Miller@courtesan.com>
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "$OpenBSD: pause.c,v 1.2 1996/08/19 08:25:16 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <signal.h>

/*
 * Backwards compatible pause(3).
 */
int
pause()
{
	sigset_t mask;

	return (sigprocmask(SIG_BLOCK, NULL, &mask) ? -1 : sigsuspend(&mask));
}
