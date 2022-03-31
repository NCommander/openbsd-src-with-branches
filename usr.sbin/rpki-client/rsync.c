/*	$OpenBSD: rsync.c,v 1.32 2022/01/13 11:50:29 claudio Exp $ */
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

#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <poll.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <imsg.h>

#include "extern.h"

#define	__STRINGIFY(x)	#x
#define	STRINGIFY(x)	__STRINGIFY(x)

/*
 * A running rsync process.
 * We can have multiple of these simultaneously and need to keep track
 * of which process maps to which request.
 */
struct	rsyncproc {
	char		*uri; /* uri of this rsync proc */
	unsigned int	 id; /* identity of request */
	pid_t	 	 pid; /* pid of process or 0 if unassociated */
};

/*
 * Return the base of a rsync URI (rsync://hostname/module). The
 * caRepository provided by the RIR CAs point deeper than they should
 * which would result in many rsync calls for almost every subdirectory.
 * This is inefficent so instead crop the URI to a common base.
 * The returned string needs to be freed by the caller.
 */
char *
rsync_base_uri(const char *uri)
{
	const char *host, *module, *rest;
	char *base_uri;

	/* Case-insensitive rsync URI. */
	if (strncasecmp(uri, "rsync://", 8) != 0) {
		warnx("%s: not using rsync schema", uri);
		return NULL;
	}

	/* Parse the non-zero-length hostname. */
	host = uri + 8;

	if ((module = strchr(host, '/')) == NULL) {
		warnx("%s: missing rsync module", uri);
		return NULL;
	} else if (module == host) {
		warnx("%s: zero-length rsync host", uri);
		return NULL;
	}

	/* The non-zero-length module follows the hostname. */
	module++;
	if (*module == '\0') {
		warnx("%s: zero-length rsync module", uri);
		return NULL;
	}

	/* The path component is optional. */
	if ((rest = strchr(module, '/')) == NULL) {
		if ((base_uri = strdup(uri)) == NULL)
			err(1, NULL);
		return base_uri;
	} else if (rest == module) {
		warnx("%s: zero-length module", uri);
		return NULL;
	}

	if ((base_uri = strndup(uri, rest - uri)) == NULL)
		err(1, NULL);
	return base_uri;
}

/*
 * The directory passed as --compare-dest needs to be relative to
 * the destination directory. This function takes care of that.
 */
static char *
rsync_fixup_dest(char *destdir, char *compdir)
{
	const char *dotdot = "../../../../../../";	/* should be enough */
	int dirs = 1;
	char *fn;
	char c;

	while ((c = *destdir++) != '\0')
		if (c == '/')
			dirs++;

	if (dirs > 6)
		/* too deep for us */
		return NULL;

	if ((asprintf(&fn, "%.*s%s", dirs * 3, dotdot, compdir)) == -1)
		err(1, NULL);
	return fn;
}

static void
proc_child(int signal)
{

	/* Nothing: just discard. */
}

/*
 * Process used for synchronising repositories.
 * This simply waits to be told which repository to synchronise, then
 * does so.
 * It then responds with the identifier of the repo that it updated.
 * It only exits cleanly when fd is closed.
 * FIXME: limit the number of simultaneous process.
 * Currently, an attacker can trivially specify thousands of different
 * repositories and saturate our system.
 */
void
proc_rsync(char *prog, char *bind_addr, int fd)
{
	size_t			 i, idsz = 0, nprocs = 0;
	int			 rc = 0;
	struct pollfd		 pfd;
	struct msgbuf		 msgq;
	struct ibuf		*b, *inbuf = NULL;
	sigset_t		 mask, oldmask;
	struct rsyncproc	*ids = NULL;

	pfd.fd = fd;

	msgbuf_init(&msgq);
	msgq.fd = fd;

	/*
	 * Unveil the command we want to run.
	 * If this has a pathname component in it, interpret as a file
	 * and unveil the file directly.
	 * Otherwise, look up the command in our PATH.
	 */

	if (strchr(prog, '/') == NULL) {
		const char *pp;
		char *save, *cmd, *path;
		struct stat stt;

		if (getenv("PATH") == NULL)
			errx(1, "PATH is unset");
		if ((path = strdup(getenv("PATH"))) == NULL)
			err(1, NULL);
		save = path;
		while ((pp = strsep(&path, ":")) != NULL) {
			if (*pp == '\0')
				continue;
			if (asprintf(&cmd, "%s/%s", pp, prog) == -1)
				err(1, NULL);
			if (lstat(cmd, &stt) == -1) {
				free(cmd);
				continue;
			} else if (unveil(cmd, "x") == -1)
				err(1, "%s: unveil", cmd);
			free(cmd);
			break;
		}
		free(save);
	} else if (unveil(prog, "x") == -1)
		err(1, "%s: unveil", prog);

	if (pledge("stdio proc exec", NULL) == -1)
		err(1, "pledge");

	/* Initialise retriever for children exiting. */

	if (sigemptyset(&mask) == -1)
		err(1, NULL);
	if (signal(SIGCHLD, proc_child) == SIG_ERR)
		err(1, NULL);
	if (sigaddset(&mask, SIGCHLD) == -1)
		err(1, NULL);
	if (sigprocmask(SIG_BLOCK, &mask, &oldmask) == -1)
		err(1, NULL);

	for (;;) {
		char *uri, *dst, *compdst;
		unsigned int id;
		pid_t pid;
		int st;

		pfd.events = 0;
		if (nprocs < MAX_RSYNC_PROCESSES)
			pfd.events |= POLLIN;
		if (msgq.queued)
			pfd.events |= POLLOUT;

		if (ppoll(&pfd, 1, NULL, &oldmask) == -1) {
			if (errno != EINTR)
				err(1, "ppoll");

			/*
			 * If we've received an EINTR, it means that one
			 * of our children has exited and we can reap it
			 * and look up its identifier.
			 * Then we respond to the parent.
			 */

			while ((pid = waitpid(WAIT_ANY, &st, WNOHANG)) > 0) {
				int ok = 1;

				for (i = 0; i < idsz; i++)
					if (ids[i].pid == pid)
						break;
				if (i >= idsz)
					errx(1, "waitpid: %d unexpected", pid);

				if (!WIFEXITED(st)) {
					warnx("rsync %s terminated abnormally",
					    ids[i].uri);
					rc = 1;
					ok = 0;
				} else if (WEXITSTATUS(st) != 0) {
					warnx("rsync %s failed", ids[i].uri);
					ok = 0;
				}

				b = io_new_buffer();
				io_simple_buffer(b, &ids[i].id,
				     sizeof(ids[i].id));
				io_simple_buffer(b, &ok, sizeof(ok));
				io_close_buffer(&msgq, b);

				free(ids[i].uri);
				ids[i].uri = NULL;
				ids[i].pid = 0;
				ids[i].id = 0;
				nprocs--;
			}
			if (pid == -1 && errno != ECHILD)
				err(1, "waitpid");
			continue;
		}

		if (pfd.revents & POLLOUT) {
			switch (msgbuf_write(&msgq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
		}

		/* connection closed */
		if (pfd.revents & POLLHUP)
			break;

		if (!(pfd.revents & POLLIN))
			continue;

		b = io_buf_read(fd, &inbuf);
		if (b == NULL)
			continue;

		/* Read host and module. */
		io_read_buf(b, &id, sizeof(id));
		io_read_str(b, &dst);
		io_read_str(b, &compdst);
		io_read_str(b, &uri);

		ibuf_free(b);

		assert(dst);
		assert(uri);

		/* Run process itself, wait for exit, check error. */

		if ((pid = fork()) == -1)
			err(1, "fork");

		if (pid == 0) {
			char *args[32];
			char *reldst;

			if (pledge("stdio exec", NULL) == -1)
				err(1, "pledge");
			i = 0;
			args[i++] = (char *)prog;
			args[i++] = "-rt";
			args[i++] = "--no-motd";
			args[i++] = "--max-size=" STRINGIFY(MAX_FILE_SIZE);
			args[i++] = "--timeout=180";
			args[i++] = "--include=*/";
			args[i++] = "--include=*.cer";
			args[i++] = "--include=*.crl";
			args[i++] = "--include=*.gbr";
			args[i++] = "--include=*.mft";
			args[i++] = "--include=*.roa";
			args[i++] = "--include=*.asa";
			args[i++] = "--exclude=*";
			if (bind_addr != NULL) {
				args[i++] = "--address";
				args[i++] = (char *)bind_addr;
			}
			if (compdst != NULL &&
			    (reldst = rsync_fixup_dest(dst, compdst)) != NULL) {
				args[i++] = "--compare-dest";
				args[i++] = reldst;
			}
			args[i++] = uri;
			args[i++] = dst;
			args[i] = NULL;
			/* XXX args overflow not prevented */
			execvp(args[0], args);
			err(1, "%s: execvp", prog);
		}

		/* Augment the list of running processes. */

		for (i = 0; i < idsz; i++)
			if (ids[i].pid == 0)
				break;
		if (i == idsz) {
			ids = reallocarray(ids, idsz + 1, sizeof(*ids));
			if (ids == NULL)
				err(1, NULL);
			idsz++;
		}

		ids[i].id = id;
		ids[i].pid = pid;
		ids[i].uri = uri;
		nprocs++;

		/* Clean up temporary values. */

		free(dst);
		free(compdst);
	}

	/* No need for these to be hanging around. */
	for (i = 0; i < idsz; i++)
		if (ids[i].pid > 0) {
			kill(ids[i].pid, SIGTERM);
			free(ids[i].uri);
		}

	msgbuf_clear(&msgq);
	free(ids);
	exit(rc);
}
