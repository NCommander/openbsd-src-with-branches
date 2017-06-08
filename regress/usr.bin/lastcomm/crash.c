/*	$OpenBSD$	*/

#include <err.h>
#include <stdlib.h>
#include <signal.h>

void handler(int);

int
main(int argc, char *argv[])
{
	int *i;

	if (signal(SIGSEGV, handler) == SIG_ERR)
		err(1, "signal");

	i = (void *)0x10UL;
	(*i)++;
	return *i;
}

void
handler(int signum)
{
	exit(0);
}
