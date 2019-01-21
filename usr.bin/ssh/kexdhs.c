/* $OpenBSD: kexdhs.c,v 1.33 2019/01/21 10:00:23 djm Exp $ */
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
#include <string.h>
#include <signal.h>

#include <openssl/dh.h>

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

static int input_kex_dh_init(int, u_int32_t, struct ssh *);

int
kexdh_server(struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	int r;

	/* generate server DH public key */
	if ((r = kex_dh_keygen(kex)) != 0)
		return r;
	debug("expecting SSH2_MSG_KEXDH_INIT");
	ssh_dispatch_set(ssh, SSH2_MSG_KEXDH_INIT, &input_kex_dh_init);
	return 0;
}

int
input_kex_dh_init(int type, u_int32_t seq, struct ssh *ssh)
{
	struct kex *kex = ssh->kex;
	BIGNUM *dh_client_pub = NULL;
	const BIGNUM *pub_key;
	struct sshkey *server_host_public, *server_host_private;
	struct sshbuf *shared_secret = NULL;
	u_char *signature = NULL, *server_host_key_blob = NULL;
	u_char hash[SSH_DIGEST_MAX_LENGTH];
	size_t sbloblen, slen;
	size_t hashlen;
	int r;

	if (kex->load_host_public_key == NULL ||
	    kex->load_host_private_key == NULL) {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	server_host_public = kex->load_host_public_key(kex->hostkey_type,
	    kex->hostkey_nid, ssh);
	server_host_private = kex->load_host_private_key(kex->hostkey_type,
	    kex->hostkey_nid, ssh);
	if (server_host_public == NULL) {
		r = SSH_ERR_NO_HOSTKEY_LOADED;
		goto out;
	}

	/* key, cert */
	if ((r = sshpkt_get_bignum2(ssh, &dh_client_pub)) != 0 ||
	    (r = sshpkt_get_end(ssh)) != 0)
		goto out;
	if ((shared_secret = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = kex_dh_compute_key(kex, dh_client_pub, shared_secret)) != 0)
		goto out;
	if ((r = sshkey_to_blob(server_host_public, &server_host_key_blob,
	    &sbloblen)) != 0)
		goto out;
	/* calc H */
	DH_get0_key(kex->dh, &pub_key, NULL);
	hashlen = sizeof(hash);
	if ((r = kex_dh_hash(
	    kex->hash_alg,
	    kex->client_version,
	    kex->server_version,
	    sshbuf_ptr(kex->peer), sshbuf_len(kex->peer),
	    sshbuf_ptr(kex->my), sshbuf_len(kex->my),
	    server_host_key_blob, sbloblen,
	    dh_client_pub,
	    pub_key,
	    sshbuf_ptr(shared_secret), sshbuf_len(shared_secret),
	    hash, &hashlen)) != 0)
		goto out;

	/* sign H */
	if ((r = kex->sign(ssh, server_host_private, server_host_public,
	    &signature, &slen, hash, hashlen, kex->hostkey_alg)) < 0)
		goto out;

	/* send server hostkey, DH pubkey 'f' and signed H */
	if ((r = sshpkt_start(ssh, SSH2_MSG_KEXDH_REPLY)) != 0 ||
	    (r = sshpkt_put_string(ssh, server_host_key_blob, sbloblen)) != 0 ||
	    (r = sshpkt_put_bignum2(ssh, pub_key)) != 0 ||	/* f */
	    (r = sshpkt_put_string(ssh, signature, slen)) != 0 ||
	    (r = sshpkt_send(ssh)) != 0)
		goto out;

	if ((r = kex_derive_keys(ssh, hash, hashlen, shared_secret)) == 0)
		r = kex_send_newkeys(ssh);
 out:
	explicit_bzero(hash, sizeof(hash));
	DH_free(kex->dh);
	kex->dh = NULL;
	BN_clear_free(dh_client_pub);
	sshbuf_free(shared_secret);
	free(server_host_key_blob);
	free(signature);
	return r;
}
