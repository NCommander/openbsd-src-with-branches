/*	$OpenBSD: tal.c,v 1.29 2021/03/25 09:27:38 claudio Exp $ */
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

#include <netinet/in.h>
#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extern.h"

static int
tal_cmp(const void *a, const void *b)
{
	char * const *sa = a;
	char * const *sb = b;

	return strcmp(*sa, *sb);
}

/*
 * Inner function for parsing RFC 8630 from a buffer.
 * Returns a valid pointer on success, NULL otherwise.
 * The pointer must be freed with tal_free().
 */
static struct tal *
tal_parse_buffer(const char *fn, char *buf)
{
	char		*nl, *line, *f, *file = NULL;
	unsigned char	*der;
	size_t		 dersz;
	int		 rc = 0;
	struct tal	*tal = NULL;
	EVP_PKEY	*pkey = NULL;

	if ((tal = calloc(1, sizeof(struct tal))) == NULL)
		err(1, NULL);

	/* Begin with the URI section, comment section already removed. */
	while ((nl = strchr(buf, '\n')) != NULL) {
		line = buf;
		*nl = '\0';

		/* advance buffer to next line */
		buf = nl + 1;

		/* Zero-length line is end of section. */
		if (*line == '\0')
			break;

		/* make sure only US-ASCII chars are in the URL */
		if (!valid_uri(line, nl - line, NULL)) {
			warnx("%s: invalid URI", fn);
			goto out;
		}
		/* Check that the URI is sensible */
		if (!(strncasecmp(line, "https://", 8) == 0 ||
		    strncasecmp(line, "rsync://", 8) == 0)) {
			warnx("%s: unsupported URL schema: %s", fn, line);
			goto out;
		}
		if (strcasecmp(nl - 4, ".cer")) {
			warnx("%s: not a certificate URL: %s", fn, line);
			goto out;
		}

		/* Append to list of URIs. */
		tal->uri = reallocarray(tal->uri,
			tal->urisz + 1, sizeof(char *));
		if (tal->uri == NULL)
			err(1, NULL);

		tal->uri[tal->urisz] = strdup(line);
		if (tal->uri[tal->urisz] == NULL)
			err(1, NULL);
		tal->urisz++;

		f = strrchr(line, '/') + 1; /* can not fail */
		if (file) {
			if (strcmp(file, f)) {
				warnx("%s: URL with different file name %s, "
				    "instead of %s", fn, f, file);
				goto out;
			}
		} else
			file = f;
	}

	if (tal->urisz == 0) {
		warnx("%s: no URIs in manifest part", fn);
		goto out;
	}

	/* sort uri lexicographically so https:// is preferred */
	qsort(tal->uri, tal->urisz, sizeof(tal->uri[0]), tal_cmp);

	/* Now the Base64-encoded public key. */
	if ((base64_decode(buf, &der, &dersz)) == -1) {
		warnx("%s: RFC 7730 section 2.1: subjectPublicKeyInfo: "
		    "bad public key", fn);
		goto out;
	}

	tal->pkey = der;
	tal->pkeysz = dersz;

	/* Make sure it's a valid public key. */
	pkey = d2i_PUBKEY(NULL, (const unsigned char **)&der, dersz);
	if (pkey == NULL) {
		cryptowarnx("%s: RFC 7730 section 2.1: subjectPublicKeyInfo: "
		    "failed public key parse", fn);
		goto out;
	}
	rc = 1;
out:
	if (rc == 0) {
		tal_free(tal);
		tal = NULL;
	}
	EVP_PKEY_free(pkey);
	return tal;
}

/*
 * Parse a TAL from "buf" conformant to RFC 7730 originally from a file
 * named "fn".
 * Returns the encoded data or NULL on syntax failure.
 */
struct tal *
tal_parse(const char *fn, char *buf)
{
	struct tal	*p;
	const char	*d;
	size_t		 dlen;

	p = tal_parse_buffer(fn, buf);
	if (p == NULL)
		return NULL;

	/* extract the TAL basename (without .tal suffix) */
	d = strrchr(fn, '/');
	if (d == NULL)
		d = fn;
	else
		d++;
	dlen = strlen(d);
	if (dlen > 4 && strcasecmp(d + dlen - 4, ".tal") == 0)
		dlen -= 4;
	if ((p->descr = strndup(d, dlen)) == NULL)
		err(1, NULL);

	return p;
}

/*
 * Read the file named "file" into a returned, NUL-terminated buffer.
 * This replaces CRLF terminators with plain LF, if found, and also
 * elides document-leading comment lines starting with "#".
 * Files may not exceeds 4096 bytes.
 * This function exits on failure, so it always returns a buffer with
 * TAL data.
 */
char *
tal_read_file(const char *file)
{
	char		*nbuf, *line = NULL, *buf = NULL;
	FILE		*in;
	ssize_t		 n, i;
	size_t		 sz = 0, bsz = 0;
	int		 optcomment = 1;

	if ((in = fopen(file, "r")) == NULL)
		err(1, "fopen: %s", file);

	while ((n = getline(&line, &sz, in)) != -1) {
		/* replace CRLF with just LF */
		if (n > 1 && line[n - 1] == '\n' && line[n - 2] == '\r') {
			line[n - 2] = '\n';
			line[n - 1] = '\0';
			n--;
		}
		if (optcomment) {
			/* if this is comment, just eat the line */
			if (line[0] == '#')
				continue;
			optcomment = 0;
			/*
			 * Empty line is end of section and needs
			 * to be eaten as well.
			 */
			if (line[0] == '\n')
				continue;
		}

		/* make sure every line is valid ascii */
		for (i = 0; i < n; i++)
			if (!isprint((unsigned char)line[i]) &&
			    !isspace((unsigned char)line[i]))
				errx(1, "getline: %s: "
				    "invalid content", file);

		/* concat line to buf */
		if ((nbuf = realloc(buf, bsz + n + 1)) == NULL)
			err(1, NULL);
		if (buf == NULL)
			nbuf[0] = '\0';	/* initialize buffer */
		buf = nbuf;
		bsz += n + 1;
		if (strlcat(buf, line, bsz) >= bsz)
			errx(1, "strlcat overflow");
		/* limit the buffer size */
		if (bsz > 4096)
			errx(1, "%s: file too big", file);
	}

	free(line);
	if (ferror(in))
		err(1, "getline: %s", file);
	fclose(in);
	if (buf == NULL)
		errx(1, "%s: no data", file);
	return buf;
}

/*
 * Free a TAL pointer.
 * Safe to call with NULL.
 */
void
tal_free(struct tal *p)
{
	size_t	 i;

	if (p == NULL)
		return;

	if (p->uri != NULL)
		for (i = 0; i < p->urisz; i++)
			free(p->uri[i]);

	free(p->pkey);
	free(p->uri);
	free(p->descr);
	free(p);
}

/*
 * Buffer TAL parsed contents for writing.
 * See tal_read() for the other side of the pipe.
 */
void
tal_buffer(struct ibuf *b, const struct tal *p)
{
	size_t	 i;

	io_buf_buffer(b, p->pkey, p->pkeysz);
	io_str_buffer(b, p->descr);
	io_simple_buffer(b, &p->urisz, sizeof(size_t));

	for (i = 0; i < p->urisz; i++)
		io_str_buffer(b, p->uri[i]);
}

/*
 * Read parsed TAL contents from descriptor.
 * See tal_buffer() for the other side of the pipe.
 * A returned pointer must be freed with tal_free().
 */
struct tal *
tal_read(int fd)
{
	size_t		 i;
	struct tal	*p;

	if ((p = calloc(1, sizeof(struct tal))) == NULL)
		err(1, NULL);

	io_buf_read_alloc(fd, (void **)&p->pkey, &p->pkeysz);
	assert(p->pkeysz > 0);
	io_str_read(fd, &p->descr);
	assert(p->descr);
	io_simple_read(fd, &p->urisz, sizeof(size_t));
	assert(p->urisz > 0);

	if ((p->uri = calloc(p->urisz, sizeof(char *))) == NULL)
		err(1, NULL);

	for (i = 0; i < p->urisz; i++) {
		io_str_read(fd, &p->uri[i]);
		assert(p->uri[i]);
	}

	return p;
}
