/*	$OpenBSD: uthread_vfork.c,v 1.2 1999/11/25 07:01:47 d Exp $	*/
#include <unistd.h>
#ifdef _THREAD_SAFE

pid_t	_dofork(int vfork);

pid_t
vfork(void)
{
	return (_dofork(1));
}
#endif
