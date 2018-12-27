/*	$OpenBSD: kcov.c,v 1.4 2018/12/27 10:10:13 anton Exp $	*/

/*
 * Copyright (c) 2018 Anton Lindqvist <anton@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/ioctl.h>
#include <sys/kcov.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int test_close(int, int);
static int test_coverage(int, int);
static int test_dying(int, int);
static int test_exec(int, int);
static int test_fork(int, int);
static int test_open(int, int);
static int test_state(int, int);

static void do_syscall(void);
static void dump(const unsigned long *);
static void kcov_disable(int);
static void kcov_enable(int, int);
static int kcov_open(void);
static __dead void usage(void);

static const char *self;
static unsigned long bufsize = 256 << 10;

int
main(int argc, char *argv[])
{
	struct {
		const char *name;
		int (*fn)(int, int);
		int coverage;		/* test must produce coverage */
	} tests[] = {
		{ "close",	test_close,	0 },
		{ "coverage",	test_coverage,	1 },
		{ "dying",	test_dying,	1 },
		{ "exec",	test_exec,	1 },
		{ "fork",	test_fork,	1 },
		{ "open",	test_open,	0 },
		{ "state",	test_state,	1 },
		{ NULL,		NULL,		0 },
	};
	unsigned long *cover;
	int c, fd, i;
	int error = 0;
	int mode = 0;
	int prereq = 0;
	int reexec = 0;
	int verbose = 0;

	self = argv[0];

	while ((c = getopt(argc, argv, "Em:pv")) != -1)
		switch (c) {
		case 'E':
			reexec = 1;
			break;
		case 'm':
			if (strcmp(optarg, "pc") == 0)
				mode = KCOV_MODE_TRACE_PC;
			else
				errx(1, "unknown mode %s", optarg);
			break;
		case 'p':
			prereq = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (prereq) {
		fd = kcov_open();
		close(fd);
		return 0;
	}

	if (reexec) {
		do_syscall();
		return 0;
	}

	if (mode == 0 || argc != 1)
		usage();
	for (i = 0; tests[i].name != NULL; i++)
		if (strcmp(argv[0], tests[i].name) == 0)
			break;
	if (tests[i].name == NULL)
		errx(1, "%s: no such test", argv[0]);

	fd = kcov_open();
	if (ioctl(fd, KIOSETBUFSIZE, &bufsize) == -1)
		err(1, "ioctl: KIOSETBUFSIZE");
	cover = mmap(NULL, bufsize * sizeof(unsigned long),
	    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (cover == MAP_FAILED)
		err(1, "mmap");

	*cover = 0;
	error = tests[i].fn(fd, mode);
	if (verbose)
		dump(cover);
	if (tests[i].coverage && *cover == 0) {
                warnx("coverage empty (count=%lu, fd=%d)\n", *cover, fd);
		error = 1;
	} else if (!tests[i].coverage && *cover != 0) {
                warnx("coverage is not empty (count=%lu, fd=%d)\n", *cover, fd);
		error = 1;
	}

	if (munmap(cover, bufsize * sizeof(unsigned long)) == -1)
		err(1, "munmap");
	close(fd);

	return error;
}

static __dead void
usage(void)
{
	fprintf(stderr, "usage: kcov [-Epv] -t mode test\n");
	exit(1);
}

static void
do_syscall(void)
{
	getpid();
}

static void
dump(const unsigned long *cover)
{
	unsigned long i;

	for (i = 0; i < cover[0]; i++)
		printf("%lu/%lu: %p\n", i + 1, cover[0], (void *)cover[i + 1]);
}

static int
kcov_open(void)
{
	int fd;

	fd = open("/dev/kcov", O_RDWR);
	if (fd == -1)
		err(1, "open: /dev/kcov");
	return fd;
}

static void
kcov_enable(int fd, int mode)
{
	if (ioctl(fd, KIOENABLE, &mode) == -1)
		err(1, "ioctl: KIOENABLE");
}

static void
kcov_disable(int fd)
{
	if (ioctl(fd, KIODISABLE) == -1)
		err(1, "ioctl: KIODISABLE");
}

/*
 * Close before mmap.
 */
static int
test_close(int oldfd, int mode)
{
	int fd;

	fd = kcov_open();
	close(fd);
	return 0;
}

/*
 * Coverage of current thread.
 */
static int
test_coverage(int fd, int mode)
{
	kcov_enable(fd, mode);
	do_syscall();
	kcov_disable(fd);
	return 0;
}

static void *
closer(void *arg)
{
	int fd = *((int *)arg);

	close(fd);
	return NULL;
}

/*
 * Close kcov descriptor in another thread during tracing.
 */
static int
test_dying(int fd, int mode)
{
	pthread_t th;
	int error;

	kcov_enable(fd, mode);

	if ((error = pthread_create(&th, NULL, closer, &fd)))
		errc(1, error, "pthread_create");
	if ((error = pthread_join(th, NULL)))
		errc(1, error, "pthread_join");

	if (close(fd) == -1) {
		if (errno != EBADF)
			err(1, "close");
	} else {
		warnx("expected kcov descriptor to be closed");
		return 1;
	}

	return 0;
}

/*
 * Coverage of thread after exec.
 */
static int
test_exec(int fd, int mode)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0) {
		kcov_enable(fd, mode);
		execlp(self, self, "-E", NULL);
		_exit(1);
	}

	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid");
	if (WIFSIGNALED(status)) {
		warnx("terminated by signal (%d)", WTERMSIG(status));
		return 1;
	} else if (WEXITSTATUS(status) != 0) {
		warnx("non-zero exit (%d)", WEXITSTATUS(status));
		return 1;
	}

	/* Upon exit, the kcov descriptor must be reusable again. */
	kcov_enable(fd, mode);
	kcov_disable(fd);

	return 0;
}

/*
 * Coverage of thread after fork.
 */
static int
test_fork(int fd, int mode)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0) {
		kcov_enable(fd, mode);
		do_syscall();
		_exit(0);
	}

	if (waitpid(pid, &status, 0) == -1)
		err(1, "waitpid");
	if (WIFSIGNALED(status)) {
		warnx("terminated by signal (%d)", WTERMSIG(status));
		return 1;
	} else if (WEXITSTATUS(status) != 0) {
		warnx("non-zero exit (%d)", WEXITSTATUS(status));
		return 1;
	}

	/* Upon exit, the kcov descriptor must be reusable again. */
	kcov_enable(fd, mode);
	kcov_disable(fd);

	return 0;
}

/*
 * Open /dev/kcov more than once.
 */
static int
test_open(int oldfd, int mode)
{
	unsigned long *cover;
	int fd;
	int ret = 0;

	fd = kcov_open();
	if (ioctl(fd, KIOSETBUFSIZE, &bufsize) == -1)
		err(1, "ioctl: KIOSETBUFSIZE");
	cover = mmap(NULL, bufsize * sizeof(unsigned long),
	    PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (cover == MAP_FAILED)
		err(1, "mmap");

	kcov_enable(fd, mode);
	do_syscall();
	kcov_disable(fd);

	if (*cover == 0) {
		warnx("coverage empty (count=0, fd=%d)\n", fd);
		ret = 1;
	}
	if (munmap(cover, bufsize * sizeof(unsigned long)))
		err(1, "munmap");
	close(fd);
	return ret;
}

/*
 * State transitions.
 */
static int
test_state(int fd, int mode)
{
	if (ioctl(fd, KIOENABLE, &mode) == -1) {
		warn("KIOSETBUFSIZE -> KIOENABLE");
		return 1;
	}
	if (ioctl(fd, KIODISABLE) == -1) {
		warn("KIOENABLE -> KIODISABLE");
		return 1;
	}
	if (ioctl(fd, KIOSETBUFSIZE, 0) != -1) {
		warnx("KIOSETBUFSIZE -> KIOSETBUFSIZE");
		return 1;
	}
	if (ioctl(fd, KIODISABLE) != -1) {
		warnx("KIOSETBUFSIZE -> KIODISABLE");
		return 1;
	}

	kcov_enable(fd, mode);
	if (ioctl(fd, KIOENABLE, &mode) != -1) {
		warnx("KIOENABLE -> KIOENABLE");
		return 1;
	}
	if (ioctl(fd, KIOSETBUFSIZE, 0) != -1) {
		warnx("KIOENABLE -> KIOSETBUFSIZE");
		return 1;
	}
	kcov_disable(fd);

	return 0;
}
