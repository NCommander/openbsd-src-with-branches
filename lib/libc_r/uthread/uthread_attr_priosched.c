/*
 * David Leonard <d@openbsd.org>, 1998. Public Domain.
 *
 * $OpenBSD$
 */
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
pthread_attr_setscope(attr, contentionscope)
	pthread_attr_t *attr;
	int contentionscope;
{

	return (ENOSYS);
}

int
pthread_attr_getscope(attr, contentionscope)
	const pthread_attr_t *attr;
	int *contentionscope;
{

	return (ENOSYS);
}

int
pthread_attr_setinheritsched(attr, inheritsched)
	pthread_attr_t *attr;
	int inheritsched;
{

	return (ENOSYS);
}

int
pthread_attr_getinheritsched(attr, inheritsched)
	const pthread_attr_t *attr;
	int *inheritsched;
{

	return (ENOSYS);
}

int
pthread_attr_setschedpolicy(attr, policy)
	pthread_attr_t *attr;
	int policy;
{

	return (ENOSYS);
}

int
pthread_attr_getschedpolicy(attr, policy)
	const pthread_attr_t *attr;
	int *policy;
{

	return (ENOSYS);
}

int
pthread_attr_setschedparam(attr, param)
	pthread_attr_t *attr;
	const struct sched_param *param;
{

	return (ENOSYS);
}

int
pthread_attr_getschedparam(attr, param)
	const pthread_attr_t *attr;
	struct sched_param *param;
{

	return (ENOSYS);
}
#endif
