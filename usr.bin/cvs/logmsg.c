/*	$OpenBSD: logmsg.c,v 1.27 2006/01/02 08:11:56 xsa Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include "buf.h"
#include "cvs.h"
#include "log.h"
#include "proto.h"


#define CVS_LOGMSG_BIGMSG	32000
#define CVS_LOGMSG_PREFIX	"CVS:"
#define CVS_LOGMSG_LINE \
"----------------------------------------------------------------------"


static const char *cvs_logmsg_ops[3] = {
	"Added", "Modified", "Removed",
};


/*
 * cvs_logmsg_open()
 *
 * Open the file specified by <path> and allocate a buffer large enough to
 * hold all of the file's contents.  Lines starting with the log prefix
 * are not included in the result.
 * The returned value must later be free()d.
 * Returns a pointer to the allocated buffer on success, or NULL on failure.
 */
char *
cvs_logmsg_open(const char *path)
{
	int lcont;
	size_t len;
	char lbuf[256], *msg;
	struct stat st;
	FILE *fp;
	BUF *bp;

	if (stat(path, &st) == -1)
		fatal("cvs_logmsg_open: stat: `%s': %s", path, strerror(errno));

	if (!S_ISREG(st.st_mode))
		fatal("cvs_logmsg_open: message file must be a regular file");

	if (st.st_size > CVS_LOGMSG_BIGMSG) {
		do {
			fprintf(stderr,
			    "The specified message file seems big.  "
			    "Proceed anyways? (y/n) ");
			if (fgets(lbuf, (int)sizeof(lbuf), stdin) == NULL)
				fatal("cvs_logmsg_open: fgets failed");

			len = strlen(lbuf);
			if (len == 0 || len > 2 ||
			    (lbuf[0] != 'y' && lbuf[0] != 'n')) {
				fprintf(stderr, "invalid input\n");
				continue;
			} else if (lbuf[0] == 'y')
				break;
			else if (lbuf[0] == 'n')
				fatal("aborted by user");

		} while (1);
	}

	if ((fp = fopen(path, "r")) == NULL)
		fatal("cvs_logmsg_open: fopen: `%s': %s",
		    path, strerror(errno));

	bp = cvs_buf_alloc((size_t)128, BUF_AUTOEXT);

	/* lcont is used to tell if a buffer returned by fgets is a start
	 * of line or just line continuation because the buffer isn't
	 * large enough to hold the entire line.
	 */
	lcont = 0;

	while (fgets(lbuf, (int)sizeof(lbuf), fp) != NULL) {
		len = strlen(lbuf);
		if (len == 0)
			continue;
		else if (lcont == 0 &&
		    strncmp(lbuf, CVS_LOGMSG_PREFIX, strlen(CVS_LOGMSG_PREFIX)) == 0)
			/* skip lines starting with the prefix */
			continue;

		cvs_buf_append(bp, lbuf, strlen(lbuf));

		lcont = (lbuf[len - 1] == '\n') ? 0 : 1;
	}
	(void)fclose(fp);

	cvs_buf_putc(bp, '\0');

	msg = (char *)cvs_buf_release(bp);

	return (msg);
}


/*
 * cvs_logmsg_get()
 *
 * Get a log message by forking and executing the user's editor.  The <dir>
 * argument is a relative path to the directory for which the log message
 * applies, and the 3 tail queue arguments contains all the files for which the
 * log message will apply.  Any of these arguments can be set to NULL in the
 * case where there is no information to display.
 * Returns the message in a dynamically allocated string on success, NULL on
 * failure.
 */
char *
cvs_logmsg_get(const char *dir, struct cvs_flist *added,
    struct cvs_flist *modified, struct cvs_flist *removed)
{
	int i, fd, argc, fds[3], nl;
	size_t len, tlen;
	char *argv[4], buf[16], path[MAXPATHLEN], fpath[MAXPATHLEN], *msg;
	FILE *fp;
	CVSFILE *cvsfp;
	struct stat st1, st2;
	struct cvs_flist *files[3];

	files[0] = added;
	files[1] = modified;
	files[2] = removed;

	msg = NULL;
	fds[0] = -1;
	fds[1] = -1;
	fds[2] = -1;
	strlcpy(path, cvs_tmpdir, sizeof(path));
	strlcat(path, "/cvsXXXXXXXXXX", sizeof(path));
	argc = 0;
	argv[argc++] = cvs_editor;
	argv[argc++] = path;
	argv[argc] = NULL;
	tlen = 0;

	if ((fd = mkstemp(path)) == -1)
		fatal("cvs_logmsg_get: mkstemp: `%s': %s",
		    path, strerror(errno));

	if ((fp = fdopen(fd, "w")) == NULL) {
		if (unlink(path) == -1)
			cvs_log(LP_ERRNO, "failed to unlink temporary file");
		fatal("cvs_logmsg_get: fdopen failed");
	}

	fprintf(fp, "\n%s %s\n%s Enter Log.  Lines beginning with `%s' are "
	    "removed automatically\n%s\n", CVS_LOGMSG_PREFIX, CVS_LOGMSG_LINE,
	    CVS_LOGMSG_PREFIX, CVS_LOGMSG_PREFIX, CVS_LOGMSG_PREFIX);

	if (dir != NULL)
		fprintf(fp, "%s Commiting in %s\n%s\n", CVS_LOGMSG_PREFIX, dir,
		    CVS_LOGMSG_PREFIX);

	for (i = 0; i < 3; i++) {
		if (files[i] == NULL)
			continue;

		if (SIMPLEQ_EMPTY(files[i]))
			continue;

		fprintf(fp, "%s %s Files:", CVS_LOGMSG_PREFIX,
		    cvs_logmsg_ops[i]);
		nl = 1;
		SIMPLEQ_FOREACH(cvsfp, files[i], cf_list) {
			/* take the space into account */
			cvs_file_getpath(cvsfp, fpath, sizeof(fpath));
			len = strlen(fpath) + 1;
			if (tlen + len >= 72)
				nl = 1;

			if (nl) {
				fprintf(fp, "\n%s\t", CVS_LOGMSG_PREFIX);
				tlen = 8;
				nl = 0;
			}

			fprintf(fp, " %s", fpath);
			tlen += len;
		}
		fputc('\n', fp);

	}
	fprintf(fp, "%s %s\n", CVS_LOGMSG_PREFIX, CVS_LOGMSG_LINE);
	(void)fflush(fp);

	if (fstat(fd, &st1) == -1) {
		if (unlink(path) == -1)
			cvs_log(LP_ERRNO, "failed to unlink log file %s", path);
		fatal("cvs_logmsg_get: fstat failed");
	}

	for (;;) {
		if (cvs_exec(argc, argv, fds) < 0)
			break;

		if (fstat(fd, &st2) == -1) {
			cvs_log(LP_ERRNO, "failed to stat log message file");
			break;
		}

		if (st2.st_mtime != st1.st_mtime) {
			msg = cvs_logmsg_open(path);
			break;
		}

		/* nothing was entered */
		fprintf(stderr,
		    "\nLog message unchanged or not specified\na)bort, "
		    "c)ontinue, e)dit, !)reuse this message unchanged "
		    "for remaining dirs\nAction: (continue) ");

		if (fgets(buf, (int)sizeof(buf), stdin) == NULL) {
			cvs_log(LP_ERRNO, "failed to read from standard input");
			break;
		}

		len = strlen(buf);
		if (len == 0 || len > 2) {
			fprintf(stderr, "invalid input\n");
			continue;
		} else if (buf[0] == 'a') {
			cvs_log(LP_ABORT, "aborted by user");
			break;
		} else if (buf[0] == '\n' || buf[0] == 'c') {
			/* empty message */
			msg = xstrdup("");
			break;
		} else if (buf[0] == 'e')
			continue;
		else if (buf[0] == '!') {
			/* XXX do something */
		}
	}

	(void)fclose(fp);
	(void)close(fd);

	if (unlink(path) == -1)
		cvs_log(LP_ERRNO, "failed to unlink log file %s", path);

	return (msg);
}


/*
 * cvs_logmsg_send()
 *
 */
void
cvs_logmsg_send(struct cvsroot *root, const char *msg)
{
	const char *mp;
	char *np, buf[256];

	cvs_sendarg(root, "-m", 0);

	for (mp = msg; mp != NULL; mp = strchr(mp, '\n')) {
		if (*mp == '\n')
			mp++;

		/* XXX ghetto */
		strlcpy(buf, mp, sizeof(buf));
		np = strchr(buf, '\n');
		if (np != NULL)
			*np = '\0';
		cvs_sendarg(root, buf, (mp == msg) ? 0 : 1);
	}
}
