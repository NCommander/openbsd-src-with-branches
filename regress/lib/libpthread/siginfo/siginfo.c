/* $OpenBSD$ */
/* PUBLIC DOMAIN Oct 2002 <marc@snafu.org> */

/* test SA_SIGINFO support */

#include <assert.h>
#include <signal.h>
#include <stdio.h>

#include "test.h"

void
act_handler(int signal, siginfo_t *siginfo, void *context)
{
 	ASSERT(siginfo && siginfo->si_addr == (char *) 0x987234 &&
	       siginfo->si_code == 1 && siginfo->si_trapno == 2);
}
 
int
main(int argc, char **argv)
{
	struct sigaction act;

	act.sa_sigaction = act_handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	CHECKe(sigaction(SIGSEGV, &act, NULL));
	*(char *) 0x987234 = 1;
	SUCCEED;
}
