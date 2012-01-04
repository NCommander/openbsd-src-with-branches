/*	$OpenBSD$	*/
/*
 * Martin Pieuchot <mpi@openbsd.org>, 2011. Public Domain.
 */

#include <unistd.h>
#include <semaphore.h>
#include "test.h"

int
main(int argc, char **argv)
{
	sem_t sem;
	int val;

	CHECKr(sem_init(&sem, 0, 0));
	CHECKe(sem_getvalue(&sem, &val));
	ASSERT(val == 0);

	CHECKr(sem_post(&sem));
	CHECKe(sem_getvalue(&sem, &val));
	ASSERT(val == 1);

	CHECKe(sem_destroy(&sem));

	SUCCEED;
}
