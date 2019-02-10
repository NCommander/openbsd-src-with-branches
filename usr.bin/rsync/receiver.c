/*	$Id$ */

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
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "extern.h"

enum	pfdt {
	PFD_SENDER_IN = 0, /* input from the sender */
	PFD_UPLOADER_IN, /* uploader input from a local file */
	PFD_DOWNLOADER_IN, /* downloader input from a local file */
	PFD_SENDER_OUT, /* output to the sender */
	PFD__MAX
};

/*
 * Pledges: unveil, rpath, cpath, wpath, stdio, fattr.
 * Pledges (dry-run): -cpath, -wpath, -fattr.
 */
int
rsync_receiver(struct sess *sess,
	int fdin, int fdout, const char *root)
{
	struct flist	*fl = NULL, *dfl = NULL;
	size_t		 i, flsz = 0, dflsz = 0, excl;
	char		*tofree;
	int		 rc = 0, dfd = -1, phase = 0, c;
	int32_t		 ioerror;
	struct pollfd	 pfd[PFD__MAX];
	struct download	*dl = NULL;
	struct upload	*ul = NULL;
	mode_t		 oumask;

	if (-1 == pledge("unveil rpath cpath wpath stdio fattr", NULL)) {
		ERR(sess, "pledge");
		goto out;
	}

	/* Client sends zero-length exclusions. */

	if ( ! sess->opts->server &&
	     ! io_write_int(sess, fdout, 0)) {
		ERRX1(sess, "io_write_int");
		goto out;
	}

	if (sess->opts->server && sess->opts->del) {
		if ( ! io_read_size(sess, fdin, &excl)) {
			ERRX1(sess, "io_read_size");
			goto out;
		} else if (0 != excl) {
			ERRX(sess, "exclusion list is non-empty");
			goto out;
		}
	}

	/*
	 * Start by receiving the file list and our mystery number.
	 * These we're going to be touching on our local system.
	 */

	if ( ! flist_recv(sess, fdin, &fl, &flsz)) {
		ERRX1(sess, "flist_recv");
		goto out;
	}

	/* The IO error is sent after the file list. */

	if ( ! io_read_int(sess, fdin, &ioerror)) {
		ERRX1(sess, "io_read_int");
		goto out;
	} else if (0 != ioerror) {
		ERRX1(sess, "io_error is non-zero");
		goto out;
	}

	if (0 == flsz && ! sess->opts->server) {
		WARNX(sess, "receiver has empty file list: exiting");
		rc = 1;
		goto out;
	} else if ( ! sess->opts->server)
		LOG1(sess, "Transfer starting: %zu files", flsz);

	LOG2(sess, "%s: receiver destination", root);

	/*
	 * Create the path for our destination directory, if we're not
	 * in dry-run mode (which would otherwise crash w/the pledge).
	 * This uses our current umask: we might set the permissions on
	 * this directory in post_dir().
	 */

	if ( ! sess->opts->dry_run) {
		if (NULL == (tofree = strdup(root))) {
			ERR(sess, "strdup");
			goto out;
		} else if (mkpath(sess, tofree) < 0) {
			ERRX1(sess, "%s: mkpath", root);
			free(tofree);
			goto out;
		}
		free(tofree);
	}

	/*
	 * Disable umask() so we can set permissions fully.
	 * Then open the directory iff we're not in dry_run.
	 */

	oumask = umask(0);

	if ( ! sess->opts->dry_run) {
		dfd = open(root, O_RDONLY | O_DIRECTORY, 0);
		if (-1 == dfd) {
			ERR(sess, "%s: open", root);
			goto out;
		}
	}

	/*
	 * Begin by conditionally getting all files we have currently
	 * available in our destination.
	 * XXX: THIS IS A BUG IN OPENBSD 6.4.
	 * For newer version of OpenBSD, this is safe to put after the
	 * unveil.
	 */

	if (sess->opts->del &&
	    sess->opts->recursive &&
	    ! flist_gen_dels(sess, root, &dfl, &dflsz, fl, flsz)) {
		ERRX1(sess, "flist_gen_local");
		goto out;
	}

	/*
	 * Make our entire view of the file-system be limited to what's
	 * in the root directory.
	 * This prevents us from accidentally (or "under the influence")
	 * writing into other parts of the file-system.
	 */

	if (-1 == unveil(root, "rwc")) {
		ERR(sess, "%s: unveil", root);
		goto out;
	} else if (-1 == unveil(NULL, NULL)) {
		ERR(sess, "%s: unveil", root);
		goto out;
	}

	/* If we have a local set, go for the deletion. */

	if ( ! flist_del(sess, dfd, dfl, dflsz)) {
		ERRX1(sess, "flist_del");
		goto out;
	}

	/* Initialise poll events to listen from the sender. */

	pfd[PFD_SENDER_IN].fd = fdin;
	pfd[PFD_UPLOADER_IN].fd = -1;
	pfd[PFD_DOWNLOADER_IN].fd = -1;
	pfd[PFD_SENDER_OUT].fd = fdout;

	pfd[PFD_SENDER_IN].events = POLLIN;
	pfd[PFD_UPLOADER_IN].events = POLLIN;
	pfd[PFD_DOWNLOADER_IN].events = POLLIN;
	pfd[PFD_SENDER_OUT].events = POLLOUT;

	ul = upload_alloc(sess, dfd, fdout,
		CSUM_LENGTH_PHASE1, fl, flsz, oumask);
	if (NULL == ul) {
		ERRX1(sess, "upload_alloc");
		goto out;
	}

	dl = download_alloc(sess, fdin, fl, flsz, dfd);
	if (NULL == dl) {
		ERRX1(sess, "download_alloc");
		goto out;
	}

	LOG2(sess, "%s: ready for phase 1 data", root);

	for (;;) {
		if (-1 == (c = poll(pfd, PFD__MAX, INFTIM))) {
			ERR(sess, "poll");
			goto out;
		}

		for (i = 0; i < PFD__MAX; i++)
			if (pfd[i].revents & (POLLERR|POLLNVAL)) {
				ERRX(sess, "poll: bad fd");
				goto out;
			} else if (pfd[i].revents & POLLHUP) {
				ERRX(sess, "poll: hangup");
				goto out;
			}

		/*
		 * If we have a read event and we're multiplexing, we
		 * might just have error messages in the pipe.
		 * It's important to flush these out so that we don't
		 * clog the pipe.
		 * Unset our polling status if there's nothing that
		 * remains in the pipe.
		 */

		if (sess->mplex_reads &&
		    (POLLIN & pfd[PFD_SENDER_IN].revents)) {
			if ( ! io_read_flush(sess, fdin)) {
				ERRX1(sess, "io_read_flush");
				goto out;
			} else if (0 == sess->mplex_read_remain)
				pfd[PFD_SENDER_IN].revents &= ~POLLIN;
		}


		/*
		 * We run the uploader if we have files left to examine
		 * (i < flsz) or if we have a file that we've opened and
		 * is read to mmap.
		 */

		if ((POLLIN & pfd[PFD_UPLOADER_IN].revents) ||
		    (POLLOUT & pfd[PFD_SENDER_OUT].revents)) {
			c = rsync_uploader(ul,
				&pfd[PFD_UPLOADER_IN].fd,
				sess, &pfd[PFD_SENDER_OUT].fd);
			if (c < 0) {
				ERRX1(sess, "rsync_uploader");
				goto out;
			}
		}

		/*
		 * We need to run the downloader when we either have
		 * read events from the sender or an asynchronous local
		 * open is ready.
		 * XXX: we don't disable PFD_SENDER_IN like with the
		 * uploader because we might stop getting error
		 * messages, which will otherwise clog up the pipes.
		 */

		if ((POLLIN & pfd[PFD_SENDER_IN].revents) ||
		    (POLLIN & pfd[PFD_DOWNLOADER_IN].revents)) {
			c = rsync_downloader(dl, sess,
				&pfd[PFD_DOWNLOADER_IN].fd);
			if (c < 0) {
				ERRX1(sess, "rsync_downloader");
				goto out;
			} else if (0 == c) {
				assert(0 == phase);
				phase++;
				LOG2(sess, "%s: receiver ready "
					"for phase 2 data", root);
				break;
			}

			/*
			 * FIXME: if we have any errors during the
			 * download, most notably files getting out of
			 * sync between the send and the receiver, then
			 * here we should bump our checksum length and
			 * go into the second phase.
			 */
		}
	}

	/* Properly close us out by progressing through the phases. */

	if (1 == phase) {
		if ( ! io_write_int(sess, fdout, -1)) {
			ERRX1(sess, "io_write_int");
			goto out;
		} else if ( ! io_read_int(sess, fdin, &ioerror)) {
			ERRX1(sess, "io_read_int");
			goto out;
		} else if (-1 != ioerror) {
			ERRX(sess, "expected phase ack");
			goto out;
		}
	}

	/*
	 * Now all of our transfers are complete, so we can fix up our
	 * directory permissions.
	 */

	if ( ! rsync_uploader_tail(ul, sess)) {
		ERRX1(sess, "rsync_uploader_tail");
		goto out;
	}

	/* Process server statistics and say good-bye. */

	if ( ! sess_stats_recv(sess, fdin)) {
		ERRX1(sess, "sess_stats_recv");
		goto out;
	} else if ( ! io_write_int(sess, fdout, -1)) {
		ERRX1(sess, "io_write_int");
		goto out;
	}

	LOG2(sess, "receiver finished updating");
	rc = 1;
out:
	if (-1 != dfd)
		close(dfd);
	upload_free(ul);
	download_free(dl);
	flist_free(fl, flsz);
	flist_free(dfl, dflsz);
	return rc;
}
