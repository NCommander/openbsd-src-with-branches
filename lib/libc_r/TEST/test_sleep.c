/* ==== test_switch.c ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test context switch functionality.
 *
 *  1.00 93/08/04 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

const char buf[] = "abcdefghijklimnopqrstuvwxyz";
int fd = 1;

void* new_thread(void* arg)
{
	int i;

	for (i = 0; i < 10; i++) {
		write(fd, buf + (long) arg, 1);
		sleep(1);
	}
	return NULL;
}

int
main()
{
	pthread_t thread[2];
	int count = sizeof thread/sizeof thread[0];
	long i;

	printf("Going to sleep\n");
	sleep(3);
	printf("Done sleeping\n");

	for(i = 0; i < count; i++) {
		if (pthread_create(&thread[i], NULL, new_thread, (void *) i)) {
			printf("error creating new thread %ld\n", i);
		}
	}

	for (i = 0; i < count; i++)
		pthread_join(thread[i], NULL);

	return(0);
}
