/*	$OpenBSD: kqueue-process.c,v 1.3 2003/06/12 04:52:40 mickey Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

#include <sys/types.h>
#include <sys/event.h>
#include <sys/wait.h>

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <signal.h>

static int process_child(void);

#define ASS(cond, mess) do { if (!(cond)) { mess; return 1; } } while (0)

#define ASSX(cond) ASS(cond, warnx("assertion " #cond " failed on line %d", __LINE__))

static void
usr1handler(int signum)
{
	/* nada */
}

int do_process(void);

int
do_process(void)
{
	struct kevent ke;
	int kq, status;
	pid_t pid, pid2;
	int didfork, didchild;
	int i;
	struct timespec ts;

	/*
	 * Timeout in case something doesn't work.
	 */
	ts.tv_sec = 10;
	ts.tv_nsec = 0;

	ASS((kq = kqueue()) >= 0, warn("kqueue"));

	/*
	 * Install a signal handler so that we can use pause() to synchronize
	 * with the child with the parent.
	 */
	signal(SIGUSR1, usr1handler);

	switch ((pid = fork())) {
	case -1:
		err(1, "fork");
	case 0:
		_exit(process_child());
	}

	sleep(2);	/* wait for child to settle down. */

	EV_SET(&ke, pid, EVFILT_PROC, EV_ADD|EV_ENABLE|EV_CLEAR,
	    NOTE_EXIT|NOTE_FORK|NOTE_EXEC|NOTE_TRACK, 0, NULL);
	ASS(kevent(kq, &ke, 1, NULL, 0, NULL) == 0,
	    warn("can't register events on kqueue"));

	kill(pid, SIGUSR1);	/* sync 1 */

	didfork = didchild = 0;

	pid2 = -1;
	for (i = 0; i < 2; i++) {
		ASS(kevent(kq, NULL, 0, &ke, 1, &ts) == 1,
		    warnx("didn't receive event"));
		ASSX(ke.filter == EVFILT_PROC);
		switch (ke.fflags) {
		case NOTE_CHILD:
			didchild = 1;
			ASSX((pid_t)ke.data == pid);
			pid2 = ke.ident;
			fprintf(stderr, "child %d\n", pid2);
			break;
		case NOTE_FORK:
			didfork = 1;
			ASSX(ke.ident == pid);
			fprintf(stderr, "fork\n");
			break;
		case NOTE_TRACKERR:
			errx(1, "child tracking failed due to resource shortage");
		default:
			errx(1, "kevent returned weird event 0x%x pid %d",
			    ke.fflags, (pid_t)ke.ident);
		}
	}

	if (pid2 == -1)
		return (1);

	/* Both children now sleeping. */

	ASSX(didchild == didfork == 1);
	
	kill(pid2, SIGUSR1);	/* sync 2.1 */
	kill(pid, SIGUSR1);	/* sync 2 */

	if (wait(&status) < 0)
		err(1, "wait");

	if (!WIFEXITED(status))
		errx(1, "child didn't exit?");

	close(kq);
	return (WEXITSTATUS(status) != 0);
}

static int
process_child(void)
{
	signal(SIGCHLD, SIG_IGN);	/* ignore our children. */

	pause();

	/* fork and see if tracking works. */
	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		/* sync 2.1 */
		pause();
		execl("/usr/bin/true", "true", (void *)NULL);
		err(1, "execl(true)");
	}

	/* sync 2 */
	pause();

	return 0;
}
