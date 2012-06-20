/*	$OpenBSD: queue_fsqueue.c,v 1.41 2012/06/01 11:42:34 eric Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <inttypes.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

enum queue_kind {
	Q_INCOMING,
	Q_QUEUE,
	Q_CORRUPT
};

static int	fsqueue_envelope_load(enum queue_kind, struct envelope *);
static int	fsqueue_envelope_update(enum queue_kind, struct envelope *);
static int	fsqueue_envelope_delete(enum queue_kind, struct envelope *);

static int	fsqueue_message_create(enum queue_kind, u_int32_t *);
static int	fsqueue_message_commit(enum queue_kind, u_int32_t);
static int	fsqueue_message_fd_r(enum queue_kind, u_int32_t);
static int	fsqueue_message_fd_rw(enum queue_kind, u_int32_t);
static int	fsqueue_message_delete(enum queue_kind, u_int32_t);
static int	fsqueue_message_corrupt(enum queue_kind, u_int32_t);

static int	fsqueue_message_path(enum queue_kind, uint32_t, char *, size_t);
static int	fsqueue_envelope_path(enum queue_kind, u_int64_t, char *, size_t);
static int	fsqueue_envelope_dump_atomic(char *, struct envelope *);

int	fsqueue_init(int);
int	fsqueue_message(enum queue_op, u_int32_t *);
int	fsqueue_envelope(enum queue_op , struct envelope *);
int	fsqueue_load_envelope_ascii(FILE *, struct envelope *);
int	fsqueue_dump_envelope_ascii(FILE *, struct envelope *);

void   *fsqueue_qwalk_new(u_int32_t);
int	fsqueue_qwalk(void *, u_int64_t *);
void	fsqueue_qwalk_close(void *);

#define PATH_INCOMING		"/incoming"
#define PATH_QUEUE		"/queue"
#define PATH_CORRUPT		"/corrupt"

#define PATH_MESSAGE		"/message"
#define PATH_ENVELOPES		"/envelopes"

#define PATH_EVPTMP		PATH_INCOMING "/envelope.tmp"

struct queue_backend	queue_backend_fs = {
	  fsqueue_init,
	  fsqueue_message,
	  fsqueue_envelope,
	  fsqueue_qwalk_new,
	  fsqueue_qwalk,
	  fsqueue_qwalk_close
};

static int
fsqueue_message_path(enum queue_kind qkind, uint32_t msgid, char *buf, size_t len)
{
	switch (qkind) {
	case Q_QUEUE:
		return bsnprintf(buf, len, "%s/%03x/%08x",
 		    PATH_QUEUE,
		    msgid & 0xfff,
		    msgid);
	case Q_INCOMING:
	case Q_CORRUPT:
		return bsnprintf(buf, len, "%s/%08x",
		    qkind == Q_INCOMING ? PATH_INCOMING : PATH_CORRUPT,
		    msgid);
	default:
		fatalx("fsqueue_message_path: unsupported queue kind.");
	}
	return (0);
}

static int
fsqueue_envelope_path(enum queue_kind qkind, uint64_t evpid, char *buf, size_t len)
{
	switch (qkind) {
	case Q_QUEUE:
		return bsnprintf(buf, len, "%s/%03x/%08x%s/%016" PRIx64,
		    PATH_QUEUE,
		    evpid_to_msgid(evpid) & 0xfff,
		    evpid_to_msgid(evpid),
		    PATH_ENVELOPES, evpid);
	case Q_INCOMING:
	case Q_CORRUPT:
		return bsnprintf(buf, len, "%s/%08x%s/%016" PRIx64,
		    qkind == Q_INCOMING ? PATH_INCOMING : PATH_CORRUPT,
		    evpid_to_msgid(evpid),
		    PATH_ENVELOPES, evpid);
	default:
		fatalx("fsqueue_envelope_path: unsupported queue kind.");
	}

	return (0);
}

static int
fsqueue_envelope_dump_atomic(char *dest, struct envelope *ep)
{
	FILE	*fp;
	char	 buf[MAXPATHLEN];

	/* temporary fix for multi-process access to the queue,
	 * should be fixed by rerouting ALL queue access through
	 * the queue process.
	 */
	snprintf(buf, sizeof buf, PATH_EVPTMP".%d", getpid());

	fp = fopen(buf, "w");
	if (fp == NULL) {
		if (errno == ENOSPC || errno == ENFILE)
			goto tempfail;
		fatal("fsqueue_envelope_dump_atomic: open");
	}

	if (! envelope_dump_file(ep, fp)) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_dump_atomic: fwrite");
	}

	if (! safe_fclose(fp))
		goto tempfail;
	fp = NULL;

	if (rename(buf, dest) == -1) {
		if (errno == ENOSPC)
			goto tempfail;
		fatal("fsqueue_envelope_dump_atomic: rename");
	}

	return 1;

tempfail:
	if (fp)
		fclose(fp);
	if (unlink(buf) == -1)
		fatal("fsqueue_envelope_dump_atomic: unlink");
	return 0;
}

static int
fsqueue_envelope_create(enum queue_kind qkind, struct envelope *ep)
{
	char		evpname[MAXPATHLEN];
	u_int64_t	evpid;
	struct stat	sb;

again:
	evpid = queue_generate_evpid(evpid_to_msgid(ep->id));
	fsqueue_envelope_path(qkind, evpid, evpname, sizeof(evpname));
	if (stat(evpname, &sb) != -1 || errno != ENOENT)
		goto again;
	ep->id = evpid;

	return (fsqueue_envelope_dump_atomic(evpname, ep));
}

static int
fsqueue_envelope_load(enum queue_kind qkind, struct envelope *ep)
{
	char pathname[MAXPATHLEN];
	FILE *fp;
	int  ret;

	fsqueue_envelope_path(qkind, ep->id, pathname, sizeof(pathname));

	fp = fopen(pathname, "r");
	if (fp == NULL) {
		if (errno == ENOENT || errno == ENFILE)
			return 0;
		fatal("fsqueue_envelope_load: fopen");
	}
	ret = envelope_load_file(ep, fp);

	fclose(fp);

	return ret;
}

static int
fsqueue_envelope_update(enum queue_kind qkind, struct envelope *ep)
{
	char dest[MAXPATHLEN];

	fsqueue_envelope_path(qkind, ep->id, dest, sizeof(dest));

	return (fsqueue_envelope_dump_atomic(dest, ep));
}

static int
fsqueue_envelope_delete(enum queue_kind qkind, struct envelope *ep)
{
	char pathname[MAXPATHLEN];

	log_debug("#### %s: queue_envelope_delete: %016" PRIx64,
	    __func__, ep->id);
	fsqueue_envelope_path(qkind, ep->id, pathname, sizeof(pathname));

	if (unlink(pathname) == -1) {
		log_debug("######: %s [errno: %d]", pathname, errno);
		fatal("fsqueue_envelope_delete: unlink");
	}

	*strrchr(pathname, '/') = '\0';

	if (rmdir(pathname) != -1)
		fsqueue_message_delete(qkind, evpid_to_msgid(ep->id));

	return 1;
}

static int
fsqueue_message_create(enum queue_kind qkind, u_int32_t *msgid)
{
	char rootdir[MAXPATHLEN];
	char evpdir[MAXPATHLEN];
	struct stat sb;

again:
	*msgid = queue_generate_msgid();
	
	/* prevent possible collision later when moving to Q_QUEUE */
	fsqueue_message_path(Q_QUEUE, *msgid, rootdir, sizeof(rootdir));
	if (stat(rootdir, &sb) != -1 || errno != ENOENT)
		goto again;

	fsqueue_message_path(qkind, *msgid, rootdir, sizeof(rootdir));
	if (mkdir(rootdir, 0700) == -1) {
		if (errno == EEXIST)
			goto again;

		if (errno == ENOSPC) {
			*msgid = 0;
			return 0;
		}
		fatal("fsqueue_message_create: mkdir");
	}

	strlcpy(evpdir, rootdir, sizeof(evpdir));
	strlcat(evpdir, PATH_ENVELOPES, sizeof(evpdir));

	if (mkdir(evpdir, 0700) == -1) {
		if (errno == ENOSPC) {
			rmdir(rootdir);
			*msgid = 0;
			return 0;
		}
		fatal("fsqueue_message_create: mkdir");
	}

	return 1;
}

static int
fsqueue_message_commit(enum queue_kind qkind, u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];
	char queuedir[MAXPATHLEN];
	char msgdir[MAXPATHLEN];

	fsqueue_message_path(qkind, msgid, rootdir, sizeof(rootdir));
	fsqueue_message_path(Q_QUEUE, msgid, msgdir, sizeof(msgdir));
	strlcpy(queuedir, msgdir, sizeof(queuedir));
	*strrchr(queuedir, '/') = '\0';

	if (mkdir(queuedir, 0700) == -1) {
		if (errno == ENOSPC)
			return 0;
		if (errno != EEXIST)
			fatal("fsqueue_message_commit: mkdir");
	}

	if (rename(rootdir, msgdir) == -1) {
		if (errno == ENOSPC)
			return 0;
		fatal("fsqueue_message_commit: rename");
	}

	return 1;
}

static int
fsqueue_message_fd_r(enum queue_kind qkind, u_int32_t msgid)
{
	int fd;
	char path[MAXPATHLEN];

	fsqueue_message_path(qkind, msgid, path, sizeof(path));
	strlcat(path, PATH_MESSAGE, sizeof(path));

	if ((fd = open(path, O_RDONLY)) == -1)
		fatal("fsqueue_message_fd_r: open");

	return fd;
}

static int
fsqueue_message_fd_rw(enum queue_kind qkind, u_int32_t msgid)
{
	char path[MAXPATHLEN];

	fsqueue_message_path(qkind, msgid, path, sizeof(path));
	strlcat(path, PATH_MESSAGE, sizeof(path));

	return open(path, O_CREAT|O_EXCL|O_RDWR, 0600);
}

static int
fsqueue_message_delete(enum queue_kind qkind, u_int32_t msgid)
{
	char rootdir[MAXPATHLEN];

	fsqueue_message_path(qkind, msgid, rootdir, sizeof(rootdir));

	if (rmtree(rootdir, 0) == -1)
		fatal("fsqueue_message_delete: rmtree");

	return 1;
}

static int
fsqueue_message_corrupt(enum queue_kind qkind, u_int32_t msgid)
{
	struct stat sb;
	char rootdir[MAXPATHLEN];
	char corruptdir[MAXPATHLEN];
	char buf[64];
	int  retry = 0;

	fsqueue_message_path(qkind, msgid, rootdir, sizeof(rootdir));
	fsqueue_message_path(Q_CORRUPT, msgid, corruptdir, sizeof(corruptdir));
again:
	if (stat(corruptdir, &sb) != -1 || errno != ENOENT) {
		fsqueue_message_path(Q_CORRUPT, msgid, corruptdir, sizeof(corruptdir));
		snprintf(buf, sizeof(buf), ".%i", retry++);
		strlcat(corruptdir, buf, sizeof(corruptdir));
		goto again;
	}

	if (rename(rootdir, corruptdir) == -1)
		fatalx("fsqueue_message_corrupt: rename");

	return 1;
}

int
fsqueue_init(int server)
{
	unsigned int	 n;
	char		*paths[] = { PATH_INCOMING, PATH_QUEUE, PATH_CORRUPT };
	char		 path[MAXPATHLEN];
	int		 ret;

	if (!fsqueue_envelope_path(Q_QUEUE, 0, path, sizeof(path)))
		errx(1, "cannot store envelope path in %s", PATH_QUEUE);
	if (!fsqueue_envelope_path(Q_INCOMING, 0, path, sizeof(path)))
		errx(1, "cannot store envelope path in %s", PATH_INCOMING);

	if (server)
		mvpurge(PATH_SPOOL PATH_INCOMING, PATH_SPOOL PATH_PURGE);

	ret = 1;
	for (n = 0; n < nitems(paths); n++) {
		strlcpy(path, PATH_SPOOL, sizeof(path));
		if (strlcat(path, paths[n], sizeof(path)) >= sizeof(path))
			errx(1, "path too long %s%s", PATH_SPOOL, paths[n]);

		if (ckdir(path, 0700, env->sc_pw->pw_uid, 0, server) == 0)
			ret = 0;
	}

	return ret;
}

int
fsqueue_message(enum queue_op qop, u_int32_t *msgid)
{
        switch (qop) {
        case QOP_CREATE:
		return fsqueue_message_create(Q_INCOMING, msgid);

        case QOP_DELETE:
		return fsqueue_message_delete(Q_INCOMING, *msgid);

        case QOP_COMMIT:
		return fsqueue_message_commit(Q_INCOMING, *msgid);

        case QOP_FD_R:
                return fsqueue_message_fd_r(Q_QUEUE, *msgid);

        case QOP_FD_RW:
                return fsqueue_message_fd_rw(Q_INCOMING, *msgid);

	case QOP_CORRUPT:
		return fsqueue_message_corrupt(Q_QUEUE, *msgid);

        default:
		fatalx("queue_fsqueue_message: unsupported operation.");
        }

	return 0;
}

int
fsqueue_envelope(enum queue_op qop, struct envelope *m)
{
        switch (qop) {
        case QOP_CREATE:
		/* currently, only bounce envelopes are created in queued
		 * messages
		 */
		return fsqueue_envelope_create(
		    m->type == D_BOUNCE ? Q_QUEUE : Q_INCOMING, m);

        case QOP_DELETE:
		return fsqueue_envelope_delete(Q_QUEUE, m);

        case QOP_LOAD:
		return fsqueue_envelope_load(Q_QUEUE, m);

        case QOP_UPDATE:
		return fsqueue_envelope_update(Q_QUEUE, m);

        default:
		fatalx("queue_fsqueue_envelope: unsupported operation.");
        }

	return 0;
}

#define	QWALK_AGAIN	0x1
#define	QWALK_RECURSE	0x2
#define	QWALK_RETURN	0x3

struct qwalk {
	char	  path[MAXPATHLEN];
	DIR	 *dirs[3];
	int	(*filefn)(struct qwalk *, char *);
	int	  bucket;
	int	  level;
	u_int32_t msgid;
};

int walk_queue(struct qwalk *, char *);

void *
fsqueue_qwalk_new(u_int32_t msgid)
{
	struct qwalk *q;

	q = calloc(1, sizeof(struct qwalk));
	if (q == NULL)
		fatal("qwalk_new: calloc");

	strlcpy(q->path, PATH_QUEUE, sizeof(q->path));

	q->level = 0;
	q->msgid = msgid;

	if (q->msgid) {
		/* force level and bucket */
		q->bucket = q->msgid & 0xfff;
		q->level = 2;
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%03x/%08x/%s",
			PATH_QUEUE, q->bucket, q->msgid, PATH_ENVELOPES))
			fatalx("walk_queue: snprintf");
	}
	q->filefn = walk_queue;
	q->dirs[q->level] = opendir(q->path);
	if (q->dirs[q->level] == NULL)
		fatal("qwalk_new: opendir");

	return (q);
}

int
fsqueue_qwalk(void *hdl, u_int64_t *evpid)
{
	struct qwalk *q = hdl;
	struct dirent	*dp;

again:
	errno = 0;
	dp = readdir(q->dirs[q->level]);
	if (errno)
		fatal("qwalk: readdir");
	if (dp == NULL) {
		closedir(q->dirs[q->level]);
		q->dirs[q->level] = NULL;
		if (q->level == 0 || q->msgid)
			return (0);
		q->level--;
		goto again;
	}

	if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0)
		goto again;

	switch (q->filefn(q, dp->d_name)) {
	case QWALK_AGAIN:
		goto again;
	case QWALK_RECURSE:
		goto recurse;
	case QWALK_RETURN: {
		char *endptr;

		errno = 0;
		*evpid = (u_int64_t)strtoull(dp->d_name, &endptr, 16);
		if (q->path[0] == '\0' || *endptr != '\0')
			goto again;
		if (errno == ERANGE && *evpid == ULLONG_MAX)
			goto again;
		if (q->msgid)
			if (evpid_to_msgid(*evpid) != q->msgid)
				return 0;

		return (1);
	}
	default:
		fatalx("qwalk: callback failed");
	}

recurse:
	q->level++;
	q->dirs[q->level] = opendir(q->path);
	if (q->dirs[q->level] == NULL) {
		if (errno == ENOENT) {
			q->level--;
			goto again;
		}
		fatal("qwalk: opendir");
	}
	goto again;
}

void
fsqueue_qwalk_close(void *hdl)
{
	int i;
	struct qwalk *q = hdl;

	for (i = 0; i <= q->level; i++)
		if (q->dirs[i])
			closedir(q->dirs[i]);

	bzero(q, sizeof(struct qwalk));
	free(q);
}

int
walk_queue(struct qwalk *q, char *fname)
{
	char	*ep;

	switch (q->level) {
	case 0:
		q->bucket = strtoul(fname, &ep, 16);
		if (fname[0] == '\0' || *ep != '\0') {
			log_warnx("walk_queue: invalid bucket: %s", fname);
			return (QWALK_AGAIN);
		}
		if (errno == ERANGE || q->bucket >= DIRHASH_BUCKETS) {
			log_warnx("walk_queue: invalid bucket: %s", fname);
			return (QWALK_AGAIN);
		}
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%03x",
			PATH_QUEUE, q->bucket & 0xfff))
			fatalx("walk_queue: snprintf");
		return (QWALK_RECURSE);
	case 1:
		if (! bsnprintf(q->path, sizeof(q->path), "%s/%03x/%s%s",
			PATH_QUEUE, q->bucket & 0xfff, fname,
			PATH_ENVELOPES))
			fatalx("walk_queue: snprintf");
		return (QWALK_RECURSE);
	case 2:
		return (QWALK_RETURN);
	}

	return (-1);
}
