/* $OpenBSD: ssh-xmss.c,v 1.8 2022/10/28 00:37:24 djm Exp $*/
/*
 * Copyright (c) 2017 Stefan-Lukas Gazdag.
 * Copyright (c) 2017 Markus Friedl.
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
#define SSHKEY_INTERNAL
#include <sys/types.h>
#include <limits.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>

#include "log.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "sshkey-xmss.h"
#include "ssherr.h"
#include "ssh.h"

#include "xmss_fast.h"

static void
ssh_xmss_cleanup(struct sshkey *k)
{
	freezero(k->xmss_pk, sshkey_xmss_pklen(k));
	freezero(k->xmss_sk, sshkey_xmss_sklen(k));
	sshkey_xmss_free_state(k);
	free(k->xmss_name);
	free(k->xmss_filename);
	k->xmss_pk = NULL;
	k->xmss_sk = NULL;
	k->xmss_name = NULL;
	k->xmss_filename = NULL;
}

static int
ssh_xmss_equal(const struct sshkey *a, const struct sshkey *b)
{
	if (a->xmss_pk == NULL || b->xmss_pk == NULL)
		return 0;
	if (sshkey_xmss_pklen(a) != sshkey_xmss_pklen(b))
		return 0;
	if (memcmp(a->xmss_pk, b->xmss_pk, sshkey_xmss_pklen(a)) != 0)
		return 0;
	return 1;
}

static int
ssh_xmss_serialize_public(const struct sshkey *key, struct sshbuf *b,
    const char *typename, enum sshkey_serialize_rep opts)
{
	int r;

	if (key->xmss_name == NULL || key->xmss_pk == NULL ||
	    sshkey_xmss_pklen(key) == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((r = sshbuf_put_cstring(b, typename)) != 0 ||
	    (r = sshbuf_put_cstring(b, key->xmss_name)) != 0 ||
	    (r = sshbuf_put_string(b, key->xmss_pk,
	    sshkey_xmss_pklen(key))) != 0 ||
	    (r = sshkey_xmss_serialize_pk_info(key, b, opts)) != 0)
		return r;

	return 0;
}

int
ssh_xmss_sign(const struct sshkey *key, u_char **sigp, size_t *lenp,
    const u_char *data, size_t datalen, u_int compat)
{
	u_char *sig = NULL;
	size_t slen = 0, len = 0, required_siglen;
	unsigned long long smlen;
	int r, ret;
	struct sshbuf *b = NULL;

	if (lenp != NULL)
		*lenp = 0;
	if (sigp != NULL)
		*sigp = NULL;

	if (key == NULL ||
	    sshkey_type_plain(key->type) != KEY_XMSS ||
	    key->xmss_sk == NULL ||
	    sshkey_xmss_params(key) == NULL)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((r = sshkey_xmss_siglen(key, &required_siglen)) != 0)
		return r;
	if (datalen >= INT_MAX - required_siglen)
		return SSH_ERR_INVALID_ARGUMENT;
	smlen = slen = datalen + required_siglen;
	if ((sig = malloc(slen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshkey_xmss_get_state(key, 1)) != 0)
		goto out;
	if ((ret = xmss_sign(key->xmss_sk, sshkey_xmss_bds_state(key), sig, &smlen,
	    data, datalen, sshkey_xmss_params(key))) != 0 || smlen <= datalen) {
		r = SSH_ERR_INVALID_ARGUMENT; /* XXX better error? */
		goto out;
	}
	/* encode signature */
	if ((b = sshbuf_new()) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	if ((r = sshbuf_put_cstring(b, "ssh-xmss@openssh.com")) != 0 ||
	    (r = sshbuf_put_string(b, sig, smlen - datalen)) != 0)
		goto out;
	len = sshbuf_len(b);
	if (sigp != NULL) {
		if ((*sigp = malloc(len)) == NULL) {
			r = SSH_ERR_ALLOC_FAIL;
			goto out;
		}
		memcpy(*sigp, sshbuf_ptr(b), len);
	}
	if (lenp != NULL)
		*lenp = len;
	/* success */
	r = 0;
 out:
	if ((ret = sshkey_xmss_update_state(key, 1)) != 0) {
		/* discard signature since we cannot update the state */
		if (r == 0 && sigp != NULL && *sigp != NULL) {
			explicit_bzero(*sigp, len);
			free(*sigp);
		}
		if (sigp != NULL)
			*sigp = NULL;
		if (lenp != NULL)
			*lenp = 0;
		r = ret;
	}
	sshbuf_free(b);
	if (sig != NULL)
		freezero(sig, slen);

	return r;
}

int
ssh_xmss_verify(const struct sshkey *key,
    const u_char *signature, size_t signaturelen,
    const u_char *data, size_t datalen, u_int compat)
{
	struct sshbuf *b = NULL;
	char *ktype = NULL;
	const u_char *sigblob;
	u_char *sm = NULL, *m = NULL;
	size_t len, required_siglen;
	unsigned long long smlen = 0, mlen = 0;
	int r, ret;

	if (key == NULL ||
	    sshkey_type_plain(key->type) != KEY_XMSS ||
	    key->xmss_pk == NULL ||
	    sshkey_xmss_params(key) == NULL ||
	    signature == NULL || signaturelen == 0)
		return SSH_ERR_INVALID_ARGUMENT;
	if ((r = sshkey_xmss_siglen(key, &required_siglen)) != 0)
		return r;
	if (datalen >= INT_MAX - required_siglen)
		return SSH_ERR_INVALID_ARGUMENT;

	if ((b = sshbuf_from(signature, signaturelen)) == NULL)
		return SSH_ERR_ALLOC_FAIL;
	if ((r = sshbuf_get_cstring(b, &ktype, NULL)) != 0 ||
	    (r = sshbuf_get_string_direct(b, &sigblob, &len)) != 0)
		goto out;
	if (strcmp("ssh-xmss@openssh.com", ktype) != 0) {
		r = SSH_ERR_KEY_TYPE_MISMATCH;
		goto out;
	}
	if (sshbuf_len(b) != 0) {
		r = SSH_ERR_UNEXPECTED_TRAILING_DATA;
		goto out;
	}
	if (len != required_siglen) {
		r = SSH_ERR_INVALID_FORMAT;
		goto out;
	}
	if (datalen >= SIZE_MAX - len) {
		r = SSH_ERR_INVALID_ARGUMENT;
		goto out;
	}
	smlen = len + datalen;
	mlen = smlen;
	if ((sm = malloc(smlen)) == NULL || (m = malloc(mlen)) == NULL) {
		r = SSH_ERR_ALLOC_FAIL;
		goto out;
	}
	memcpy(sm, sigblob, len);
	memcpy(sm+len, data, datalen);
	if ((ret = xmss_sign_open(m, &mlen, sm, smlen,
	    key->xmss_pk, sshkey_xmss_params(key))) != 0) {
		debug2_f("xmss_sign_open failed: %d", ret);
	}
	if (ret != 0 || mlen != datalen) {
		r = SSH_ERR_SIGNATURE_INVALID;
		goto out;
	}
	/* XXX compare 'm' and 'data' ? */
	/* success */
	r = 0;
 out:
	if (sm != NULL)
		freezero(sm, smlen);
	if (m != NULL)
		freezero(m, smlen);
	sshbuf_free(b);
	free(ktype);
	return r;
}

static const struct sshkey_impl_funcs sshkey_xmss_funcs = {
	/* .size = */		NULL,
	/* .alloc = */		NULL,
	/* .cleanup = */	ssh_xmss_cleanup,
	/* .equal = */		ssh_xmss_equal,
	/* .ssh_serialize_public = */ ssh_xmss_serialize_public,
	/* .generate = */	sshkey_xmss_generate_private_key,
};

const struct sshkey_impl sshkey_xmss_impl = {
	/* .name = */		"ssh-xmss@openssh.com",
	/* .shortname = */	"XMSS",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_XMSS,
	/* .nid = */		0,
	/* .cert = */		0,
	/* .sigonly = */	0,
	/* .keybits = */	256,
	/* .funcs = */		&sshkey_xmss_funcs,
};

const struct sshkey_impl sshkey_xmss_cert_impl = {
	/* .name = */		"ssh-xmss-cert-v01@openssh.com",
	/* .shortname = */	"XMSS-CERT",
	/* .sigalg = */		NULL,
	/* .type = */		KEY_XMSS_CERT,
	/* .nid = */		0,
	/* .cert = */		1,
	/* .sigonly = */	0,
	/* .keybits = */	256,
	/* .funcs = */		&sshkey_xmss_funcs,
};
