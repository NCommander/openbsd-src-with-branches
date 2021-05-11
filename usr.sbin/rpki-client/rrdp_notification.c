/*
 * Copyright (c) 2020 Nils Fisher <nils_fisher@hotmail.com>
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
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
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <expat.h>
#include <openssl/sha.h>

#include "extern.h"
#include "rrdp.h"

enum notification_scope {
	NOTIFICATION_SCOPE_START,
	NOTIFICATION_SCOPE_NOTIFICATION,
	NOTIFICATION_SCOPE_SNAPSHOT,
	NOTIFICATION_SCOPE_NOTIFICATION_POST_SNAPSHOT,
	NOTIFICATION_SCOPE_DELTA,
	NOTIFICATION_SCOPE_END
};

struct delta_item {
	char			*uri;
	char			 hash[SHA256_DIGEST_LENGTH];
	long long		 serial;
	TAILQ_ENTRY(delta_item)	 q;
};

TAILQ_HEAD(delta_q, delta_item);

struct notification_xml {
	XML_Parser		 parser;
	struct rrdp_session	*repository;
	struct rrdp_session	*current;
	char			*session_id;
	char			*snapshot_uri;
	char			 snapshot_hash[SHA256_DIGEST_LENGTH];
	struct delta_q		 delta_q;
	long long		 serial;
	int			 version;
	enum notification_scope	 scope;
};

static void	free_delta(struct delta_item *);

static int
add_delta(struct notification_xml *nxml, const char *uri,
    const char hash[SHA256_DIGEST_LENGTH], long long serial)
{
	struct delta_item *d, *n;

	if ((d = calloc(1, sizeof(struct delta_item))) == NULL)
		err(1, "%s - calloc", __func__);

	d->serial = serial;
	d->uri = xstrdup(uri);
	memcpy(d->hash, hash, sizeof(d->hash));

	/* optimise for a sorted input */
	n = TAILQ_LAST(&nxml->delta_q, delta_q);
	if (n == NULL)
		TAILQ_INSERT_HEAD(&nxml->delta_q, d, q);
	else if (n->serial < serial)
		TAILQ_INSERT_TAIL(&nxml->delta_q, d, q);
	else
		TAILQ_FOREACH(n, &nxml->delta_q, q) {
			if (n->serial == serial) {
				warnx("duplicate delta serial %lld ", serial);
				free_delta(d);
				return 0;
			}
			if (n->serial > serial) {
				TAILQ_INSERT_BEFORE(n, d, q);
				break;
			}
		}

	return 1;
}

static void
free_delta(struct delta_item *d)
{
	free(d->uri);
	free(d);
}

static void
start_notification_elem(struct notification_xml *nxml, const char **attr)
{
	XML_Parser p = nxml->parser;
	int has_xmlns = 0;
	size_t i;

	if (nxml->scope != NOTIFICATION_SCOPE_START)
		PARSE_FAIL(p,
		    "parse failed - entered notification elem unexpectedely");
	for (i = 0; attr[i]; i += 2) {
		const char *errstr;
		if (strcmp("xmlns", attr[i]) == 0) {
			has_xmlns = 1;
			continue;
		}
		if (strcmp("session_id", attr[i]) == 0) {
			nxml->session_id = xstrdup(attr[i + 1]);
			continue;
		}
		if (strcmp("version", attr[i]) == 0) {
			nxml->version = strtonum(attr[i + 1],
			    1, MAX_VERSION, &errstr);
			if (errstr == NULL)
				continue;
		}
		if (strcmp("serial", attr[i]) == 0) {
			nxml->serial = strtonum(attr[i + 1],
			    1, LLONG_MAX, &errstr);
			if (errstr == NULL)
				continue;
		}
		PARSE_FAIL(p, "parse failed - non conforming "
		    "attribute found in notification elem");
	}
	if (!(has_xmlns && nxml->version && nxml->session_id && nxml->serial))
		PARSE_FAIL(p, "parse failed - incomplete "
		    "notification attributes");

	nxml->scope = NOTIFICATION_SCOPE_NOTIFICATION;
}

static void
end_notification_elem(struct notification_xml *nxml)
{
	XML_Parser p = nxml->parser;

	if (nxml->scope != NOTIFICATION_SCOPE_NOTIFICATION_POST_SNAPSHOT)
		PARSE_FAIL(p, "parse failed - exited notification "
		    "elem unexpectedely");
	nxml->scope = NOTIFICATION_SCOPE_END;
}

static void
start_snapshot_elem(struct notification_xml *nxml, const char **attr)
{
	XML_Parser p = nxml->parser;
	int i, hasUri = 0, hasHash = 0;

	if (nxml->scope != NOTIFICATION_SCOPE_NOTIFICATION)
		PARSE_FAIL(p,
		    "parse failed - entered snapshot elem unexpectedely");
	for (i = 0; attr[i]; i += 2) {
		if (strcmp("uri", attr[i]) == 0 && hasUri++ == 0) {
			if (valid_uri(attr[i + 1], strlen(attr[i + 1]),
			    "https://")) {
				nxml->snapshot_uri = xstrdup(attr[i + 1]);
				continue;
			}
		}
		if (strcmp("hash", attr[i]) == 0 && hasHash++ == 0) {
			if (hex_decode(attr[i + 1], nxml->snapshot_hash,
			    sizeof(nxml->snapshot_hash)) == 0)
				continue;
		}
		PARSE_FAIL(p, "parse failed - non conforming "
		    "attribute found in snapshot elem");
	}
	if (hasUri != 1 || hasHash != 1)
		PARSE_FAIL(p, "parse failed - incomplete snapshot attributes");

	nxml->scope = NOTIFICATION_SCOPE_SNAPSHOT;
}

static void
end_snapshot_elem(struct notification_xml *nxml)
{
	XML_Parser p = nxml->parser;

	if (nxml->scope != NOTIFICATION_SCOPE_SNAPSHOT)
		PARSE_FAIL(p, "parse failed - exited snapshot "
		    "elem unexpectedely");
	nxml->scope = NOTIFICATION_SCOPE_NOTIFICATION_POST_SNAPSHOT;
}

static void
start_delta_elem(struct notification_xml *nxml, const char **attr)
{
	XML_Parser p = nxml->parser;
	int i, hasUri = 0, hasHash = 0;
	const char *delta_uri = NULL;
	char delta_hash[SHA256_DIGEST_LENGTH];
	long long delta_serial = 0;

	if (nxml->scope != NOTIFICATION_SCOPE_NOTIFICATION_POST_SNAPSHOT)
		PARSE_FAIL(p, "parse failed - entered delta "
		    "elem unexpectedely");
	for (i = 0; attr[i]; i += 2) {
		if (strcmp("uri", attr[i]) == 0 && hasUri++ == 0) {
			if (valid_uri(attr[i + 1], strlen(attr[i + 1]),
			    "https://")) {
				delta_uri = attr[i + 1];
				continue;
			}
		}
		if (strcmp("hash", attr[i]) == 0 && hasHash++ == 0) {
			if (hex_decode(attr[i + 1], delta_hash,
			    sizeof(delta_hash)) == 0)
				continue;
		}
		if (strcmp("serial", attr[i]) == 0 && delta_serial == 0) {
			const char *errstr;

			delta_serial = strtonum(attr[i + 1],
			    1, LLONG_MAX, &errstr);
			if (errstr == NULL)
				continue;
		}
		PARSE_FAIL(p, "parse failed - non conforming "
		    "attribute found in snapshot elem");
	}
	/* Only add to the list if we are relevant */
	if (hasUri != 1 || hasHash != 1 || delta_serial == 0)
		PARSE_FAIL(p, "parse failed - incomplete delta attributes");

	/* optimisation, add only deltas that could be interesting */
	if (nxml->repository->serial != 0 &&
	    nxml->repository->serial < delta_serial &&
	    nxml->repository->session_id != NULL &&
	    strcmp(nxml->session_id, nxml->repository->session_id) == 0) {
		if (add_delta(nxml, delta_uri, delta_hash, delta_serial) == 0)
			PARSE_FAIL(p, "parse failed - adding delta failed");
	}

	nxml->scope = NOTIFICATION_SCOPE_DELTA;
}

static void
end_delta_elem(struct notification_xml *nxml)
{
	XML_Parser p = nxml->parser;

	if (nxml->scope != NOTIFICATION_SCOPE_DELTA)
		PARSE_FAIL(p, "parse failed - exited delta elem unexpectedely");
	nxml->scope = NOTIFICATION_SCOPE_NOTIFICATION_POST_SNAPSHOT;
}

static void
notification_xml_elem_start(void *data, const char *el, const char **attr)
{
	struct notification_xml *nxml = data;
	XML_Parser p = nxml->parser;

	/*
	 * Can only enter here once as we should have no ways to get back to
	 * START scope
	 */
	if (strcmp("notification", el) == 0)
		start_notification_elem(nxml, attr);
	/*
	 * Will enter here multiple times, BUT never nested. will start
	 * collecting character data in that handler
	 * mem is cleared in end block, (TODO or on parse failure)
	 */
	else if (strcmp("snapshot", el) == 0)
		start_snapshot_elem(nxml, attr);
	else if (strcmp("delta", el) == 0)
		start_delta_elem(nxml, attr);
	else
		PARSE_FAIL(p, "parse failed - unexpected elem exit found");
}

static void
notification_xml_elem_end(void *data, const char *el)
{
	struct notification_xml *nxml = data;
	XML_Parser p = nxml->parser;

	if (strcmp("notification", el) == 0)
		end_notification_elem(nxml);
	else if (strcmp("snapshot", el) == 0)
		end_snapshot_elem(nxml);
	else if (strcmp("delta", el) == 0)
		end_delta_elem(nxml);
	else
		PARSE_FAIL(p, "parse failed - unexpected elem exit found");
}

struct notification_xml *
new_notification_xml(XML_Parser p, struct rrdp_session *repository,
    struct rrdp_session *current)
{
	struct notification_xml *nxml;

	if ((nxml = calloc(1, sizeof(*nxml))) == NULL)
		err(1, "%s", __func__);
	TAILQ_INIT(&(nxml->delta_q));
	nxml->parser = p;
	nxml->repository = repository;
	nxml->current = current;

	XML_SetElementHandler(nxml->parser, notification_xml_elem_start,
	    notification_xml_elem_end);
	XML_SetUserData(nxml->parser, nxml);

	return nxml;
}

void
free_notification_xml(struct notification_xml *nxml)
{
	if (nxml == NULL)
		return;

	free(nxml->session_id);
	free(nxml->snapshot_uri);
	while (!TAILQ_EMPTY(&nxml->delta_q)) {
		struct delta_item *d = TAILQ_FIRST(&nxml->delta_q);
		TAILQ_REMOVE(&nxml->delta_q, d, q);
		free_delta(d);
	}
	free(nxml);
}

/*
 * Finalize notification step, decide if a delta update is possible
 * if either the session_id changed or the delta files fail to cover
 * all the steps up to the new serial fall back to a snapshot.
 * Return SNAPSHOT or DELTA for snapshot or delta processing.
 * Return NOTIFICATION if repository is up to date.
 */
enum rrdp_task
notification_done(struct notification_xml *nxml, char *last_mod)
{
	struct delta_item *d;
	long long s, last_s = 0;

	nxml->current->last_mod = last_mod;
	nxml->current->session_id = xstrdup(nxml->session_id);

	/* check the that the session_id was valid and still the same */
	if (nxml->repository->session_id == NULL ||
	    strcmp(nxml->session_id, nxml->repository->session_id) != 0)
		goto snapshot;

	/* if repository serial is 0 fall back to snapshot */
	if (nxml->repository->serial == 0)
		goto snapshot;

	if (nxml->repository->serial == nxml->serial) {
		nxml->current->serial = nxml->serial;
		return NOTIFICATION;
	}

	/* check that all needed deltas are available */
	s = nxml->repository->serial + 1;
	TAILQ_FOREACH(d, &nxml->delta_q, q) {
		if (d->serial != s++)
			goto snapshot;
		last_s = d->serial;
	}
	if (last_s != nxml->serial)
		goto snapshot;

	/* update via delta possible */
	nxml->current->serial = nxml->repository->serial;
	nxml->repository->serial = nxml->serial;
	return DELTA;

snapshot:
	/* update via snapshot download */
	nxml->current->serial = nxml->serial;
	return SNAPSHOT;
}

const char *
notification_get_next(struct notification_xml *nxml, char *hash, size_t hlen,
    enum rrdp_task task)
{
	struct delta_item *d;

	switch (task) {
	case SNAPSHOT:
		assert(hlen == sizeof(nxml->snapshot_hash));
		memcpy(hash, nxml->snapshot_hash, hlen);
		/*
		 * Ensure that the serial is correct in case a previous
		 * delta request failed.
		 */
		nxml->current->serial = nxml->serial;
		return nxml->snapshot_uri;
	case DELTA:
		/* first bump serial, then use first delta  */
		nxml->current->serial += 1;
		d = TAILQ_FIRST(&nxml->delta_q);
		assert(d->serial == nxml->current->serial);
		assert(hlen == sizeof(d->hash));
		memcpy(hash, d->hash, hlen);
		return d->uri;
	default:
		errx(1, "%s: bad task", __func__);
	}
}

/*
 * Pop first element from the delta queue. Return non-0 if this was the last
 * delta to fetch.
 */
int
notification_delta_done(struct notification_xml *nxml)
{
	struct delta_item *d;

	d = TAILQ_FIRST(&nxml->delta_q);
	assert(d->serial == nxml->current->serial);
	TAILQ_REMOVE(&nxml->delta_q, d, q);
	free_delta(d);

	assert(!TAILQ_EMPTY(&nxml->delta_q) ||
	    nxml->serial == nxml->current->serial);
	return TAILQ_EMPTY(&nxml->delta_q);
}

void
log_notification_xml(struct notification_xml *nxml)
{
	logx("session_id: %s, serial: %lld", nxml->session_id, nxml->serial);
	logx("snapshot_uri: %s", nxml->snapshot_uri);
}
