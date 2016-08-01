/*	$OpenBSD: setjmp-signal.c,v 1.2 2002/07/31 05:25:55 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */

#include <setjmp.h>
#include <signal.h>

jmp_buf jb;

void
segv_handler(int signum)
{
	longjmp(jb, 1);
}

int
main()
{
	signal(SIGSEGV, segv_handler);
	if (setjmp(jb) == 0) {
		*((int *)0L) = 0;
		return (1);
	}
	return (0);
}
