/*	$Id: main.c,v 1.16 2019/02/14 18:32:00 florian Exp $ */
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <assert.h>
#include <err.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static void
fargs_free(struct fargs *p)
{
	size_t	 i;

	if (p == NULL)
		return;

	if (p->sources != NULL)
		for (i = 0; i < p->sourcesz; i++)
			free(p->sources[i]);

	free(p->sources);
	free(p->sink);
	free(p->host);
	free(p);
}

/*
 * A remote host is has a colon before the first path separator.
 * This works for rsh remote hosts (host:/foo/bar), implicit rsync
 * remote hosts (host::/foo/bar), and explicit (rsync://host/foo).
 * Return zero if local, non-zero if remote.
 */
static int
fargs_is_remote(const char *v)
{
	size_t	 pos;

	pos = strcspn(v, ":/");
	return v[pos] == ':';
}

/*
 * Test whether a remote host is specifically an rsync daemon.
 * Return zero if not, non-zero if so.
 */
static int
fargs_is_daemon(const char *v)
{
	size_t	 pos;

	if (strncasecmp(v, "rsync://", 8) == 0)
		return 1;

	pos = strcspn(v, ":/");
	return v[pos] == ':' && v[pos + 1] == ':';
}

/*
 * Take the command-line filenames (e.g., rsync foo/ bar/ baz/) and
 * determine our operating mode.
 * For example, if the first argument is a remote file, this means that
 * we're going to transfer from the remote to the local.
 * We also make sure that the arguments are consistent, that is, if
 * we're going to transfer from the local to the remote, that no
 * filenames for the local transfer indicate remote hosts.
 * Always returns the parsed and sanitised options.
 */
static struct fargs *
fargs_parse(size_t argc, char *argv[])
{
	struct fargs	*f = NULL;
	char		*cp;
	size_t		 i, j, len = 0;

	/* Allocations. */

	if ((f = calloc(1, sizeof(struct fargs))) == NULL)
		err(EXIT_FAILURE, "calloc");

	f->sourcesz = argc - 1;
	if ((f->sources = calloc(f->sourcesz, sizeof(char *))) == NULL)
		err(EXIT_FAILURE, "calloc");

	for (i = 0; i < argc - 1; i++)
		if ((f->sources[i] = strdup(argv[i])) == NULL)
			err(EXIT_FAILURE, "strdup");

	if ((f->sink = strdup(argv[i])) == NULL)
		err(EXIT_FAILURE, "strdup");

	/*
	 * Test files for its locality.
	 * If the last is a remote host, then we're sending from the
	 * local to the remote host ("sender" mode).
	 * If the first, remote to local ("receiver" mode).
	 * If neither, a local transfer in sender style.
	 */

	f->mode = FARGS_SENDER;

	if (fargs_is_remote(f->sink)) {
		f->mode = FARGS_SENDER;
		if ((f->host = strdup(f->sink)) == NULL)
			err(EXIT_FAILURE, "strdup");
	}

	if (fargs_is_remote(f->sources[0])) {
		if (f->host != NULL)
			errx(EXIT_FAILURE, "both source and "
				"destination cannot be remote files");
		f->mode = FARGS_RECEIVER;
		if ((f->host = strdup(f->sources[0])) == NULL)
			err(EXIT_FAILURE, "strdup");
	}

	if (f->host != NULL) {
		if (strncasecmp(f->host, "rsync://", 8) == 0) {
			/* rsync://host/module[/path] */
			f->remote = 1;
			len = strlen(f->host) - 8 + 1;
			memmove(f->host, f->host + 8, len);
			if ((cp = strchr(f->host, '/')) == NULL)
				errx(EXIT_FAILURE, "rsync protocol "
					"requires a module name");
			*cp++ = '\0';
			f->module = cp;
			if ((cp = strchr(f->module, '/')) != NULL)
				*cp = '\0';
		} else {
			/* host:[/path] */
			cp = strchr(f->host, ':');
			assert(cp != NULL);
			*cp++ = '\0';
			if (*cp == ':') {
				/* host::module[/path] */
				f->remote = 1;
				f->module = ++cp;
				cp = strchr(f->module, '/');
				if (cp != NULL)
					*cp = '\0';
			}
		}
		if ((len = strlen(f->host)) == 0)
			errx(EXIT_FAILURE, "empty remote host");
		if (f->remote && strlen(f->module) == 0)
			errx(EXIT_FAILURE, "empty remote module");
	}

	/* Make sure we have the same "hostspec" for all files. */

	if (!f->remote) {
		if (f->mode == FARGS_SENDER)
			for (i = 0; i < f->sourcesz; i++) {
				if (!fargs_is_remote(f->sources[i]))
					continue;
				errx(EXIT_FAILURE, "remote file in "
					"list of local sources: %s",
					f->sources[i]);
			}
		if (f->mode == FARGS_RECEIVER)
			for (i = 0; i < f->sourcesz; i++) {
				if (fargs_is_remote(f->sources[i]) &&
				    !fargs_is_daemon(f->sources[i]))
					continue;
				if (fargs_is_daemon(f->sources[i]))
					errx(EXIT_FAILURE, "remote "
						"daemon in list of "
						"remote sources: %s",
						f->sources[i]);
				errx(EXIT_FAILURE, "local file in "
					"list of remote sources: %s",
					f->sources[i]);
			}
	} else {
		if (f->mode != FARGS_RECEIVER)
			errx(EXIT_FAILURE, "sender mode for remote "
				"daemon receivers not yet supported");
		for (i = 0; i < f->sourcesz; i++) {
			if (fargs_is_daemon(f->sources[i]))
				continue;
			errx(EXIT_FAILURE, "non-remote daemon file "
				"in list of remote daemon sources: "
				"%s", f->sources[i]);
		}
	}

	/*
	 * If we're not remote and a sender, strip our hostname.
	 * Then exit if we're a sender or a local connection.
	 */

	if (!f->remote) {
		if (f->host == NULL)
			return f;
		if (f->mode == FARGS_SENDER) {
			assert(f->host != NULL);
			assert(len > 0);
			j = strlen(f->sink);
			memmove(f->sink, f->sink + len + 1, j - len);
			return f;
		} else if (f->mode != FARGS_RECEIVER)
			return f;
	}

	/*
	 * Now strip the hostnames from the remote host.
	 *   rsync://host/module/path -> module/path
	 *   host::module/path -> module/path
	 *   host:path -> path
	 * Also make sure that the remote hosts are the same.
	 */

	assert(f->host != NULL);
	assert(len > 0);

	for (i = 0; i < f->sourcesz; i++) {
		cp = f->sources[i];
		j = strlen(cp);
		if (f->remote &&
		    strncasecmp(cp, "rsync://", 8) == 0) {
			/* rsync://path */
			cp += 8;
			if (strncmp(cp, f->host, len) ||
			    (cp[len] != '/' && cp[len] != '\0'))
				errx(EXIT_FAILURE, "different remote "
					"host: %s", f->sources[i]);
			memmove(f->sources[i],
				f->sources[i] + len + 8 + 1,
				j - len - 8);
		} else if (f->remote && strncmp(cp, "::", 2) == 0) {
			/* ::path */
			memmove(f->sources[i],
				f->sources[i] + 2, j - 1);
		} else if (f->remote) {
			/* host::path */
			if (strncmp(cp, f->host, len) ||
			    (cp[len] != ':' && cp[len] != '\0'))
				errx(EXIT_FAILURE, "different remote "
					"host: %s", f->sources[i]);
			memmove(f->sources[i], f->sources[i] + len + 2,
			    j - len - 1);
		} else if (cp[0] == ':') {
			/* :path */
			memmove(f->sources[i], f->sources[i] + 1, j);
		} else {
			/* host:path */
			if (strncmp(cp, f->host, len) ||
			    (cp[len] != ':' && cp[len] != '\0'))
				errx(EXIT_FAILURE, "different remote "
					"host: %s", f->sources[i]);
			memmove(f->sources[i],
				f->sources[i] + len + 1, j - len);
		}
	}

	return f;
}

int
main(int argc, char *argv[])
{
	struct opts	 opts;
	pid_t		 child;
	int		 fds[2], c, st;
	struct fargs	*fargs;
	struct option	 lopts[] = {
		{ "delete",	no_argument,	&opts.del,		1 },
		{ "rsync-path",	required_argument, NULL,		1 },
		{ "rsh",	required_argument, NULL,		'e' },
		{ "sender",	no_argument,	&opts.sender,		1 },
		{ "server",	no_argument,	&opts.server,		1 },
		{ "verbose",	no_argument,	&opts.verbose,		1 },
		{ "links",	no_argument,	&opts.preserve_links,	1 },
		{ "dry-run",	no_argument,	&opts.dry_run,		1 },
		{ "perms",	no_argument,	&opts.preserve_perms,	1 },
		{ "recursive",	no_argument,	&opts.recursive,	1 },
		{ "times",	no_argument,	&opts.preserve_times,	1 },
		{ "group",	no_argument,	&opts.preserve_gids,	1 },
		{ "version",	no_argument,	NULL,			2 },
		{ NULL,		0,		NULL,			0 }};

	/* Global pledge. */

	if (pledge("stdio rpath wpath cpath inet fattr dns getpw proc exec unveil",
	    NULL) == -1)
		err(EXIT_FAILURE, "pledge");

	memset(&opts, 0, sizeof(struct opts));

	while ((c = getopt_long(argc, argv, "e:glnoprtv", lopts, NULL)) != -1) {
		switch (c) {
		case 'e':
			opts.ssh_prog = optarg;
			/* Ignore. */
			break;
		case 'g':
			opts.preserve_gids = 1;
			break;
		case 'l':
			opts.preserve_links = 1;
			break;
		case 'n':
			opts.dry_run = 1;
			break;
		case 'o':
			opts.preserve_uids = 1;
			break;
		case 'p':
			opts.preserve_perms = 1;
			break;
		case 'r':
			opts.recursive = 1;
			break;
		case 't':
			opts.preserve_times = 1;
			break;
		case 'v':
			opts.verbose++;
			break;
		case 0:
			/* Non-NULL flag values (e.g., --sender). */
			break;
		case 1:
			opts.rsync_path = optarg;
			break;
		case 2:
			fprintf(stderr, "openrsync: protocol version %u\n",
			    RSYNC_PROTOCOL);
			exit(0);
		default:
			goto usage;
		}
	}

	argc -= optind;
	argv += optind;

	/* FIXME: reference implementation rsync accepts this. */

	if (argc < 2)
		goto usage;

	/*
	 * This is what happens when we're started with the "hidden"
	 * --server option, which is invoked for the rsync on the remote
	 * host by the parent.
	 */

	if (opts.server) {
		if (pledge("stdio rpath wpath cpath fattr getpw unveil", NULL) == -1)
			err(EXIT_FAILURE, "pledge");
		c = rsync_server(&opts, (size_t)argc, argv);
		return c ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	/*
	 * Now we know that we're the client on the local machine
	 * invoking rsync(1).
	 * At this point, we need to start the client and server
	 * initiation logic.
	 * The client is what we continue running on this host; the
	 * server is what we'll use to connect to the remote and
	 * invoke rsync with the --server option.
	 */

	fargs = fargs_parse(argc, argv);
	assert(fargs != NULL);

	/*
	 * If we're contacting an rsync:// daemon, then we don't need to
	 * fork, because we won't start a server ourselves.
	 * Route directly into the socket code, in that case.
	 */

	if (fargs->remote) {
		assert(fargs->mode == FARGS_RECEIVER);
		if (pledge("stdio rpath wpath cpath inet fattr dns getpw unveil",
		    NULL) == -1)
			err(EXIT_FAILURE, "pledge");
		c = rsync_socket(&opts, fargs);
		fargs_free(fargs);
		return c ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	/* Drop the dns/inet possibility. */

	if (pledge("stdio rpath wpath cpath fattr getpw proc exec unveil",
	    NULL) == -1)
		err(EXIT_FAILURE, "pledge");

	/* Create a bidirectional socket and start our child. */

	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, fds) == -1)
		err(EXIT_FAILURE, "socketpair");

	if ((child = fork()) == -1) {
		close(fds[0]);
		close(fds[1]);
		err(EXIT_FAILURE, "fork");
	}

	/* Drop the fork possibility. */

	if (pledge("stdio rpath wpath cpath fattr getpw exec unveil", NULL) == -1)
		err(EXIT_FAILURE, "pledge");

	if (child == 0) {
		close(fds[0]);
		fds[0] = -1;
		if (pledge("stdio exec", NULL) == -1)
			err(EXIT_FAILURE, "pledge");
		rsync_child(&opts, fds[1], fargs);
		/* NOTREACHED */
	}

	close(fds[1]);
	fds[1] = -1;
	if (pledge("stdio rpath wpath cpath fattr getpw unveil", NULL) == -1)
		err(EXIT_FAILURE, "pledge");
	c = rsync_client(&opts, fds[0], fargs);
	fargs_free(fargs);

	/*
	 * If the client has an error and exits, the server may be
	 * sitting around waiting to get data while we waitpid().
	 * So close the connection here so that they don't hang.
	 */

	if (!c) {
		close(fds[0]);
		fds[0] = -1;
	}

	if (waitpid(child, &st, 0) == -1)
		err(EXIT_FAILURE, "waitpid");
	if (!(WIFEXITED(st) && WEXITSTATUS(st) == EXIT_SUCCESS))
		c = 0;

	if (fds[0] != -1)
		close(fds[0]);
	return c ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	fprintf(stderr, "usage: %s [-glnoprtv] "
		"[-e ssh-prog] [--delete] [--rsync-path=prog] src ... dst\n",
		getprogname());
	return EXIT_FAILURE;
}
