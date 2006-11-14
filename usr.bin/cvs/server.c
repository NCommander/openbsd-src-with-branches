/*	$OpenBSD: server.c,v 1.35 2006/11/13 12:57:03 xsa Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "diff.h"
#include "remote.h"

struct cvs_resp cvs_responses[] = {
	/* this is what our server uses, the client should support it */
	{ "Valid-requests",	1,	cvs_client_validreq, RESP_NEEDED },
	{ "ok",			0,	cvs_client_ok, RESP_NEEDED},
	{ "error",		0,	cvs_client_error, RESP_NEEDED },
	{ "E",			0,	cvs_client_e, RESP_NEEDED },
	{ "M",			0,	cvs_client_m, RESP_NEEDED },
	{ "Checked-in",		0,	cvs_client_checkedin, RESP_NEEDED },
	{ "Updated",		0,	cvs_client_updated, RESP_NEEDED },
	{ "Merged",		0,	cvs_client_merged, RESP_NEEDED },
	{ "Removed",		0,	cvs_client_removed, RESP_NEEDED },
	{ "Remove-entry",	0,	cvs_client_remove_entry, RESP_NEEDED },

	/* unsupported responses until told otherwise */
	{ "New-entry",			0,	NULL, 0 },
	{ "Created",			0,	NULL, 0 },
	{ "Update-existing",		0,	NULL, 0 },
	{ "Rcs-diff",			0,	NULL, 0 },
	{ "Patched",			0,	NULL, 0 },
	{ "Mode",			0,	NULL, 0 },
	{ "Mod-time",			0,	NULL, 0 },
	{ "Checksum",			0,	NULL, 0 },
	{ "Copy-file",			0,	NULL, 0 },
	{ "Set-static-directory",	0,	NULL, 0 },
	{ "Clear-static-directory",	0,	NULL, 0 },
	{ "Set-sticky",			0,	NULL, 0 },
	{ "Clear-sticky",		0,	NULL, 0 },
	{ "Template",			0,	NULL, 0 },
	{ "Set-checkin-prog",		0,	NULL, 0 },
	{ "Set-update-prog",		0,	NULL, 0 },
	{ "Notified",			0,	NULL, 0 },
	{ "Module-expansion",		0,	NULL, 0 },
	{ "Wrapper-rcsOption",		0,	NULL, 0 },
	{ "Mbinary",			0,	NULL, 0 },
	{ "F",				0,	NULL, 0 },
	{ "MT",				0,	NULL, 0 },
	{ "",				-1,	NULL, 0 }
};

int	cvs_server(int, char **);
char	*cvs_server_path = NULL;

static char *server_currentdir = NULL;
static char *server_argv[CVS_CMD_MAXARG];
static int server_argc = 1;

struct cvs_cmd cvs_cmd_server = {
	CVS_OP_SERVER, 0, "server", { "", "" },
	"server mode",
	NULL,
	NULL,
	NULL,
	cvs_server
};


int
cvs_server(int argc, char **argv)
{
	int l;
	char *cmd, *data;
	struct cvs_req *req;

	server_argv[0] = xstrdup("server");

	cvs_server_path = xmalloc(MAXPATHLEN);
	l = snprintf(cvs_server_path, MAXPATHLEN, "%s/cvs-serv%d",
	    cvs_tmpdir, getpid());
	if (l == -1 || l >= MAXPATHLEN)
		fatal("cvs_server: overflow in server path");

	if (mkdir(cvs_server_path, 0700) == -1)
		fatal("failed to create temporary server directory: %s, %s",
		    cvs_server_path, strerror(errno));

	if (chdir(cvs_server_path) == -1)
		fatal("failed to change directory to '%s'", cvs_server_path);

	for (;;) {
		cmd = cvs_remote_input();

		if ((data = strchr(cmd, ' ')) != NULL)
			(*data++) = '\0';

		req = cvs_remote_get_request_info(cmd);
		if (req == NULL)
			fatal("request '%s' is not supported by our server",
			    cmd);

		if (req->hdlr == NULL)
			fatal("opencvs server does not support '%s'", cmd);

		(*req->hdlr)(data);
		xfree(cmd);
	}

	return (0);
}

void
cvs_server_send_response(char *fmt, ...)
{
	va_list ap;
	char *data, *s;
	struct cvs_resp *resp;

	va_start(ap, fmt);
	vasprintf(&data, fmt, ap);
	va_end(ap);

	if ((s = strchr(data, ' ')) != NULL)
		*s = '\0';

	resp = cvs_remote_get_response_info(data);
	if (resp == NULL)
		fatal("'%s' is an unknown response", data);

	if (resp->supported != 1)
		fatal("remote cvs client does not support '%s'", data);

	if (s != NULL)
		*s = ' ';

	cvs_log(LP_TRACE, "%s", data);
	cvs_remote_output(data);
	xfree(data);
}

void
cvs_server_root(char *data)
{
	fatal("duplicate Root request from client, violates the protocol");
}

void
cvs_server_validresp(char *data)
{
	int i;
	char *sp, *ep;
	struct cvs_resp *resp;

	sp = data;
	do {
		if ((ep = strchr(sp, ' ')) != NULL)
			*ep = '\0';

		resp = cvs_remote_get_response_info(sp);
		if (resp != NULL)
			resp->supported = 1;

		if (ep != NULL)
			sp = ep + 1;
	} while (ep != NULL);

	for (i = 0; cvs_responses[i].supported != -1; i++) {
		resp = &cvs_responses[i];
		if ((resp->flags & RESP_NEEDED) &&
		    resp->supported != 1) {
			fatal("client does not support required '%s'",
			    resp->name);
		}
	}
}

void
cvs_server_validreq(char *data)
{
	BUF *bp;
	char *d;
	int i, first;

	first = 0;
	bp = cvs_buf_alloc(512, BUF_AUTOEXT);
	for (i = 0; cvs_requests[i].supported != -1; i++) {
		if (cvs_requests[i].hdlr == NULL)
			continue;

		if (first != 0)
			cvs_buf_append(bp, " ", 1);
		else
			first++;

		cvs_buf_append(bp, cvs_requests[i].name,
		    strlen(cvs_requests[i].name));
	}

	cvs_buf_putc(bp, '\0');
	d = cvs_buf_release(bp);

	cvs_server_send_response("Valid-requests %s", d);
	cvs_server_send_response("ok");
	xfree(d);
}

void
cvs_server_globalopt(char *data)
{
	if (!strcmp(data, "-t"))
		cvs_trace = 1;

	if (!strcmp(data, "-n"))
		cvs_noexec = 1;

	if (!strcmp(data, "-V"))
		verbosity = 2;
}

void
cvs_server_directory(char *data)
{
	int l;
	CVSENTRIES *entlist;
	char *dir, *repo, *parent, *entry, *dirn;

	dir = cvs_remote_input();
	if (strlen(dir) < strlen(current_cvsroot->cr_dir) + 1)
		fatal("cvs_server_directory: bad Directory request");

	repo = dir + strlen(current_cvsroot->cr_dir) + 1;
	cvs_mkpath(repo);

	if ((dirn = basename(repo)) == NULL)
		fatal("cvs_server_directory: %s", strerror(errno));

	if ((parent = dirname(repo)) == NULL)
		fatal("cvs_server_directory: %s", strerror(errno));

	if (strcmp(parent, ".")) {
		entlist = cvs_ent_open(parent);
		entry = xmalloc(CVS_ENT_MAXLINELEN);
		l = snprintf(entry, CVS_ENT_MAXLINELEN, "D/%s////", dirn);
		if (l == -1 || l >= CVS_ENT_MAXLINELEN)
			fatal("cvs_server_directory: overflow");

		cvs_ent_add(entlist, entry);
		cvs_ent_close(entlist, ENT_SYNC);
		xfree(entry);
	}

	if (server_currentdir != NULL)
		xfree(server_currentdir);
	server_currentdir = xstrdup(repo);

	xfree(dir);
}

void
cvs_server_entry(char *data)
{
	CVSENTRIES *entlist;

	entlist = cvs_ent_open(server_currentdir);
	cvs_ent_add(entlist, data);
	cvs_ent_close(entlist, ENT_SYNC);
}

void
cvs_server_modified(char *data)
{
	BUF *bp;
	int fd, l;
	size_t flen;
	mode_t fmode;
	const char *errstr;
	char *mode, *len, *fpath;

	mode = cvs_remote_input();
	len = cvs_remote_input();

	cvs_strtomode(mode, &fmode);
	xfree(mode);

	flen = strtonum(len, 0, INT_MAX, &errstr);
	if (errstr != NULL)
		fatal("cvs_server_modified: %s", errstr);
	xfree(len);

	bp = cvs_remote_receive_file(flen);

	fpath = xmalloc(MAXPATHLEN);
	l = snprintf(fpath, MAXPATHLEN, "%s/%s", server_currentdir, data);
	if (l == -1 || l >= MAXPATHLEN)
		fatal("cvs_server_modified: overflow");

	if ((fd = open(fpath, O_WRONLY | O_CREAT | O_TRUNC)) == -1)
		fatal("cvs_server_modified: %s: %s", fpath, strerror(errno));

	if (cvs_buf_write_fd(bp, fd) == -1)
		fatal("cvs_server_modified: failed to write file '%s'", fpath);

	if (fchmod(fd, 0600) == -1)
		fatal("cvs_server_modified: failed to set file mode");

	xfree(fpath);
	(void)close(fd);
	cvs_buf_free(bp);
}

void
cvs_server_useunchanged(char *data)
{
}

void
cvs_server_unchanged(char *data)
{
	int l, fd;
	char *fpath;
	CVSENTRIES *entlist;
	struct cvs_ent *ent;
	struct timeval tv[2];

	fpath = xmalloc(MAXPATHLEN);
	l = snprintf(fpath, MAXPATHLEN, "%s/%s", server_currentdir, data);
	if (l == -1 || l >= MAXPATHLEN)
		fatal("cvs_server_unchanged: overflow");

	if ((fd = open(fpath, O_RDWR | O_CREAT | O_TRUNC)) == -1)
		fatal("cvs_server_unchanged: %s: %s", fpath, strerror(errno));

	entlist = cvs_ent_open(server_currentdir);
	ent = cvs_ent_get(entlist, data);
	if (ent == NULL)
		fatal("received Unchanged request for non-existing file");
	cvs_ent_close(entlist, ENT_NOSYNC);

	tv[0].tv_sec = cvs_hack_time(ent->ce_mtime, 0);
	tv[0].tv_usec = 0;
	tv[1] = tv[0];
	if (futimes(fd, tv) == -1)
		fatal("cvs_server_unchanged: failed to set modified time");

	if (fchmod(fd, 0600) == -1)
		fatal("cvs_server_unchanged: failed to set mode");

	cvs_ent_free(ent);
	xfree(fpath);
	(void)close(fd);
}

void
cvs_server_questionable(char *data)
{
}

void
cvs_server_argument(char *data)
{

	if (server_argc > CVS_CMD_MAXARG)
		fatal("cvs_server_argument: too many arguments sent");

	server_argv[server_argc++] = xstrdup(data);
}

void
cvs_server_add(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_add: %s", strerror(errno));

	cvs_cmdop = CVS_OP_ADD;
	cvs_add(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_admin(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_admin: %s", strerror(errno));

	cvs_cmdop = CVS_OP_ADMIN;
	cvs_admin(server_argc, server_argv);
	cvs_server_send_response("ok");
}


void
cvs_server_commit(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_commit: %s", strerror(errno));

	cvs_cmdop = CVS_OP_COMMIT;
	cvs_commit(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_diff(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_diff: %s", strerror(errno));

	cvs_cmdop = CVS_OP_DIFF;
	cvs_diff(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_init(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_init: %s", strerror(errno));

	cvs_cmdop = CVS_OP_INIT;
	cvs_init(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_remove(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_remove: %s", strerror(errno));

	cvs_cmdop = CVS_OP_REMOVE;
	cvs_remove(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_status(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_status: %s", strerror(errno));

	cvs_cmdop = CVS_OP_STATUS;
	cvs_status(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_log(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_log: %s", strerror(errno));

	cvs_cmdop = CVS_OP_LOG;
	cvs_getlog(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_tag(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_tag: %s", strerror(errno));

	cvs_cmdop = CVS_OP_TAG;
	cvs_tag(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_update(char *data)
{
	if (chdir(server_currentdir) == -1)
		fatal("cvs_server_update: %s", strerror(errno));

	cvs_cmdop = CVS_OP_UPDATE;
	cvs_update(server_argc, server_argv);
	cvs_server_send_response("ok");
}

void
cvs_server_version(char *data)
{
	cvs_cmdop = CVS_OP_VERSION;
	cvs_version(server_argc, server_argv);
	cvs_server_send_response("ok");
}
