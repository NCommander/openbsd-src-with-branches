/*	$OpenBSD: main.c,v 1.93 2021/02/04 08:10:24 claudio Exp $ */
/*
 * Copyright (c) 2019 Kristaps Dzonsons <kristaps@bsd.lv>
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

/*-
 * Copyright (C) 2009 Gabor Kovesdan <gabor@FreeBSD.org>
 * Copyright (C) 2012 Oleg Moskalenko <mom040267@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fts.h>
#include <poll.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <limits.h>
#include <syslog.h>
#include <unistd.h>
#include <imsg.h>

#include "extern.h"

/*
 * Maximum number of TAL files we'll load.
 */
#define	TALSZ_MAX	8

/*
 * An rsync repository.
 */
struct	repo {
	char		*repo;	/* repository rsync URI */
	char		*local;	/* local path name */
	char		*notify; /* RRDB notify URI if available */
	size_t		 id; /* identifier (array index) */
	int		 loaded; /* whether loaded or not */
};

size_t	entity_queue;
int	timeout = 60*60;
volatile sig_atomic_t killme;
void	suicide(int sig);

/*
 * Table of all known repositories.
 */
static struct	repotab {
	struct repo	*repos; /* repositories */
	size_t		 reposz; /* number of repos */
} rt;

/*
 * Database of all file path accessed during a run.
 */
struct filepath {
	RB_ENTRY(filepath)	entry;
	char			*file;
};

static inline int
filepathcmp(struct filepath *a, struct filepath *b)
{
	return strcasecmp(a->file, b->file);
}

RB_HEAD(filepath_tree, filepath);
RB_PROTOTYPE(filepath_tree, filepath, entry, filepathcmp);
struct filepath_tree  fpt = RB_INITIALIZER(&fpt);

static void	entityq_flush(struct msgbuf *, struct entityq *,
		    const struct repo *);

const char	*bird_tablename = "ROAS";

int	verbose;
int	noop;

struct stats	 stats;

/*
 * Log a message to stderr if and only if "verbose" is non-zero.
 * This uses the err(3) functionality.
 */
void
logx(const char *fmt, ...)
{
	va_list		 ap;

	if (verbose && fmt != NULL) {
		va_start(ap, fmt);
		vwarnx(fmt, ap);
		va_end(ap);
	}
}

/*
 * Functions to lookup which files have been accessed during computation.
 */
static void
filepath_add(char *file)
{
	struct filepath *fp;

	if ((fp = malloc(sizeof(*fp))) == NULL)
		err(1, NULL);
	if ((fp->file = strdup(file)) == NULL)
		err(1, NULL);

	if (RB_INSERT(filepath_tree, &fpt, fp) != NULL) {
		/* already in the tree */
		free(fp->file);
		free(fp);
	}
}

static int
filepath_exists(char *file)
{
	struct filepath needle;

	needle.file = file;
	return RB_FIND(filepath_tree, &fpt, &needle) != NULL;
}

RB_GENERATE(filepath_tree, filepath, entry, filepathcmp);

void
entity_free(struct entity *ent)
{

	if (ent == NULL)
		return;

	free(ent->pkey);
	free(ent->uri);
	free(ent->descr);
	free(ent);
}

/*
 * Read a queue entity from the descriptor.
 * Matched by entity_buffer_req().
 * The pointer must be passed entity_free().
 */
void
entity_read_req(int fd, struct entity *ent)
{

	io_simple_read(fd, &ent->type, sizeof(enum rtype));
	io_str_read(fd, &ent->uri);
	io_simple_read(fd, &ent->has_pkey, sizeof(int));
	if (ent->has_pkey)
		io_buf_read_alloc(fd, (void **)&ent->pkey, &ent->pkeysz);
	io_str_read(fd, &ent->descr);
}

/*
 * Like entity_write_req() but into a buffer.
 * Matched by entity_read_req().
 */
static void
entity_buffer_req(struct ibuf *b, const struct entity *ent)
{

	io_simple_buffer(b, &ent->type, sizeof(ent->type));
	io_str_buffer(b, ent->uri);
	io_simple_buffer(b, &ent->has_pkey, sizeof(int));
	if (ent->has_pkey)
		io_buf_buffer(b, ent->pkey, ent->pkeysz);
	io_str_buffer(b, ent->descr);
}

/*
 * Write the queue entity.
 * Simply a wrapper around entity_buffer_req().
 */
static void
entity_write_req(struct msgbuf *msgq, const struct entity *ent)
{
	struct ibuf *b;

	if ((b = ibuf_dynamic(sizeof(*ent), UINT_MAX)) == NULL)
		err(1, NULL);
	entity_buffer_req(b, ent);
	ibuf_close(msgq, b);
}

/*
 * Scan through all queued requests and see which ones are in the given
 * repo, then flush those into the parser process.
 */
static void
entityq_flush(struct msgbuf *msgq, struct entityq *q, const struct repo *repo)
{
	struct entity	*p, *np;

	TAILQ_FOREACH_SAFE(p, q, entries, np) {
		if (p->repo < 0 || repo->id != (size_t)p->repo)
			continue;
		entity_write_req(msgq, p);
		TAILQ_REMOVE(q, p, entries);
		entity_free(p);
	}
}

/*
 * Look up a repository, queueing it for discovery if not found.
 */
static const struct repo *
repo_lookup(struct msgbuf *msgq, const char *uri)
{
	const char	*host, *mod;
	size_t		 hostsz, modsz, i;
	char		*local;
	struct repo	*rp;
	struct ibuf	*b;

	if (!rsync_uri_parse(&host, &hostsz,
	    &mod, &modsz, NULL, NULL, NULL, uri))
		errx(1, "%s: malformed", uri);

	if (asprintf(&local, "%.*s/%.*s", (int)hostsz, host,
	    (int)modsz, mod) == -1)
		err(1, "asprintf");

	/* Look up in repository table. */

	for (i = 0; i < rt.reposz; i++) {
		if (strcmp(rt.repos[i].local, local))
			continue;
		free(local);
		return &rt.repos[i];
	}

	rt.repos = reallocarray(rt.repos,
		rt.reposz + 1, sizeof(struct repo));
	if (rt.repos == NULL)
		err(1, "reallocarray");

	rp = &rt.repos[rt.reposz++];
	memset(rp, 0, sizeof(struct repo));
	rp->id = rt.reposz - 1;
	rp->local = local;

	if ((rp->repo = strndup(uri, mod + modsz - uri)) == NULL)
		err(1, "strdup");

	if (!noop) {
		if (asprintf(&local, "%s", rp->local) == -1)
			err(1, "asprintf");
		logx("%s: pulling from network", local);
		if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
			err(1, NULL);
		io_simple_buffer(b, &rp->id, sizeof(rp->id));
		io_str_buffer(b, local);
		io_str_buffer(b, rp->repo);
		ibuf_close(msgq, b);
		free(local);
	} else {
		rp->loaded = 1;
		logx("%s: using cache", rp->local);
		stats.repos++;
		/* there is nothing in the queue so no need to flush */
	}
	return rp;
}

/*
 * Build local file name base on the URI and the repo info.
 */
static char *
repo_filename(const struct repo *repo, const char *uri)
{
	char *nfile;

	uri += strlen(repo->repo) + 1;
	if (asprintf(&nfile, "%s/%s", repo->local, uri) == -1)
		err(1, "asprintf");
	return nfile;
}

/*
 * Add the heap-allocated file to the queue for processing.
 */
static void
entityq_add(struct msgbuf *msgq, struct entityq *q, char *file, enum rtype type,
    const struct repo *rp, const unsigned char *pkey, size_t pkeysz,
    char *descr)
{
	struct entity	*p;

	if ((p = calloc(1, sizeof(struct entity))) == NULL)
		err(1, "calloc");

	p->type = type;
	p->uri = file;
	p->repo = (rp != NULL) ? (ssize_t)rp->id : -1;
	p->has_pkey = pkey != NULL;
	if (p->has_pkey) {
		p->pkeysz = pkeysz;
		if ((p->pkey = malloc(pkeysz)) == NULL)
			err(1, "malloc");
		memcpy(p->pkey, pkey, pkeysz);
	}
	if (descr != NULL)
		if ((p->descr = strdup(descr)) == NULL)
			err(1, "strdup");

	filepath_add(file);

	entity_queue++;

	/*
	 * Write to the queue if there's no repo or the repo has already
	 * been loaded else enqueue it for later.
	 */

	if (rp == NULL || rp->loaded) {
		entity_write_req(msgq, p);
		entity_free(p);
	} else
		TAILQ_INSERT_TAIL(q, p, entries);
}

/*
 * Add a file (CER, ROA, CRL) from an MFT file, RFC 6486.
 * These are always relative to the directory in which "mft" sits.
 */
static void
queue_add_from_mft(struct msgbuf *msgq, struct entityq *q, const char *mft,
    const struct mftfile *file, enum rtype type)
{
	char		*cp, *nfile;

	/* Construct local path from filename. */
	cp = strrchr(mft, '/');
	assert(cp != NULL);
	assert(cp - mft < INT_MAX);
	if (asprintf(&nfile, "%.*s/%s", (int)(cp - mft), mft, file->file) == -1)
		err(1, "asprintf");

	/*
	 * Since we're from the same directory as the MFT file, we know
	 * that the repository has already been loaded.
	 */

	entityq_add(msgq, q, nfile, type, NULL, NULL, 0, NULL);
}

/*
 * Loops over queue_add_from_mft() for all files.
 * The order here is important: we want to parse the revocation
 * list *before* we parse anything else.
 * FIXME: set the type of file in the mftfile so that we don't need to
 * keep doing the check (this should be done in the parser, where we
 * check the suffix anyway).
 */
static void
queue_add_from_mft_set(struct msgbuf *msgq, struct entityq *q,
    const struct mft *mft)
{
	size_t			 i, sz;
	const struct mftfile	*f;

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".crl"))
			continue;
		queue_add_from_mft(msgq, q, mft->file, f, RTYPE_CRL);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".cer"))
			continue;
		queue_add_from_mft(msgq, q, mft->file, f, RTYPE_CER);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".roa"))
			continue;
		queue_add_from_mft(msgq, q, mft->file, f, RTYPE_ROA);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".gbr"))
			continue;
		queue_add_from_mft(msgq, q, mft->file, f, RTYPE_GBR);
	}

	for (i = 0; i < mft->filesz; i++) {
		f = &mft->files[i];
		sz = strlen(f->file);
		assert(sz > 4);
		if (strcasecmp(f->file + sz - 4, ".crl") == 0 ||
		    strcasecmp(f->file + sz - 4, ".cer") == 0 ||
		    strcasecmp(f->file + sz - 4, ".roa") == 0 ||
		    strcasecmp(f->file + sz - 4, ".gbr") == 0)
			continue;
		logx("%s: unsupported file type: %s", mft->file, f->file);
	}
}

/*
 * Add a local TAL file (RFC 7730) to the queue of files to fetch.
 */
static void
queue_add_tal(struct msgbuf *msgq, struct entityq *q, const char *file)
{
	char	*nfile, *buf;

	if ((nfile = strdup(file)) == NULL)
		err(1, "strdup");
	buf = tal_read_file(file);

	/* Record tal for later reporting */
	if (stats.talnames == NULL)
		stats.talnames = strdup(file);
	else {
		char *tmp;
		if (asprintf(&tmp, "%s %s", stats.talnames, file) == -1)
			err(1, "asprintf");
		free(stats.talnames);
		stats.talnames = tmp;
	}

	/* Not in a repository, so directly add to queue. */
	entityq_add(msgq, q, nfile, RTYPE_TAL, NULL, NULL, 0, buf);
	/* entityq_add makes a copy of buf */
	free(buf);
}

/*
 * Add URIs (CER) from a TAL file, RFC 8630.
 */
static void
queue_add_from_tal(struct msgbuf *procq, struct msgbuf *rsyncq,
    struct entityq *q, const struct tal *tal)
{
	char			*nfile;
	const struct repo	*repo;
	const char		*uri = NULL;
	size_t			 i;

	assert(tal->urisz);

	for (i = 0; i < tal->urisz; i++) {
		uri = tal->uri[i];
		if (strncasecmp(uri, "rsync://", 8) == 0)
			break;
	}
	if (uri == NULL)
		errx(1, "TAL file has no rsync:// URI");

	/* Look up the repository. */
	repo = repo_lookup(rsyncq, uri);
	nfile = repo_filename(repo, uri);

	entityq_add(procq, q, nfile, RTYPE_CER, repo, tal->pkey,
	    tal->pkeysz, tal->descr);
}

/*
 * Add a manifest (MFT) found in an X509 certificate, RFC 6487.
 */
static void
queue_add_from_cert(struct msgbuf *procq, struct msgbuf *rsyncq,
    struct entityq *q, const struct cert *cert)
{
	const struct repo	*repo;
	char			*nfile;

	repo = repo_lookup(rsyncq, cert->mft);
	nfile = repo_filename(repo, cert->mft);

	entityq_add(procq, q, nfile, RTYPE_MFT, repo, NULL, 0, NULL);
}

/*
 * Process parsed content.
 * For non-ROAs, we grok for more data.
 * For ROAs, we want to extract the valid info.
 * In all cases, we gather statistics.
 */
static void
entity_process(int proc, struct msgbuf *procq, struct msgbuf *rsyncq,
    struct stats *st, struct entityq *q, struct vrp_tree *tree)
{
	enum rtype	type;
	struct tal	*tal;
	struct cert	*cert;
	struct mft	*mft;
	struct roa	*roa;
	int		 c;

	/*
	 * For most of these, we first read whether there's any content
	 * at all---this means that the syntactic parse failed (X509
	 * certificate, for example).
	 * We follow that up with whether the resources didn't parse.
	 */
	io_simple_read(proc, &type, sizeof(type));

	switch (type) {
	case RTYPE_TAL:
		st->tals++;
		tal = tal_read(proc);
		queue_add_from_tal(procq, rsyncq, q, tal);
		tal_free(tal);
		break;
	case RTYPE_CER:
		st->certs++;
		io_simple_read(proc, &c, sizeof(int));
		if (c == 0) {
			st->certs_fail++;
			break;
		}
		cert = cert_read(proc);
		if (cert->valid) {
			/*
			 * Process the revocation list from the
			 * certificate *first*, since it might mark that
			 * we're revoked and then we don't want to
			 * process the MFT.
			 */
			queue_add_from_cert(procq, rsyncq,
			    q, cert);
		} else
			st->certs_invalid++;
		cert_free(cert);
		break;
	case RTYPE_MFT:
		st->mfts++;
		io_simple_read(proc, &c, sizeof(int));
		if (c == 0) {
			st->mfts_fail++;
			break;
		}
		mft = mft_read(proc);
		if (mft->stale)
			st->mfts_stale++;
		queue_add_from_mft_set(procq, q, mft);
		mft_free(mft);
		break;
	case RTYPE_CRL:
		st->crls++;
		break;
	case RTYPE_ROA:
		st->roas++;
		io_simple_read(proc, &c, sizeof(int));
		if (c == 0) {
			st->roas_fail++;
			break;
		}
		roa = roa_read(proc);
		if (roa->valid)
			roa_insert_vrps(tree, roa, &st->vrps, &st->uniqs);
		else
			st->roas_invalid++;
		roa_free(roa);
		break;
	case RTYPE_GBR:
		st->gbrs++;
		break;
	default:
		abort();
	}

	entity_queue--;
}

/*
 * Assign filenames ending in ".tal" in "/etc/rpki" into "tals",
 * returning the number of files found and filled-in.
 * This may be zero.
 * Don't exceded "max" filenames.
 */
static size_t
tal_load_default(const char *tals[], size_t max)
{
	static const char *confdir = "/etc/rpki";
	size_t s = 0;
	char *path;
	DIR *dirp;
	struct dirent *dp;

	dirp = opendir(confdir);
	if (dirp == NULL)
		err(1, "open %s", confdir);
	while ((dp = readdir(dirp)) != NULL) {
		if (fnmatch("*.tal", dp->d_name, FNM_PERIOD) == FNM_NOMATCH)
			continue;
		if (s >= max)
			err(1, "too many tal files found in %s",
			    confdir);
		if (asprintf(&path, "%s/%s", confdir, dp->d_name) == -1)
			err(1, "asprintf");
		tals[s++] = path;
	}
	closedir (dirp);
	return (s);
}

static char **
add_to_del(char **del, size_t *dsz, char *file)
{
	size_t i = *dsz;

	del = reallocarray(del, i + 1, sizeof(*del));
	if (del == NULL)
		err(1, "reallocarray");
	del[i] = strdup(file);
	if (del[i] == NULL)
		err(1, "strdup");
	*dsz = i + 1;
	return del;
}

static size_t
repo_cleanup(const char *cachedir)
{
	size_t i, delsz = 0;
	char *argv[2], **del = NULL;
	FTS *fts;
	FTSENT *e;

	/* change working directory to the cache directory */
	if (chdir(cachedir) == -1)
		err(1, "%s: chdir", cachedir);

	for (i = 0; i < rt.reposz; i++) {
		if (asprintf(&argv[0], "%s", rt.repos[i].local) == -1)
			err(1, NULL);
		argv[1] = NULL;
		if ((fts = fts_open(argv, FTS_PHYSICAL | FTS_NOSTAT,
		    NULL)) == NULL)
			err(1, "fts_open");
		errno = 0;
		while ((e = fts_read(fts)) != NULL) {
			switch (e->fts_info) {
			case FTS_NSOK:
				if (!filepath_exists(e->fts_path))
					del = add_to_del(del, &delsz,
					    e->fts_path);
				break;
			case FTS_D:
			case FTS_DP:
				/* TODO empty directory pruning */
				break;
			case FTS_SL:
			case FTS_SLNONE:
				warnx("symlink %s", e->fts_path);
				del = add_to_del(del, &delsz, e->fts_path);
				break;
			case FTS_NS:
			case FTS_ERR:
				warnx("fts_read %s: %s", e->fts_path,
				    strerror(e->fts_errno));
				break;
			default:
				warnx("unhandled[%x] %s", e->fts_info,
				    e->fts_path);
				break;
			}

			errno = 0;
		}
		if (errno)
			err(1, "fts_read");
		if (fts_close(fts) == -1)
			err(1, "fts_close");
	}

	for (i = 0; i < delsz; i++) {
		if (unlink(del[i]) == -1)
			warn("unlink %s", del[i]);
		if (verbose > 1)
			logx("deleted %s", del[i]);
		free(del[i]);
	}
	free(del);

	return delsz;
}

void
suicide(int sig __attribute__((unused)))
{
	killme = 1;

}

int
main(int argc, char *argv[])
{
	int		 rc = 1, c, proc, st, rsync,
			 fl = SOCK_STREAM | SOCK_CLOEXEC;
	size_t		 i, j, outsz = 0, talsz = 0;
	pid_t		 procpid, rsyncpid;
	int		 fd[2];
	struct entityq	 q;
	struct msgbuf	 procq, rsyncq;
	struct pollfd	 pfd[2];
	struct roa	**out = NULL;
	char		*rsync_prog = "openrsync";
	char		*bind_addr = NULL;
	const char	*cachedir = NULL, *errs;
	const char	*tals[TALSZ_MAX];
	struct vrp_tree	 v = RB_INITIALIZER(&v);
	struct rusage	ru;
	struct timeval	start_time, now_time;

	gettimeofday(&start_time, NULL);

	/* If started as root, priv-drop to _rpki-client */
	if (getuid() == 0) {
		struct passwd *pw;

		pw = getpwnam("_rpki-client");
		if (!pw)
			errx(1, "no _rpki-client user to revoke to");
		if (setgroups(1, &pw->pw_gid) == -1 ||
		    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) == -1 ||
		    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) == -1)
			err(1, "unable to revoke privs");

	}
	cachedir = RPKI_PATH_BASE_DIR;
	outputdir = RPKI_PATH_OUT_DIR;

	if (pledge("stdio rpath wpath cpath fattr proc exec unveil", NULL) == -1)
		err(1, "pledge");

	while ((c = getopt(argc, argv, "b:Bcd:e:jnos:t:T:v")) != -1)
		switch (c) {
		case 'b':
			bind_addr = optarg;
			break;
		case 'B':
			outformats |= FORMAT_BIRD;
			break;
		case 'c':
			outformats |= FORMAT_CSV;
			break;
		case 'd':
			cachedir = optarg;
			break;
		case 'e':
			rsync_prog = optarg;
			break;
		case 'j':
			outformats |= FORMAT_JSON;
			break;
		case 'n':
			noop = 1;
			break;
		case 'o':
			outformats |= FORMAT_OPENBGPD;
			break;
		case 's':
			timeout = strtonum(optarg, 0, 24*60*60, &errs);
			if (errs)
				errx(1, "-s: %s", errs);
			break;
		case 't':
			if (talsz >= TALSZ_MAX)
				err(1,
				    "too many tal files specified");
			tals[talsz++] = optarg;
			break;
		case 'T':
			bird_tablename = optarg;
			break;
		case 'v':
			verbose++;
			break;
		default:
			goto usage;
		}

	argv += optind;
	argc -= optind;
	if (argc == 1)
		outputdir = argv[0];
	else if (argc > 1)
		goto usage;

	if (timeout) {
		signal(SIGALRM, suicide);
		/* Commit suicide eventually - cron will normally start a new one */
		alarm(timeout);
	}

	if (cachedir == NULL) {
		warnx("cache directory required");
		goto usage;
	}
	if (outputdir == NULL) {
		warnx("output directory required");
		goto usage;
	}

	if (outformats == 0)
		outformats = FORMAT_OPENBGPD;

	if (talsz == 0)
		talsz = tal_load_default(tals, TALSZ_MAX);
	if (talsz == 0)
		err(1, "no TAL files found in %s", "/etc/rpki");

	TAILQ_INIT(&q);

	/*
	 * Create the file reader as a jailed child process.
	 * It will be responsible for reading all of the files (ROAs,
	 * manifests, certificates, etc.) and returning contents.
	 */

	if (socketpair(AF_UNIX, fl, 0, fd) == -1)
		err(1, "socketpair");
	if ((procpid = fork()) == -1)
		err(1, "fork");

	if (procpid == 0) {
		close(fd[1]);

		/* change working directory to the cache directory */
		if (chdir(cachedir) == -1)
			err(1, "%s: chdir", cachedir);

		/* Only allow access to the cache directory. */
		if (unveil(cachedir, "r") == -1)
			err(1, "%s: unveil", cachedir);
		if (pledge("stdio rpath", NULL) == -1)
			err(1, "pledge");
		proc_parser(fd[0]);
		/* NOTREACHED */
	}

	close(fd[0]);
	proc = fd[1];

	/*
	 * Create a process that will do the rsync'ing.
	 * This process is responsible for making sure that all the
	 * repositories referenced by a certificate manifest (or the
	 * TAL) exists and has been downloaded.
	 */

	if (!noop) {
		if (socketpair(AF_UNIX, fl, 0, fd) == -1)
			err(1, "socketpair");
		if ((rsyncpid = fork()) == -1)
			err(1, "fork");

		if (rsyncpid == 0) {
			close(proc);
			close(fd[1]);

			/* change working directory to the cache directory */
			if (chdir(cachedir) == -1)
				err(1, "%s: chdir", cachedir);

			if (pledge("stdio rpath cpath proc exec unveil", NULL)
			    == -1)
				err(1, "pledge");

			proc_rsync(rsync_prog, bind_addr, fd[0]);
			/* NOTREACHED */
		}

		close(fd[0]);
		rsync = fd[1];
	} else
		rsync = -1;

	assert(rsync != proc);

	if (pledge("stdio rpath wpath cpath fattr", NULL) == -1)
		err(1, "pledge");

	msgbuf_init(&procq);
	msgbuf_init(&rsyncq);
	procq.fd = proc;
	rsyncq.fd = rsync;

	/*
	 * The main process drives the top-down scan to leaf ROAs using
	 * data downloaded by the rsync process and parsed by the
	 * parsing process.
	 */

	pfd[0].fd = rsync;
	pfd[1].fd = proc;

	/*
	 * Prime the process with our TAL file.
	 * This will contain (hopefully) links to our manifest and we
	 * can get the ball rolling.
	 */

	for (i = 0; i < talsz; i++)
		queue_add_tal(&procq, &q, tals[i]);

	while (entity_queue > 0 && !killme) {
		pfd[0].events = POLLIN;
		if (rsyncq.queued)
			pfd[0].events = POLLOUT;
		pfd[1].events = POLLIN;
		if (procq.queued)
			pfd[1].events = POLLOUT;

		if ((c = poll(pfd, 2, verbose ? 10000 : INFTIM)) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}

		/* Debugging: print some statistics if we stall. */

		if (c == 0) {
			for (i = j = 0; i < rt.reposz; i++)
				if (!rt.repos[i].loaded) {
					logx("pending repo %s",
					    rt.repos[i].local);
					j++;
				}
			logx("period stats: %zu pending repos", j);
			logx("period stats: %zu pending entries", entity_queue);
			continue;
		}

		if ((pfd[0].revents & (POLLERR|POLLNVAL)) ||
		    (pfd[1].revents & (POLLERR|POLLNVAL)))
			errx(1, "poll: bad fd");
		if ((pfd[0].revents & POLLHUP) ||
		    (pfd[1].revents & POLLHUP))
			errx(1, "poll: hangup");

		if (pfd[0].revents & POLLOUT) {
			switch (msgbuf_write(&rsyncq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}
		if (pfd[1].revents & POLLOUT) {
			switch (msgbuf_write(&procq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}

		/*
		 * Check the rsync process.
		 * This means that one of our modules has completed
		 * downloading and we can flush the module requests into
		 * the parser process.
		 */

		if ((pfd[0].revents & POLLIN)) {
			int ok;
			io_simple_read(rsync, &i, sizeof(size_t));
			io_simple_read(rsync, &ok, sizeof(ok));
			assert(i < rt.reposz);

			assert(!rt.repos[i].loaded);
			rt.repos[i].loaded = 1;
			if (ok)
				logx("%s: loaded from network",
				    rt.repos[i].local);
			else
				logx("%s: load from network failed, "
				    "fallback to cache", rt.repos[i].local);
			stats.repos++;
			entityq_flush(&procq, &q, &rt.repos[i]);
		}

		/*
		 * The parser has finished something for us.
		 * Dequeue these one by one.
		 */

		if ((pfd[1].revents & POLLIN)) {
			entity_process(proc, &procq, &rsyncq, &stats, &q, &v);
		}
	}

	if (killme) {
		syslog(LOG_CRIT|LOG_DAEMON,
		    "excessive runtime (%d seconds), giving up", timeout);
		errx(1, "excessive runtime (%d seconds), giving up", timeout);
	}

	assert(TAILQ_EMPTY(&q));
	logx("all files parsed: generating output");
	rc = 0;

	/*
	 * For clean-up, close the input for the parser and rsync
	 * process.
	 * This will cause them to exit, then we reap them.
	 */

	close(proc);
	close(rsync);

	if (waitpid(procpid, &st, 0) == -1)
		err(1, "waitpid");
	if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
		warnx("parser process exited abnormally");
		rc = 1;
	}
	if (!noop) {
		if (waitpid(rsyncpid, &st, 0) == -1)
			err(1, "waitpid");
		if (!WIFEXITED(st) || WEXITSTATUS(st) != 0) {
			warnx("rsync process exited abnormally");
			rc = 1;
		}
	}
	gettimeofday(&now_time, NULL);
	timersub(&now_time, &start_time, &stats.elapsed_time);
	if (getrusage(RUSAGE_SELF, &ru) == 0) {
		stats.user_time = ru.ru_utime;
		stats.system_time = ru.ru_stime;
	}
	if (getrusage(RUSAGE_CHILDREN, &ru) == 0) {
		timeradd(&stats.user_time, &ru.ru_utime, &stats.user_time);
		timeradd(&stats.system_time, &ru.ru_stime, &stats.system_time);
	}

	if (outputfiles(&v, &stats))
		rc = 1;

	stats.del_files = repo_cleanup(cachedir);

	logx("Route Origin Authorizations: %zu (%zu failed parse, %zu invalid)",
	    stats.roas, stats.roas_fail, stats.roas_invalid);
	logx("Certificates: %zu (%zu failed parse, %zu invalid)",
	    stats.certs, stats.certs_fail, stats.certs_invalid);
	logx("Trust Anchor Locators: %zu", stats.tals);
	logx("Manifests: %zu (%zu failed parse, %zu stale)",
	    stats.mfts, stats.mfts_fail, stats.mfts_stale);
	logx("Certificate revocation lists: %zu", stats.crls);
	logx("Ghostbuster records: %zu", stats.gbrs);
	logx("Repositories: %zu", stats.repos);
	logx("Files removed: %zu", stats.del_files);
	logx("VRP Entries: %zu (%zu unique)", stats.vrps, stats.uniqs);

	/* Memory cleanup. */
	for (i = 0; i < rt.reposz; i++) {
		free(rt.repos[i].local);
		free(rt.repos[i].repo);
	}
	free(rt.repos);

	for (i = 0; i < outsz; i++)
		roa_free(out[i]);
	free(out);

	return rc;

usage:
	fprintf(stderr,
	    "usage: rpki-client [-Bcjnov] [-b sourceaddr] [-d cachedir]"
	    " [-e rsync_prog]\n"
	    "                   [-s timeout] [-T table] [-t tal] [outputdir]\n");
	return 1;
}
