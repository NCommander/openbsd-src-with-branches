/* $OpenBSD: kexdhc.c,v 1.28 2019/01/21 10:03:37 djm Exp $ */
/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
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

#include <openssl/dh.h>

#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "sshkey.h"
#include "cipher.h"
#include "digest.h"
#include "dh.h"
#include "kex.h"
#include "log.h"
#include "packet.h"
#include "ssh2.h"
#include "dispatch.h"
#include "compat.h"
#include "ssherr.h"
#include "sshbuf.h"

static int input_kex_dh(int, u_int32_t, struct ssh *);

int
kexdh_client(struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	int r;
	const BIGNUM *pub_key;

	/* generate and send 'e', client DH public key */
	if ((r = kex_dh_keygen(kex)) != 0)
		goto out;
	debug("sending SSH2_MSG_KEXDH_INIT");
	DH_get0_key(kex->dh, &pub_key, NULL);
	if ((r = sshpkt_start(ssh, SSH2_MSG_KEXDH_INIT)) != 0 ||
	    (r = sshpkt_put_bignum2(ssh, pub_key)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		goto out;
#ifdef DEBUG_KEXDH
	DHparams_print_fp(stderr, kex->dh);
	fprintf(stderr, "pub= ");
	BN_print_fp(stderr, pub_key);
	fprintf(stderr, "\n");
#endif
	debug("expecting SSH2_MSG_KEXDH_REPLY");
	ssh_dispatch_set(ssh, SSH2_MSG_KEXDH_REPLY, &input_kex_dh);
	r = 0;
 out:
	return r;
}

static int
input_kex_dh(int type, u_int32_t seq, struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	BIGNUM *dh_server_pub = NULL;
	const BIGNUM *pub_key;
	struct sshkey *server_host_key = NULL;
	struct sshbuf *shared_secret = NULL;
	u_char *server_host_key_blob = NULL, *signature = NULL;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t slen, sbloblen, hashlen;
	int r;

	/* key, cert */
	if ((r = sshpkt_get_string(ssh, &server_host_key_blob,
	    &sbloblen)) != 0 ||
	    (r = sshkey_from_blob(server_host_key_blob, sbloblen,
	    &server_host_key)) != 0)
		goto out;
	if ((r = kex_verify_host_key(ssh, server_host_key)) != 0)
		goto out;
	/* DH parameter f, server public DH key, signed H */
	if ((r = sshpkt_get_bignum2(ssh, &dh_server_pub)) != 0 ||
	    (r = sshpkt_get_string(ssh, &signature, &slen)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		goto out;
	if ((shared_secret = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = kex_dh_compute_key(kex, dh_server_pub, shared_secret)) != 0)
		goto out;

	/* calc and verify H */
	DH_get0_key(kex->dh, &pub_key, NULL);
	hashlen = sizeof(hash);
	if ((r = kex_dh_hash(
	    kex->hash_alg,
	    kex->client_version,
	    kex->server_version,
	    sshbuf_ptr(kex->my), sshbuf_len(kex->my),
	    sshbuf_ptr(kex->peer), sshbuf_len(kex->peer),
	    server_host_key_blob, sbloblen,
	    pub_key,
	    dh_server_pub,
	    sshbuf_ptr(shared_secret), sshbuf_len(shared_secret),
	    hash, &hashlen)) != 0)
		goto out;

	if ((r = sshkey_verify(server_host_key, signature, slen, hash, hashlen,
	    kex->hostkey_alg, ssh->compat)) != 0)
		goto out;

	if ((r = kex_derive_keys(ssh, hash, hashlen, shared_secret)) == 0)
		r = kex_send_newkeys(ssh);
 out:
	explicit_bzero(hash, sizeof(hash));
	DH_free(kex->dh);
	kex->dh = NULL;
	BN_clear_free(dh_server_pub);
	sshbuf_free(shared_secret);
	sshkey_free(server_host_key);
	free(server_host_key_blob);
	free(signature);
	return r;
}
