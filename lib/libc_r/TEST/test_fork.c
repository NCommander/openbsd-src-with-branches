/*	$OpenBSD: test_fork.c,v 1.8 2000/10/04 05:50:58 d Exp $	*/
/*
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Test the fork system call.
 */

#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "test.h"


void *
empty(void *arg)
{

	return (void *)0x12345678;
}

void *
sleeper(void *arg)
{

	pthread_set_name_np(pthread_self(), "slpr");
	sleep(10);
	PANIC("sleeper timed out");
}


int
main()
{
	int flags;
	pthread_t sleeper_thread;
	void *result;
	int status;
	pid_t parent_pid;
	pid_t child_pid;

	parent_pid = getpid();

	CHECKe(flags = fcntl(STDOUT_FILENO, F_GETFL));
	if ((flags & (O_NONBLOCK | O_NDELAY))) {
		/* This fails when stdout is /dev/null!? */
		/*CHECKe*/(fcntl(STDOUT_FILENO, F_SETFL, 
		    flags & ~(O_NONBLOCK | O_NDELAY)));
	}

	CHECKr(pthread_create(&sleeper_thread, NULL, sleeper, NULL));
	sleep(1);

	printf("forking from pid %d\n", getpid());

	CHECKe(child_pid = fork());
	if (child_pid == 0) {
		/* child: */
		printf("child = pid %d\n", getpid());
		/* Our pid should change */
		ASSERT(getpid() != parent_pid);
		/* Our sleeper thread should have disappeared */
		printf("sleeper should have disappeared\n");
		ASSERT(ESRCH == pthread_join(sleeper_thread, &result));
		printf("sleeper disappeared correctly\n");
		/* Test starting another thread */
		CHECKr(pthread_create(&sleeper_thread, NULL, empty, NULL));
		sleep(1);
		CHECKr(pthread_join(sleeper_thread, &result));
		ASSERT(result == (void *)0x12345678);
		printf("child ok\n");
		_exit(0);
		PANIC("child _exit");
	}

	/* parent: */
	printf("parent = pid %d\n", getpid());
	/* Our pid should stay the same */
	ASSERT(getpid() == parent_pid);
	/* wait for the child */
	ASSERTe(wait(&status), == child_pid);
	/* the child should have called exit(0) */
	ASSERT(WIFEXITED(status));
	ASSERT(WEXITSTATUS(status) == 0);
	/* Our sleeper thread should still be around */
	CHECKr(pthread_detach(sleeper_thread));
	printf("parent ok\n");
	SUCCEED;
}
