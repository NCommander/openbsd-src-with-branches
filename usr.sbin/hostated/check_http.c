/*	$OpenBSD$	*/
/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@spootnik.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <net/if.h>
#include <sha1.h>
#include <limits.h>
#include <event.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "hostated.h"

struct buf	*http_request(struct host *, struct table *, int, const char *);

struct buf *
http_request(struct host *host, struct table *table, int s, const char *req)
{
	int		 fl;
	ssize_t		 sz;
	char		 rbuf[1024];
	struct buf	*buf;

	if ((fl = fcntl(s, F_GETFL, 0)) == -1)
		fatal("http_request: cannot get flags for socket");
        if (fcntl(s, F_SETFL, fl & ~(O_NONBLOCK)) == -1)
                fatal("http_request: cannot set blocking socket");
	if ((buf = buf_dynamic(sizeof(rbuf), UINT_MAX)) == NULL)
		fatalx("http_request: cannot create dynamic buffer");

	if (write(s, req, strlen(req)) != (ssize_t) strlen(req)) {
		close(s);
		return (NULL);
	}
	for (; (sz = read(s, rbuf, sizeof(rbuf))) != 0; ) {
		if (sz == -1)
			fatal("http_request: read");
		if (buf_add(buf, rbuf, sz) == -1)
			fatal("http_request: buf_add");
	}
	return (buf);
}

int
check_http_code(struct host *host, struct table *table)
{
	int		 s;
	int		 code;
	char		 scode[4];
	char		*req;
	char		*head;
	const char	*estr;
	struct buf	*buf;

	if ((s = tcp_connect(host, table)) <= 0)
		return (s);

	asprintf(&req, "HEAD %s HTTP/1.0\r\n\r\n", table->path);
	if ((buf = http_request(host, table, s, req)) == NULL)
		return (HOST_UNKNOWN);
	free(req);

	head = buf->buf;
	if (strncmp(head, "HTTP/1.1 ", strlen("HTTP/1.1 ")) &&
	    strncmp(head, "HTTP/1.0 ", strlen("HTTP/1.0 "))) {
		log_debug("check_http_code: cannot parse HTTP version");
		close(s);
		return (HOST_DOWN);
	}
	head += strlen("HTTP/1.1 ");
	if (strlen(head) < 5) /* code + \r\n */
		return (HOST_DOWN);
	strlcpy(scode, head, sizeof(scode));
	code = strtonum(scode, 100, 999, &estr);
	if (estr != NULL) {
		log_debug("check_http_code: cannot parse HTTP code");
		close(s);
		return (HOST_DOWN);
	}
	if (code != table->retcode) {
		log_debug("check_http_code: invalid HTTP code returned");
		close(s);
		return (HOST_DOWN);
	}
	close(s);
	return (HOST_UP);
}

int
check_http_digest(struct host *host, struct table *table)
{
	int		 s;
	char		*head;
	char		*req;
	struct buf	*buf;
	char		 digest[(SHA1_DIGEST_LENGTH*2)+1];

	if ((s = tcp_connect(host, table)) <= 0)
		return (s);

	asprintf(&req, "GET %s HTTP/1.0\r\n\r\n", table->path);
	if ((buf = http_request(host, table, s, req)) == NULL)
		return (HOST_UNKNOWN);
	free(req);

	head = buf->buf;
	if ((head = strstr(head, "\r\n\r\n")) == NULL) {
		log_debug("check_http_digest: host %u no end of headers",
			  host->id);
		close(s);
		return (HOST_DOWN);
	}
	head += strlen("\r\n\r\n");
	SHA1Data(head, strlen(head), digest);
	close(s);
	buf_free(buf);

	if (strcmp(table->digest, digest)) {
		log_warnx("check_http_digest: wrong digest for host %u",
			  host->id);
		return(HOST_DOWN);
	}
	return (HOST_UP);
}
