/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: aes.c,v 1.6 2020/01/21 11:06:47 tb Exp $ */

/*! \file isc/aes.c */

#include "config.h"

#include <isc/assertions.h>
#include <isc/aes.h>
#include <isc/platform.h>
#include <string.h>
#include <isc/types.h>
#include <isc/util.h>

#include <openssl/opensslv.h>
#include <openssl/evp.h>

void
isc_aes128_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out)
{
	EVP_CIPHER_CTX *c;
	int len;

	c = EVP_CIPHER_CTX_new();
	RUNTIME_CHECK(c != NULL);
	RUNTIME_CHECK(EVP_EncryptInit(c, EVP_aes_128_ecb(), key, NULL) == 1);
	EVP_CIPHER_CTX_set_padding(c, 0);
	RUNTIME_CHECK(EVP_EncryptUpdate(c, out, &len, in,
					ISC_AES_BLOCK_LENGTH) == 1);
	RUNTIME_CHECK(len == ISC_AES_BLOCK_LENGTH);
	EVP_CIPHER_CTX_free(c);
}

void
isc_aes192_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out)
{
	EVP_CIPHER_CTX *c;
	int len;

	c = EVP_CIPHER_CTX_new();
	RUNTIME_CHECK(c != NULL);
	RUNTIME_CHECK(EVP_EncryptInit(c, EVP_aes_192_ecb(), key, NULL) == 1);
	EVP_CIPHER_CTX_set_padding(c, 0);
	RUNTIME_CHECK(EVP_EncryptUpdate(c, out, &len, in,
					ISC_AES_BLOCK_LENGTH) == 1);
	RUNTIME_CHECK(len == ISC_AES_BLOCK_LENGTH);
	EVP_CIPHER_CTX_free(c);
}

void
isc_aes256_crypt(const unsigned char *key, const unsigned char *in,
		 unsigned char *out)
{
	EVP_CIPHER_CTX *c;
	int len;

	c = EVP_CIPHER_CTX_new();
	RUNTIME_CHECK(c != NULL);
	RUNTIME_CHECK(EVP_EncryptInit(c, EVP_aes_256_ecb(), key, NULL) == 1);
	EVP_CIPHER_CTX_set_padding(c, 0);
	RUNTIME_CHECK(EVP_EncryptUpdate(c, out, &len, in,
					ISC_AES_BLOCK_LENGTH) == 1);
	RUNTIME_CHECK(len == ISC_AES_BLOCK_LENGTH);
	EVP_CIPHER_CTX_free(c);
}

