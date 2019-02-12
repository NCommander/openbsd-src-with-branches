/*	$Id: fargs.c,v 1.7 2019/02/12 18:06:25 benno Exp $ */
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

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "extern.h"

#define	RSYNC_PATH	"rsync"

char **
fargs_cmdline(struct sess *sess, const struct fargs *f)
{
	char		**args;
	size_t		  i = 0, j, argsz = 0;
	char		 *rsync_path, *ssh_prog;

	assert(f != NULL);
	assert(f->sourcesz > 0);

	if ((rsync_path = sess->opts->rsync_path) == NULL)
		rsync_path = RSYNC_PATH;
	if ((ssh_prog = sess->opts->ssh_prog) == NULL)
		ssh_prog = "ssh";

	/* Be explicit with array size. */

	argsz += 1;	/* dot separator */
	argsz += 1;	/* sink file */
	argsz += 5;	/* per-mode maximum */
	argsz += 11;	/* shared args */
	argsz += 1;	/* NULL pointer */
	argsz += f->sourcesz;

	args = calloc(argsz, sizeof(char *));
	if (args == NULL) {
		ERR(sess, "calloc");
		return NULL;
	}

	if (f->host != NULL) {
		assert(f->host != NULL);

		args[i++] = ssh_prog;
		args[i++] = f->host;
		args[i++] = rsync_path;
		args[i++] = "--server";
		if (f->mode == FARGS_RECEIVER)
			args[i++] = "--sender";
	} else {
		args[i++] = rsync_path;
		args[i++] = "--server";
	}

	/* Shared arguments. */

	if (sess->opts->del)
		args[i++] = "--delete";
	if (sess->opts->preserve_gids)
		args[i++] = "-g";
	if (sess->opts->preserve_links)
		args[i++] = "-l";
	if (sess->opts->dry_run)
		args[i++] = "-n";
	if (sess->opts->preserve_perms)
		args[i++] = "-p";
	if (sess->opts->recursive)
		args[i++] = "-r";
	if (sess->opts->preserve_times)
		args[i++] = "-t";
	if (sess->opts->verbose > 3)
		args[i++] = "-v";
	if (sess->opts->verbose > 2)
		args[i++] = "-v";
	if (sess->opts->verbose > 1)
		args[i++] = "-v";
	if (sess->opts->verbose > 0)
		args[i++] = "-v";

	/* Terminate with a full-stop for reasons unknown. */

	args[i++] = ".";

	if (f->mode == FARGS_RECEIVER) {
		for (j = 0; j < f->sourcesz; j++)
			args[i++] = f->sources[j];
	} else
		args[i++] = f->sink;

	args[i] = NULL;
	return args;
}
