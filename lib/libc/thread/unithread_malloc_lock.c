/*	$OpenBSD$	*/

#include <sys/cdefs.h>
#include <pthread.h>
#include "thread_private.h"

WEAK_PROTOTYPE(_thread_malloc_lock);
WEAK_PROTOTYPE(_thread_malloc_unlock);

WEAK_ALIAS(_thread_malloc_lock);
WEAK_ALIAS(_thread_malloc_unlock);

void
WEAK_NAME(_thread_malloc_lock)()
{
}

void
WEAK_NAME(_thread_malloc_unlock)()
{
}
