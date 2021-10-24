/*	$Id: test-cert.c,v 1.13 2021/10/13 06:56:07 claudio Exp $ */
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

#include <sys/socket.h>
#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509v3.h>

#include "extern.h"

int verbose;

int
main(int argc, char *argv[])
{
	int		 c, i, verb = 0, ta = 0;
	X509		*xp = NULL;
	struct cert	*p;

	ERR_load_crypto_strings();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();

	while ((c = getopt(argc, argv, "tv")) != -1)
		switch (c) {
		case 't':
			ta = 1;
			break;
		case 'v':
			verb++;
			break;
		default:
			errx(1, "bad argument %c", c);
		}

	argv += optind;
	argc -= optind;

	if (argc == 0)
		errx(1, "argument missing");

	if (ta) {
		if (argc % 2)
			errx(1, "need even number of arguments");

		for (i = 0; i < argc; i += 2) {
			const char	*cert_path = argv[i];
			const char	*tal_path = argv[i + 1];
			char		*buf;
			struct tal	*tal;

			buf = tal_read_file(tal_path);
			tal = tal_parse(tal_path, buf);
			free(buf);
			if (tal == NULL)
				break;

			p = ta_parse(&xp, cert_path, tal->pkey, tal->pkeysz);
			tal_free(tal);
			if (p == NULL)
				break;

			if (verb)
				cert_print(p);
			cert_free(p);
			X509_free(xp);
		}
	} else {
		for (i = 0; i < argc; i++) {
			p = cert_parse(&xp, argv[i]);
			if (p == NULL)
				break;
			if (verb)
				cert_print(p);
			cert_free(p);
			X509_free(xp);
		}
	}

	EVP_cleanup();
	CRYPTO_cleanup_all_ex_data();
	ERR_free_strings();

	if (i < argc)
		errx(1, "test failed for %s", argv[i]);

	printf("OK\n");
	return 0;
}
