/*	$OpenBSD: rthread_sem.c,v 1.14 2013/11/21 17:43:57 tedu Exp $ */
/*
 * Copyright (c) 2004,2005,2013 Ted Unangst <tedu@openbsd.org>
 * All Rights Reserved.
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
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sha2.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include "rthread.h"

#define SHARED_IDENT ((void *)-1)

/* SHA256_DIGEST_STRING_LENGTH includes nul */
/* "/tmp/" + sha256 + ".sem" */
#define SEM_PATH_SIZE (5 + SHA256_DIGEST_STRING_LENGTH + 4)

/* long enough to be hard to guess */
#define SEM_RANDOM_NAME_LEN	160

/*
 * Size of memory to be mmap()'ed by named semaphores.
 * Should be >= SEM_PATH_SIZE and page-aligned.
 */
#define SEM_MMAP_SIZE	getpagesize()

/*
 * Internal implementation of semaphores
 */
int
_sem_wait(sem_t sem, int tryonly, const struct timespec *abstime,
    int *delayed_cancel)
{
	void *ident = (void *)&sem->waitcount;
	int r;

	if (sem->shared)
		ident = SHARED_IDENT;

	_spinlock(&sem->lock);
	if (sem->value) {
		sem->value--;
		r = 0;
	} else if (tryonly) {
		r = EAGAIN;
	} else {
		sem->waitcount++;
		do {
			r = __thrsleep(ident, CLOCK_REALTIME |
			    _USING_TICKETS, abstime, &sem->lock.ticket,
			    delayed_cancel);
			_spinlock(&sem->lock);
			/* ignore interruptions other than cancelation */
			if (r == EINTR && (delayed_cancel == NULL ||
			    *delayed_cancel == 0))
				r = 0;
		} while (r == 0 && sem->value == 0);
		sem->waitcount--;
		if (r == 0)
			sem->value--;
	}
	_spinunlock(&sem->lock);
	return (r);
}

/* always increment count */
int
_sem_post(sem_t sem)
{
	void *ident = (void *)&sem->waitcount;
	int rv = 0;

	if (sem->shared)
		ident = SHARED_IDENT;

	_spinlock(&sem->lock);
	sem->value++;
	if (sem->waitcount) {
		__thrwakeup(ident, 1);
		rv = 1;
	}
	_spinunlock(&sem->lock);
	return (rv);
}

/*
 * exported semaphores
 */
int
sem_init(sem_t *semp, int pshared, unsigned int value)
{
	sem_t sem, *sempshared;
	int i, oerrno;
	char name[SEM_RANDOM_NAME_LEN];

	if (!semp) {
		errno = EINVAL;
		return (-1);
	}

	if (pshared) {
		while (1) {
			for (i = 0; i < SEM_RANDOM_NAME_LEN - 1; i++)
				name[i] = arc4random_uniform(255) + 1;
			name[SEM_RANDOM_NAME_LEN - 1] = '\0';
			sempshared = sem_open(name, O_CREAT|O_EXCL);
			if (sempshared)
				break;
			if (errno == EEXIST)
				continue;
			if (errno != EINVAL && errno != EPERM)
				errno = ENOSPC;
			return (-1);
		}

		/* unnamed semaphore should not be opened twice */
		if (sem_unlink(name) == -1) {
			oerrno = errno;
			sem_close(sempshared);
			errno = oerrno;
			return (-1);
		}

		sem = *sempshared;
		sem->value = value;
		*semp = sem;
		free(sempshared);
		return (0);
	}

	if (value > SEM_VALUE_MAX) {
		errno = EINVAL;
		return (-1);
	}

	sem = calloc(1, sizeof(*sem));
	if (!sem) {
		errno = ENOSPC;
		return (-1);
	}
	sem->lock = _SPINLOCK_UNLOCKED_ASSIGN;
	sem->value = value;
	*semp = sem;

	return (0);
}

int
sem_destroy(sem_t *semp)
{
	sem_t sem;

	if (!semp || !(sem = *semp)) {
		errno = EINVAL;
		return (-1);
	}

	if (sem->waitcount) {
#define MSG "sem_destroy on semaphore with waiters!\n"
		write(2, MSG, sizeof(MSG) - 1);
#undef MSG
		errno = EBUSY;
		return (-1);
	}

	*semp = NULL;
	if (sem->shared)
		munmap(sem, SEM_MMAP_SIZE);
	else
		free(sem);

	return (0);
}

int
sem_getvalue(sem_t *semp, int *sval)
{
	sem_t sem;

	if (!semp || !(sem = *semp)) {
		errno = EINVAL;
		return (-1);
	}

	_spinlock(&sem->lock);
	*sval = sem->value;
	_spinunlock(&sem->lock);

	return (0);
}

int
sem_post(sem_t *semp)
{
	sem_t sem;

	if (!semp || !(sem = *semp)) {
		errno = EINVAL;
		return (-1);
	}

	_sem_post(sem);

	return (0);
}

int
sem_wait(sem_t *semp)
{
	pthread_t self = pthread_self();
	sem_t sem;
	int r;

	if (!semp || !(sem = *semp)) {
		errno = EINVAL;
		return (-1);
	}

	_enter_delayed_cancel(self);
	r = _sem_wait(sem, 0, NULL, &self->delayed_cancel);
	_leave_delayed_cancel(self, r);

	if (r) {
		errno = r;
		return (-1);
	}

	return (0);
}

int
sem_timedwait(sem_t *semp, const struct timespec *abstime)
{
	pthread_t self = pthread_self();
	sem_t sem;
	int r;

	if (!semp || !(sem = *semp)) {
		errno = EINVAL;
		return (-1);
	}

	_enter_delayed_cancel(self);
	r = _sem_wait(sem, 0, abstime, &self->delayed_cancel);
	_leave_delayed_cancel(self, r);

	if (r) {
		errno = r == EWOULDBLOCK ? ETIMEDOUT : r;
		return (-1);
	}

	return (0);
}

int
sem_trywait(sem_t *semp)
{
	sem_t sem;
	int r;

	if (!semp || !(sem = *semp)) {
		errno = EINVAL;
		return (-1);
	}

	r = _sem_wait(sem, 1, NULL, NULL);

	if (r) {
		errno = r;
		return (-1);
	}

	return (0);
}


static void
makesempath(const char *origpath, char *sempath, size_t len)
{
	char buf[SHA256_DIGEST_STRING_LENGTH];

	SHA256Data(origpath, strlen(origpath), buf);
	snprintf(sempath, len, "/tmp/%s.sem", buf);
}

sem_t *
sem_open(const char *name, int oflag, ...)
{
	char sempath[SEM_PATH_SIZE];
	struct stat sb;
	int created = 0, fd, oerrno;
	sem_t sem;
	sem_t *semp = SEM_FAILED;
	mode_t unusedmode;
	unsigned value = 0;

	if (oflag & ~(O_CREAT | O_EXCL)) {
		errno = EINVAL;
		return (semp);
	}

	if (oflag & O_CREAT) {
		va_list ap;
		va_start(ap, oflag);
		unusedmode = va_arg(ap, mode_t);
		value = va_arg(ap, unsigned);
		va_end(ap);
	}

	makesempath(name, sempath, sizeof(sempath));
	fd = open(sempath, O_RDWR | O_NOFOLLOW | oflag, 0600);
	if (fd == -1)
		return (semp);
	if (fstat(fd, &sb) == -1 || !S_ISREG(sb.st_mode)) {
		close(fd);
		errno = EINVAL;
		return (semp);
	}
	if (sb.st_uid != getuid()) {
		close(fd);
		errno = EPERM;
		return (semp);
	}
	if (sb.st_size != SEM_MMAP_SIZE) {
		if (!(oflag & O_CREAT)) {
			close(fd);
			errno = EINVAL;
			return (semp);
		}
		if (sb.st_size != 0) {
			close(fd);
			errno = EINVAL;
			return (semp);
		}
		if (ftruncate(fd, SEM_MMAP_SIZE) == -1) {
			oerrno = errno;
			close(fd);
			errno = oerrno;
			/* XXX can set errno to EIO, ENOTDIR... */
			return (semp);
		}
		created = 1;
	}
	sem = mmap(NULL, SEM_MMAP_SIZE, PROT_READ | PROT_WRITE,
	    MAP_FILE | MAP_SHARED | MAP_HASSEMAPHORE, fd, 0);
	oerrno = errno;
	close(fd);
	if (sem == MAP_FAILED) {
		errno = oerrno;
		return (semp);
	}
	if (created) {
		sem->lock = _SPINLOCK_UNLOCKED_ASSIGN;
		sem->value = value;
	}
	sem->shared = 1;
	semp = malloc(sizeof(*semp));
	if (!semp) {
		free(semp);
		munmap(sem, SEM_MMAP_SIZE);
		errno = ENOSPC;
		return (SEM_FAILED);
	}
	*semp = sem;

	return (semp);
}

int
sem_close(sem_t *semp)
{
	sem_t sem;

	if (!semp || !(sem = *semp) || !sem->shared) {
		errno = EINVAL;
		return (-1);
	}

	munmap(sem, SEM_MMAP_SIZE);
	free(semp);

	return (0);
}

int
sem_unlink(const char *name)
{
	char sempath[SEM_PATH_SIZE];

	makesempath(name, sempath, sizeof(sempath));
	return (unlink(sempath));
}
