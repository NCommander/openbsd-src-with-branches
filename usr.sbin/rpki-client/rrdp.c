/*      $OpenBSD: rrdp.c,v 1.4 2021/04/12 17:23:30 claudio Exp $ */
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
#include <sys/queue.h>
#include <sys/stat.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <imsg.h>

#include <expat.h>
#include <openssl/sha.h>

#include "extern.h"
#include "rrdp.h"

#define MAX_SESSIONS	12
#define	READ_BUF_SIZE	(32 * 1024)

static struct msgbuf	msgq;

#define RRDP_STATE_REQ		0x01
#define RRDP_STATE_WAIT		0x02
#define RRDP_STATE_PARSE	0x04
#define RRDP_STATE_PARSE_ERROR	0x08
#define RRDP_STATE_PARSE_DONE	0x10
#define RRDP_STATE_HTTP_DONE	0x20
#define RRDP_STATE_DONE		(RRDP_STATE_PARSE_DONE | RRDP_STATE_HTTP_DONE)

struct rrdp {
	TAILQ_ENTRY(rrdp)	 entry;
	size_t			 id;
	char			*notifyuri;
	char			*local;
	char			*last_mod;

	struct pollfd		*pfd;
	int			 infd;
	int			 state;
	unsigned int		 file_pending;
	unsigned int		 file_failed;
	enum http_result	 res;
	enum rrdp_task		 task;

	char			 hash[SHA256_DIGEST_LENGTH];
	SHA256_CTX		 ctx;

	struct rrdp_session	 repository;
	struct rrdp_session	 current;
	XML_Parser		 parser;
	struct notification_xml	*nxml;
	struct snapshot_xml	*sxml;
	struct delta_xml	*dxml;
};

TAILQ_HEAD(,rrdp)	states = TAILQ_HEAD_INITIALIZER(states);

struct publish_xml {
	char			*uri;
	char			*data;
	char			 hash[SHA256_DIGEST_LENGTH];
	int			 data_length;
	enum publish_type	 type;
};

char *
xstrdup(const char *s)
{
	char *r;
	if ((r = strdup(s)) == NULL)
		err(1, "strdup");
	return r;
}

/*
 * Hex decode hexstring into the supplied buffer.
 * Return 0 on success else -1, if buffer too small or bad encoding.
 */
int
hex_decode(const char *hexstr, char *buf, size_t len)
{
	unsigned char ch, r;
	size_t pos = 0;
	int i;

	while (*hexstr) {
		r = 0;
		for (i = 0; i < 2; i++) {
			ch = hexstr[i];
			if (isdigit(ch))
				ch -= '0';
			else if (islower(ch))
				ch -= ('a' - 10);
			else if (isupper(ch))
				ch -= ('A' - 10);
			else
				return -1;
			if (ch > 0xf)
				return -1;
			r = r << 4 | ch;
		}
		if (pos < len)
			buf[pos++] = r;
		else
			return -1;

		hexstr += 2;
	}
	return 0;
}

/*
 * Report back that a RRDP request finished.
 * ok should only be set to 1 if the cache is now up-to-date.
 */
static void
rrdp_done(size_t id, int ok)
{
	enum rrdp_msg type = RRDP_END;
	struct ibuf *b;

	if ((b = ibuf_open(sizeof(type) + sizeof(id) + sizeof(ok))) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	io_simple_buffer(b, &ok, sizeof(ok));
	ibuf_close(&msgq, b);
}

/*
 * Request an URI to be fetched via HTTPS.
 * The main process will respond with a RRDP_HTTP_INI which includes
 * the file descriptor to read from. RRDP_HTTP_FIN is sent at the
 * end of the request with the HTTP status code and last modified timestamp.
 * If the request should not set the If-Modified-Since: header then last_mod
 * should be set to NULL, else it should point to a proper date string.
 */
static void
rrdp_http_req(size_t id, const char *uri, const char *last_mod)
{
	enum rrdp_msg type = RRDP_HTTP_REQ;
	struct ibuf *b;

	if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &id, sizeof(id));
	io_str_buffer(b, uri);
	io_str_buffer(b, last_mod);
	ibuf_close(&msgq, b);
}

/*
 * Send the session state to the main process so it gets stored.
 */
static void
rrdp_state_send(struct rrdp *s)
{
	enum rrdp_msg type = RRDP_SESSION;
	struct ibuf *b;

	if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &s->id, sizeof(s->id));
	io_str_buffer(b, s->current.session_id);
	io_simple_buffer(b, &s->current.serial, sizeof(s->current.serial));
	io_str_buffer(b, s->current.last_mod);
	ibuf_close(&msgq, b);
}

static struct rrdp *
rrdp_new(size_t id, char *local, char *notify, char *session_id,
    long long serial, char *last_mod)
{
	struct rrdp *s;

	if ((s = calloc(1, sizeof(*s))) == NULL)
		err(1, NULL);

	s->infd = -1;
	s->id = id;
	s->local = local;
	s->notifyuri = notify;
	s->repository.session_id = session_id;
	s->repository.serial = serial;
	s->repository.last_mod = last_mod;

	s->state = RRDP_STATE_REQ;
	if ((s->parser = XML_ParserCreate("US-ASCII")) == NULL)
		err(1, "XML_ParserCreate");

	s->nxml = new_notification_xml(s->parser, &s->repository, &s->current);

	TAILQ_INSERT_TAIL(&states, s, entry);

	return s;
}

static void
rrdp_free(struct rrdp *s)
{
	if (s == NULL)
		return;

	TAILQ_REMOVE(&states, s, entry);

	free_notification_xml(s->nxml);
	free_snapshot_xml(s->sxml);
	free_delta_xml(s->dxml);

	if (s->parser)
		XML_ParserFree(s->parser);
	if (s->infd != -1)
		close(s->infd);
	free(s->notifyuri);
	free(s->local);
	free(s->last_mod);
	free(s->repository.last_mod);
	free(s->repository.session_id);
	free(s->current.last_mod);
	free(s->current.session_id);

	free(s);
}

static struct rrdp *
rrdp_get(size_t id)
{
	struct rrdp *s;

	TAILQ_FOREACH(s, &states, entry)
		if (s->id == id)
			break;
	return s;
}

static void
rrdp_failed(struct rrdp *s)
{
	size_t id = s->id;

	/* reset file state before retrying */
	s->file_failed = 0;

	/* XXX MUST do some cleanup in the repo here */
	if (s->task == DELTA) {
		/* fallback to a snapshot as per RFC8182 */
		free_delta_xml(s->dxml);
		s->dxml = NULL;
		s->sxml = new_snapshot_xml(s->parser, &s->current, s);
		s->task = SNAPSHOT;
		s->state = RRDP_STATE_REQ;
	} else {
		/*
		 * TODO: update state to track recurring failures
		 * and fall back to rsync after a while.
		 * This should probably happen in the main process.
		 */
		rrdp_free(s);
		rrdp_done(id, 0);
	}
}

static void
rrdp_finished(struct rrdp *s)
{
	size_t id = s->id;

	/* check if all parts of the process have finished */
	if ((s->state & RRDP_STATE_DONE) != RRDP_STATE_DONE)
		return;

	/* still some files pending */
	if (s->file_pending > 0)
		return;

	if (s->state & RRDP_STATE_PARSE_ERROR) {
		warnx("%s: failed after XML parse error", s->local);
		rrdp_failed(s);
		return;
	}

	if (s->res == HTTP_OK) {
		XML_Parser p = s->parser;

		/*
		 * Finalize parsing on success to be sure that
		 * all of the XML is correct. Needs to be done here
		 * since the call would most probably fail for non
		 * successful data fetches.
		 */
		if (XML_Parse(p, NULL, 0, 1) != XML_STATUS_OK) {
			warnx("%s: XML error at line %llu: %s", s->local,
			    (unsigned long long)XML_GetCurrentLineNumber(p),
			    XML_ErrorString(XML_GetErrorCode(p)));
			rrdp_failed(s);
			return;
		}

		/* If a file caused an error fail the update */
		if (s->file_failed > 0) {
			rrdp_failed(s);
			return;
		}

		switch (s->task) {
		case NOTIFICATION:
			s->task = notification_done(s->nxml, s->last_mod);
			s->last_mod = NULL;
			switch (s->task) {
			case NOTIFICATION:
				warnx("%s: repository not modified",
				    s->local);
				rrdp_state_send(s);
				rrdp_free(s);
				rrdp_done(id, 1);
				break;
			case SNAPSHOT:
				warnx("%s: downloading snapshot",
				    s->local);
				s->sxml = new_snapshot_xml(p, &s->current, s);
				s->state = RRDP_STATE_REQ;
				break;
			case DELTA:
				warnx("%s: downloading %lld deltas",
				    s->local, s->repository.serial -
				    s->current.serial);
				s->dxml = new_delta_xml(p, &s->current, s);
				s->state = RRDP_STATE_REQ;
				break;
			}
			break;
		case SNAPSHOT:
			rrdp_state_send(s);
			rrdp_free(s);
			rrdp_done(id, 1);
			break;
		case DELTA:
			if (notification_delta_done(s->nxml)) {
				/* finished */
				rrdp_state_send(s);
				rrdp_free(s);
				rrdp_done(id, 1);
			} else {
				/* reset delta parser for next delta */
				free_delta_xml(s->dxml);
				s->dxml = new_delta_xml(p, &s->current, s);
				s->state = RRDP_STATE_REQ;
			}
			break;
		}
	} else if (s->res == HTTP_NOT_MOD && s->task == NOTIFICATION) {
		warnx("%s: notification file not modified", s->local);
		/* no need to update state file */
		rrdp_free(s);
		rrdp_done(id, 1);
	} else {
		warnx("%s: HTTP request failed", s->local);
		rrdp_failed(s);
	}
}

static void
rrdp_input_handler(int fd)
{
	char *local, *notify, *session_id, *last_mod;
	struct rrdp *s;
	enum rrdp_msg type;
	enum http_result res;
	long long serial;
	size_t id;
	int infd, ok;

	infd = io_recvfd(fd, &type, sizeof(type));
	io_simple_read(fd, &id, sizeof(id));

	switch (type) {
	case RRDP_START:
		io_str_read(fd, &local);
		io_str_read(fd, &notify);
		io_str_read(fd, &session_id);
		io_simple_read(fd, &serial, sizeof(serial));
		io_str_read(fd, &last_mod);
		if (infd != -1)
			errx(1, "received unexpected fd %d", infd);

		s = rrdp_new(id, local, notify, session_id, serial, last_mod);
		break;
	case RRDP_HTTP_INI:
		if (infd == -1)
			errx(1, "expected fd not received");
		s = rrdp_get(id);
		if (s == NULL)
			errx(1, "rrdp session %zu does not exist", id);
		if (s->state != RRDP_STATE_WAIT)
			errx(1, "%s: bad internal state", s->local);

		s->infd = infd;
		s->state = RRDP_STATE_PARSE;
		break;
	case RRDP_HTTP_FIN:
		io_simple_read(fd, &res, sizeof(res));
		io_str_read(fd, &last_mod);
		if (infd != -1)
			errx(1, "received unexpected fd");

		s = rrdp_get(id);
		if (s == NULL)
			errx(1, "rrdp session %zu does not exist", id);
		if (!(s->state & RRDP_STATE_PARSE))
			errx(1, "%s: bad internal state", s->local);

		s->res = res;
		s->last_mod = last_mod;
		s->state |= RRDP_STATE_HTTP_DONE;
		rrdp_finished(s);
		break;
	case RRDP_FILE:
		s = rrdp_get(id);
		if (s == NULL)
			errx(1, "rrdp session %zu does not exist", id);
		if (infd != -1)
			errx(1, "received unexpected fd %d", infd);
		io_simple_read(fd, &ok, sizeof(ok));
		if (ok == 0)
			s->file_failed++;
		s->file_pending--;
		if (s->file_pending == 0)
			rrdp_finished(s);
		break;
	default:
		errx(1, "unexpected message %d", type);
	}
}

static void
rrdp_data_handler(struct rrdp *s)
{
	char buf[READ_BUF_SIZE];
	XML_Parser p = s->parser;
	ssize_t len;

	len = read(s->infd, buf, sizeof(buf));
	if (len == -1) {
		s->state |= RRDP_STATE_PARSE_ERROR;
		warn("%s: read failure", s->local);
		return;
	}
	if ((s->state & RRDP_STATE_PARSE) == 0)
		errx(1, "%s: bad parser state", s->local);
	if (len == 0) {
		/* parser stage finished */
		close(s->infd);
		s->infd = -1;

		if (s->task != NOTIFICATION) {
			char h[SHA256_DIGEST_LENGTH];

			SHA256_Final(h, &s->ctx);
			if (memcmp(s->hash, h, sizeof(s->hash)) != 0) {
				s->state |= RRDP_STATE_PARSE_ERROR;
				warnx("%s: bad message digest", s->local);
			}
		}

		s->state |= RRDP_STATE_PARSE_DONE;
		rrdp_finished(s);
		return;
	}

	/* parse and maybe hash the bytes just read */
	if (s->task != NOTIFICATION)
		SHA256_Update(&s->ctx, buf, len);
	if ((s->state & RRDP_STATE_PARSE_ERROR) == 0 &&
	    XML_Parse(p, buf, len, 0) != XML_STATUS_OK) {
		warnx("%s: parse error at line %llu: %s", s->local,
		    (unsigned long long)XML_GetCurrentLineNumber(p),
		    XML_ErrorString(XML_GetErrorCode(p)));
		s->state |= RRDP_STATE_PARSE_ERROR;
	}
}

void
proc_rrdp(int fd)
{
	struct pollfd pfds[MAX_SESSIONS + 1];
	struct rrdp *s, *ns;
	size_t i;

	if (pledge("stdio recvfd", NULL) == -1)
		err(1, "pledge");

	memset(&pfds, 0, sizeof(pfds));

	msgbuf_init(&msgq);
	msgq.fd = fd;

	for (;;) {
		i = 1;
		TAILQ_FOREACH(s, &states, entry) {
			if (i >= MAX_SESSIONS + 1) {
				/* not enough sessions, wait for better times */
				s->pfd = NULL;
				continue;
			}
			/* request new assets when there are free sessions */
			if (s->state == RRDP_STATE_REQ) {
				const char *uri;
				switch (s->task) {
				case NOTIFICATION:
					rrdp_http_req(s->id, s->notifyuri,
					    s->repository.last_mod);
					break;
				case SNAPSHOT:
				case DELTA:
					uri = notification_get_next(s->nxml,
					    s->hash, sizeof(s->hash),
					    s->task);
					SHA256_Init(&s->ctx);
					rrdp_http_req(s->id, uri, NULL);
					break;
				}
				s->state = RRDP_STATE_WAIT;
			}
			s->pfd = pfds + i++;
			s->pfd->fd = s->infd;
			s->pfd->events = POLLIN;
		}

		/*
		 * Update main fd last.
		 * The previous loop may have enqueue messages.
		 */
		pfds[0].fd = fd;
		pfds[0].events = POLLIN;
		if (msgq.queued)
			pfds[0].events |= POLLOUT;

		if (poll(pfds, i, INFTIM) == -1)
			err(1, "poll");

		if (pfds[0].revents & POLLHUP)
			break;
		if (pfds[0].revents & POLLOUT) {
			io_socket_nonblocking(fd);
			switch (msgbuf_write(&msgq)) {
			case 0:
				errx(1, "write: connection closed");
			case -1:
				err(1, "write");
			}
			io_socket_blocking(fd);
		}
		if (pfds[0].revents & POLLIN)
			rrdp_input_handler(fd);

		TAILQ_FOREACH_SAFE(s, &states, entry, ns) {
			if (s->pfd == NULL)
				continue;
			if (s->pfd->revents != 0)
				rrdp_data_handler(s);
		}
	}

	exit(0);
}

/*
 * Both snapshots and deltas use publish_xml to store the publish and
 * withdraw records. Once all the content is added the request is sent
 * to the main process where it is processed.
 */
struct publish_xml *
new_publish_xml(enum publish_type type, char *uri, char *hash, size_t hlen)
{
	struct publish_xml *pxml;

	if ((pxml = calloc(1, sizeof(*pxml))) == NULL)
		err(1, "%s", __func__);

	pxml->type = type;
	pxml->uri = uri;
	if (hlen > 0) {
		assert(hlen == sizeof(pxml->hash));
		memcpy(pxml->hash, hash, hlen);
	}

	return pxml;
}

void
free_publish_xml(struct publish_xml *pxml)
{
	if (pxml == NULL)
		return;

	free(pxml->uri);
	free(pxml->data);
	free(pxml);
}

/*
 * Add buf to the base64 data string, ensure that this remains a proper
 * string by NUL-terminating the string.
 */
void
publish_add_content(struct publish_xml *pxml, const char *buf, int length)
{
	int new_length;

	/*
	 * optmisiation, this often gets called with '\n' as the
	 * only data... seems wasteful
	 */
	if (length == 1 && buf[0] == '\n')
		return;

	/* append content to data */
	new_length = pxml->data_length + length;
	pxml->data = realloc(pxml->data, new_length + 1);
	if (pxml->data == NULL)
		err(1, "%s", __func__);

	memcpy(pxml->data + pxml->data_length, buf, length);
	pxml->data[new_length] = '\0';
	pxml->data_length = new_length;
}

/*
 * Base64 decode the data blob and send the file to the main process
 * where the hash is validated and the file stored in the repository.
 * Increase the file_pending counter to ensure the RRDP process waits
 * until all files have been processed before moving to the next stage.
 * Returns 0 on success or -1 on errors (base64 decode failed).
 */
int
publish_done(struct rrdp *s, struct publish_xml *pxml)
{
	enum rrdp_msg type = RRDP_FILE;
	struct ibuf *b;
	unsigned char *data = NULL;
	size_t datasz = 0;

	if (pxml->data_length > 0)
		if ((base64_decode(pxml->data, &data, &datasz)) == -1)
			return -1;

	if ((b = ibuf_dynamic(256, UINT_MAX)) == NULL)
		err(1, NULL);
	io_simple_buffer(b, &type, sizeof(type));
	io_simple_buffer(b, &s->id, sizeof(s->id));
	io_simple_buffer(b, &pxml->type, sizeof(pxml->type));
	if (pxml->type != PUB_ADD)
		io_simple_buffer(b, &pxml->hash, sizeof(pxml->hash));
	io_str_buffer(b, pxml->uri);
	io_buf_buffer(b, data, datasz);
	ibuf_close(&msgq, b);
	s->file_pending++;

	free(data);
	free_publish_xml(pxml);
	return 0;
}
