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
#include <assert.h>
#include <grp.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * Free a list of struct ident previously allocated with idents_gid_add().
 * Does nothing if the pointer is NULL.
 */
void
idents_free(struct ident *p, size_t sz)
{
	size_t	 i;

	if (NULL == p)
		return;
	for (i = 0; i < sz; i++)
		free(p[i].name);
	free(p);
}

/*
 * Given a list of groups from the remote host, fill in our local
 * identifiers of the same names.
 * Use the remote numeric identifier if we can't find the group OR the
 * group has identifier zero.
 */
void
idents_gid_remap(struct sess *sess, struct ident *gids, size_t gidsz)
{
	size_t	 	 i;
	struct group	*grp;

	for (i = 0; i < gidsz; i++) {
		if (NULL == (grp = getgrnam(gids[i].name)))
			gids[i].mapped = gids[i].id;
		else if (0 == grp->gr_gid)
			gids[i].mapped = gids[i].id;
		else
			gids[i].mapped = grp->gr_gid;
		LOG4(sess, "remapped group %s: %" PRId32 " -> %" PRId32,
			gids[i].name, gids[i].id, gids[i].mapped);
	}
}

/*
 * If "gid" is not part of the list of known groups, add it.
 * This also verifies that the group name isn't too long.
 * Return zero on failure, non-zero on success.
 */
int
idents_gid_add(struct sess *sess, struct ident **gids, size_t *gidsz, gid_t gid)
{
	struct group	*grp;
	size_t		 i, sz;
	void		*pp;

	for (i = 0; i < *gidsz; i++)
		if ((*gids)[i].id == (int32_t)gid)
			return 1;

	/* 
	 * Look us up in /etc/group.
	 * Make sure that the group name length is sane: we transmit it
	 * using a single byte.
	 */

	assert(i == *gidsz);
	if (NULL == (grp = getgrgid(gid))) {
		ERR(sess, "%u: unknown gid", gid);
		return 0;
	} else if ((sz = strlen(grp->gr_name)) > UINT8_MAX) {
		ERRX(sess, "%u: group name too long: %s", gid, grp->gr_name);
		return 0;
	} else if (0 == sz) {
		ERRX(sess, "%u: group name zero-length", gid);
		return 0;
	}

	/* Add the group to the array. */

	pp = reallocarray(*gids, *gidsz + 1, sizeof(struct ident));
	if (NULL == pp) {
		ERR(sess, "reallocarray");
		return 0;
	}
	*gids = pp;
	(*gids)[*gidsz].id = gid;
	(*gids)[*gidsz].name = strdup(grp->gr_name);
	if (NULL == (*gids)[*gidsz].name) {
		ERR(sess, "strdup");
		return 0;
	}

	LOG4(sess, "adding group to list: %s (%u)", 
		(*gids)[*gidsz].name, (*gids)[*gidsz].id);
	(*gidsz)++;
	return 1;
}

/*
 * Send a list of struct ident.
 * See idents_recv().
 * We should only do this if we're preserving gids/uids.
 * Return zero on failure, non-zero on success.
 */
int
idents_send(struct sess *sess, 
	int fd, const struct ident *ids, size_t idsz)
{
	size_t	 i, sz;

	for (i = 0; i < idsz; i++) {
		assert(NULL != ids[i].name);
		sz = strlen(ids[i].name);
		assert(sz > 0 && sz <= UINT8_MAX);
		if (!io_write_int(sess, fd, ids[i].id)) {
			ERRX1(sess, "io_write_int");
			return 0;
		} else if (!io_write_byte(sess, fd, sz)) {
			ERRX1(sess, "io_write_byte");
			return 0;
		} else if (!io_write_buf(sess, fd, ids[i].name, sz)) {
			ERRX1(sess, "io_write_byte");
			return 0;
		}
	}

	if (!io_write_int(sess, fd, 0)) {
		ERRX1(sess, "io_write_int");
		return 0;
	}

	return 1;
}

/*
 * Receive a list of struct ident.
 * See idents_send().
 * We should only do this if we're preserving gids/uids.
 * Return zero on failure, non-zero on success.
 */
int
idents_recv(struct sess *sess,
	int fd, struct ident **ids, size_t *idsz)
{
	int32_t	 id;
	uint8_t	 sz;
	void	*pp;

	for (;;) {
		if (!io_read_int(sess, fd, &id)) {
			ERRX1(sess, "io_read_int");
			return 0;
		} else if (0 == id)
			break;
		
		pp = reallocarray(*ids, 
			*idsz + 1, sizeof(struct ident));
		if (NULL == pp) {
			ERR(sess, "reallocarray");
			return 0;
		}
		*ids = pp;
		memset(&(*ids)[*idsz], 0, sizeof(struct ident));
		if (!io_read_byte(sess, fd, &sz)) {
			ERRX1(sess, "io_read_byte");
			return 0;
		}
		(*ids)[*idsz].id = id;
		(*ids)[*idsz].name = calloc(sz + 1, 1);
		if (NULL == (*ids)[*idsz].name) {
			ERR(sess, "calloc");
			return 0;
		}
		if (!io_read_buf(sess, fd, (*ids)[*idsz].name, sz)) {
			ERRX1(sess, "io_read_buf");
			return 0;
		}
		(*idsz)++;
	}

	return 1;
}

