/*	$OpenBSD: unithread_malloc_lock.c,v 1.7 2008/01/01 00:43:39 kurt Exp $	*/

#include <sys/time.h>
#include "thread_private.h"

WEAK_PROTOTYPE(_thread_malloc_lock);
WEAK_PROTOTYPE(_thread_malloc_unlock);

WEAK_ALIAS(_thread_malloc_lock);
WEAK_ALIAS(_thread_malloc_unlock);

WEAK_PROTOTYPE(_thread_atexit_lock);
WEAK_PROTOTYPE(_thread_atexit_unlock);

WEAK_ALIAS(_thread_atexit_lock);
WEAK_ALIAS(_thread_atexit_unlock);

WEAK_PROTOTYPE(_thread_arc4_lock);
WEAK_PROTOTYPE(_thread_arc4_unlock);

WEAK_ALIAS(_thread_arc4_lock);
WEAK_ALIAS(_thread_arc4_unlock);

void
WEAK_NAME(_thread_malloc_lock)(void)
{
	return;
}

void
WEAK_NAME(_thread_malloc_unlock)(void)
{
	return;
}

void
WEAK_NAME(_thread_atexit_lock)(void)
{
	return;
}

void
WEAK_NAME(_thread_atexit_unlock)(void)
{
	return;
}

void
WEAK_NAME(_thread_arc4_lock)(void)
{
	return;
}

void
WEAK_NAME(_thread_arc4_unlock)(void)
{
	return;
}
