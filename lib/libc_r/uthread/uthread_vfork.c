/*
 *	$OpenBSD$
 */
#include <unistd.h>
#ifdef _THREAD_SAFE

int
vfork(void)
{
	return (fork());
}
#endif /* _THREAD_SAFE */
