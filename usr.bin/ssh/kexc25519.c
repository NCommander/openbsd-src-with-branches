/* $OpenBSD: kexc25519.c,v 1.13 2019/01/21 10:20:12 djm Exp $ */
/*
 * Copyright (c) 2019 Markus Friedl.  All rights reserved.
 * Copyright (c) 2010 Damien Miller.  All rights reserved.
 * Copyright (c) 2013 Aris Adamantiadis.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "sshkey.h"
#include "kex.h"
#include "sshbuf.h"
#include "digest.h"
#include "ssherr.h"
#include "ssh2.h"

extern int crypto_scalarmult_curve25519(u_char a[CURVE25519_SIZE],
    const u_char b[CURVE25519_SIZE], const u_char c[CURVE25519_SIZE])
	__attribute__((__bounded__(__minbytes__, 1, CURVE25519_SIZE)))
	__attribute__((__bounded__(__minbytes__, 2, CURVE25519_SIZE)))
	__attribute__((__bounded__(__minbytes__, 3, CURVE25519_SIZE)));

void
kexc25519_keygen(u_char key[CURVE25519_SIZE], u_char pub[CURVE25519_SIZE])
{
	static const u_char basepoint[CURVE25519_SIZE] = {9};

	arc4random_buf(key, CURVE25519_SIZE);
	crypto_scalarmult_curve25519(pub, key, basepoint);
}

int
kexc25519_shared_key_ext(const u_char key[CURVE25519_SIZE],
    const u_char pub[CURVE25519_SIZE], struct sshbuf *out, int raw)
{
	u_char shared_key[CURVE25519_SIZE];
	u_char zero[CURVE25519_SIZE];
	int r;

	crypto_scalarmult_curve25519(shared_key, key, pub);

	/* Check for all-zero shared secret */
	explicit_bzero(zero, CURVE25519_SIZE);
	if (timingsafe_bcmp(zero, shared_key, CURVE25519_SIZE) == 0)
		return SSH_ERR_KEY_INVALID_EC_VALUE;

#ifdef DEBUG_KEXECDH
	dump_digest("shared secret", shared_key, CURVE25519_SIZE);
#endif
	if (raw)
		r = sshbuf_put(out, shared_key, CURVE25519_SIZE);
	else
		r = sshbuf_put_bignum2_bytes(out, shared_key, CURVE25519_SIZE);
	explicit_bzero(shared_key, CURVE25519_SIZE);
	return r;
}

int
kexc25519_shared_key(const u_char key[CURVE25519_SIZE],
    const u_char pub[CURVE25519_SIZE], struct sshbuf *out)
{
	return kexc25519_shared_key_ext(key, pub, out, 0);
}

int
kex_c25519_hash(
    int hash_alg,
    const struct sshbuf *client_version,
    const struct sshbuf *server_version,
    const u_char *ckexinit, size_t ckexinitlen,
    const u_char *skexinit, size_t skexinitlen,
    const u_char *serverhostkeyblob, size_t sbloblen,
    const u_char *client_pub, size_t client_pub_len,
    const u_char *server_pub, size_t server_pub_len,
    const u_char *shared_secret, size_t secretlen,
    u_char *hash, size_t *hashlen)
{
	struct sshbuf *b;
	int r;

	if (*hashlen < ssh_digest_bytes(hash_alg))
		return SSH_ERR_INVALID_ARGUMENT;
	if ((b = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_put_stringb(b, client_version)) != 0 ||
	    (r = sshbuf_put_stringb(b, server_version)) != 0 ||
	    /* kexinit messages: fake header: len+SSH2_MSG_KEXINIT */
	    (r = sshbuf_put_u32(b, ckexinitlen+1)) != 0 ||
	    (r = sshbuf_put_u8(b, SSH2_MSG_KEXINIT)) != 0 ||
	    (r = sshbuf_put(b, ckexinit, ckexinitlen)) != 0 ||
	    (r = sshbuf_put_u32(b, skexinitlen+1)) != 0 ||
	    (r = sshbuf_put_u8(b, SSH2_MSG_KEXINIT)) != 0 ||
	    (r = sshbuf_put(b, skexinit, skexinitlen)) != 0 ||
	    (r = sshbuf_put_string(b, serverhostkeyblob, sbloblen)) != 0 ||
	    (r = sshbuf_put_string(b, client_pub, client_pub_len)) != 0 ||
	    (r = sshbuf_put_string(b, server_pub, server_pub_len)) != 0 ||
	    (r = sshbuf_put(b, shared_secret, secretlen)) != 0) {
		sshbuf_free(b);
		return r;
	}
#ifdef DEBUG_KEX
	sshbuf_dump(b, stderr);
#endif
	if (ssh_digest_buffer(hash_alg, b, hash, *hashlen) != 0) {
		sshbuf_free(b);
		return SSH_ERR_LIBCRYPTO_ERROR;
	}
	sshbuf_free(b);
	*hashlen = ssh_digest_bytes(hash_alg);
#ifdef DEBUG_KEX
	dump_digest("hash", hash, *hashlen);
#endif
	return 0;
}

int
kex_c25519_keypair(struct kex *kex)
{
	struct sshbuf *buf = NULL;
	u_char *cp = NULL;
	int r;

	if ((buf = sshbuf_new()) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_reserve(buf, CURVE25519_SIZE, &cp)) != 0)
		goto out;
	kexc25519_keygen(kex->c25519_client_key, cp);
#ifdef DEBUG_KEXECDH
	dump_digest("client public key c25519:", cp, CURVE25519_SIZE);
#endif
	kex->kem_client_pub = buf;
	buf = NULL;
 out:
	sshbuf_free(buf);
	return r;
}

int
kex_c25519_enc(struct kex *kex, const u_char *pkblob,
   size_t pklen, struct sshbuf **server_blobp, struct sshbuf **shared_secretp)
{
	struct sshbuf *server_blob = NULL;
	struct sshbuf *buf = NULL;
	u_char *server_pub;
	u_char server_key[CURVE25519_SIZE];
	int r;

	*server_blobp = NULL;
	*shared_secretp = NULL;

	if (pklen != CURVE25519_SIZE) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
#ifdef DEBUG_KEXECDH
	dump_digest("client public key 25519:", pkblob, CURVE25519_SIZE);
#endif
	/* allocate space for encrypted KEM key and ECDH pub key */
	if ((server_blob = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_reserve(server_blob, CURVE25519_SIZE, &server_pub)) != 0)
		goto out;
	kexc25519_keygen(server_key, server_pub);
	/* allocate shared secret */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = kexc25519_shared_key_ext(server_key, pkblob, buf, 0)) < 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("server public key 25519:", server_pub, CURVE25519_SIZE);
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	*server_blobp = server_blob;
	*shared_secretp = buf;
	server_blob = NULL;
	buf = NULL;
 out:
	explicit_bzero(server_key, sizeof(server_key));
	sshbuf_free(server_blob);
	sshbuf_free(buf);
	return r;
}

int
kex_c25519_dec(struct kex *kex, const u_char *pkblob,
    size_t pklen, struct sshbuf **shared_secretp)
{
	struct sshbuf *buf = NULL;
	int r;

	*shared_secretp = NULL;

	if (pklen != CURVE25519_SIZE) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
#ifdef DEBUG_KEXECDH
	dump_digest("server public key c25519:", pkblob, CURVE25519_SIZE);
#endif
	/* shared secret */
	if ((buf = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = kexc25519_shared_key_ext(kex->c25519_client_key, pkblob,
	    buf, 0)) < 0)
		goto out;
#ifdef DEBUG_KEXECDH
	dump_digest("encoded shared secret:", sshbuf_ptr(buf), sshbuf_len(buf));
#endif
	*shared_secretp = buf;
	buf = NULL;
 out:
	sshbuf_free(buf);
	return r;
}
