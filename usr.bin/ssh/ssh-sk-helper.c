/* $OpenBSD: ssh-sk-helper.c,v 1.4 2019/12/13 19:11:14 djm Exp $ */
/*
 * Copyright (c) 2019 Google LLC
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

/*
 * This is a tiny program used to isolate the address space used for
 * security key middleware signing operations from ssh-agent. It is similar
 * to ssh-pkcs11-helper.c but considerably simpler as the operations for
 * security keys are stateless.
 *
 * Please crank SSH_SK_HELPER_VERSION in sshkey.h for any incompatible
 * protocol changes.
 */

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "xmalloc.h"
#include "log.h"
#include "sshkey.h"
#include "authfd.h"
#include "misc.h"
#include "sshbuf.h"
#include "msg.h"
#include "uidswap.h"
#include "sshkey.h"
#include "ssherr.h"
#include "ssh-sk.h"

extern char *__progname;

static struct sshbuf *
process_sign(struct sshbuf *req)
{
	int r = SSH_ERR_INTERNAL_ERROR;
	struct sshbuf *resp, *kbuf;
	struct sshkey *key;
	uint32_t compat;
	const u_char *message;
	u_char *sig;
	size_t msglen, siglen;
	char *provider;

	if ((r = sshbuf_froms(req, &kbuf)) != 0 ||
	    (r = sshbuf_get_cstring(req, &provider, NULL)) != 0 ||
	    (r = sshbuf_get_string_direct(req, &message, &msglen)) != 0 ||
	    (r = sshbuf_get_cstring(req, NULL, NULL)) != 0 || /* alg */
	    (r = sshbuf_get_u32(req, &compat)) != 0)
		fatal("%s: buffer error: %s", __progname, ssh_err(r));
	if (sshbuf_len(req) != 0)
		fatal("%s: trailing data in request", __progname);

	if ((r = sshkey_private_deserialize(kbuf, &key)) != 0)
		fatal("Unable to parse private key: %s", ssh_err(r));
	if (!sshkey_is_sk(key))
		fatal("Unsupported key type %s", sshkey_ssh_name(key));

	debug("%s: ready to sign with key %s, provider %s: "
	    "msg len %zu, compat 0x%lx", __progname, sshkey_type(key),
	    provider, msglen, (u_long)compat);

	if ((r = sshsk_sign(provider, key, &sig, &siglen,
	    message, msglen, compat)) != 0)
		fatal("Signing failed: %s", ssh_err(r));

	if ((resp = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __progname);

	if ((r = sshbuf_put_string(resp, sig, siglen)) != 0)
		fatal("%s: buffer error: %s", __progname, ssh_err(r));

	sshbuf_free(kbuf);
	free(provider);

	return resp;
}

static struct sshbuf *
process_enroll(struct sshbuf *req)
{
	int r;
	u_int type;
	char *provider;
	char *application;
	uint8_t flags;
	struct sshbuf *challenge, *attest, *kbuf, *resp;
	struct sshkey *key;

	if ((resp = sshbuf_new()) == NULL ||
	    (attest = sshbuf_new()) == NULL ||
	    (kbuf = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __progname);

	if ((r = sshbuf_get_u32(req, &type)) != 0 ||
	    (r = sshbuf_get_cstring(req, &provider, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(req, &application, NULL)) != 0 ||
	    (r = sshbuf_get_u8(req, &flags)) != 0 ||
	    (r = sshbuf_froms(req, &challenge)) != 0)
		fatal("%s: buffer error: %s", __progname, ssh_err(r));
	if (sshbuf_len(req) != 0)
		fatal("%s: trailing data in request", __progname);

	if (type > INT_MAX)
		fatal("%s: bad type %u", __progname, type);
	if (sshbuf_len(challenge) == 0) {
		sshbuf_free(challenge);
		challenge = NULL;
	}

	if ((r = sshsk_enroll((int)type, provider, application, flags,
	    challenge, &key, attest)) != 0)
		fatal("%s: sshsk_enroll failed: %s", __progname, ssh_err(r));

	if ((r = sshkey_private_serialize(key, kbuf)) != 0)
		fatal("%s: serialize private key: %s", __progname, ssh_err(r));
	if ((r = sshbuf_put_stringb(resp, kbuf)) != 0 ||
	    (r = sshbuf_put_stringb(resp, attest)) != 0)
		fatal("%s: buffer error: %s", __progname, ssh_err(r));

	sshkey_free(key);
	sshbuf_free(kbuf);
	sshbuf_free(attest);
	sshbuf_free(challenge);
	free(provider);
	free(application);

	return resp;
}

static struct sshbuf *
process_load_resident(struct sshbuf *req)
{
	int r;
	char *provider, *pin;
	struct sshbuf *kbuf, *resp;
	struct sshkey **keys = NULL;
	size_t nkeys = 0, i;

	if ((resp = sshbuf_new()) == NULL ||
	    (kbuf = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __progname);

	if ((r = sshbuf_get_cstring(req, &provider, NULL)) != 0 ||
	    (r = sshbuf_get_cstring(req, &pin, NULL)) != 0)
		fatal("%s: buffer error: %s", __progname, ssh_err(r));
	if (sshbuf_len(req) != 0)
		fatal("%s: trailing data in request", __progname);

	if ((r = sshsk_load_resident(provider, pin, &keys, &nkeys)) != 0)
		fatal("%s: sshsk_load_resident failed: %s",
		    __progname, ssh_err(r));

	for (i = 0; i < nkeys; i++) {
		debug("%s: key %zu %s %s", __func__, i,
		    sshkey_type(keys[i]), keys[i]->sk_application);
		sshbuf_reset(kbuf);
		if ((r = sshkey_private_serialize(keys[i], kbuf)) != 0)
			fatal("%s: serialize private key: %s",
			    __progname, ssh_err(r));
		if ((r = sshbuf_put_stringb(resp, kbuf)) != 0 ||
		    (r = sshbuf_put_cstring(resp, "")) != 0) /* comment */
			fatal("%s: buffer error: %s", __progname, ssh_err(r));
	}

	for (i = 0; i < nkeys; i++)
		sshkey_free(keys[i]);
	free(keys);
	sshbuf_free(kbuf);
	free(provider);
	freezero(pin, strlen(pin));
	return resp;
}

int
main(int argc, char **argv)
{
	SyslogFacility log_facility = SYSLOG_FACILITY_AUTH;
	LogLevel log_level = SYSLOG_LEVEL_ERROR;
	struct sshbuf *req, *resp;
	int in, out, ch, r, log_stderr = 0;
	u_int rtype;
	uint8_t version;

	sanitise_stdfd();
	log_init(__progname, log_level, log_facility, log_stderr);

	while ((ch = getopt(argc, argv, "v")) != -1) {
		switch (ch) {
		case 'v':
			log_stderr = 1;
			if (log_level == SYSLOG_LEVEL_ERROR)
				log_level = SYSLOG_LEVEL_DEBUG1;
			else if (log_level < SYSLOG_LEVEL_DEBUG3)
				log_level++;
			break;
		default:
			fprintf(stderr, "usage: %s [-v]\n", __progname);
			exit(1);
		}
	}
	log_init(__progname, log_level, log_facility, log_stderr);

	/*
	 * Rearrange our file descriptors a little; we don't trust the
	 * providers not to fiddle with stdin/out.
	 */
	closefrom(STDERR_FILENO + 1);
	if ((in = dup(STDIN_FILENO)) == -1 || (out = dup(STDOUT_FILENO)) == -1)
		fatal("%s: dup: %s", __progname, strerror(errno));
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	sanitise_stdfd(); /* resets to /dev/null */

	if ((req = sshbuf_new()) == NULL)
		fatal("%s: sshbuf_new failed", __progname);
	if (ssh_msg_recv(in, req) < 0)
		fatal("ssh_msg_recv failed");
	close(in);
	debug("%s: received message len %zu", __progname, sshbuf_len(req));

	if ((r = sshbuf_get_u8(req, &version)) != 0)
		fatal("%s: buffer error: %s", __progname, ssh_err(r));
	if (version != SSH_SK_HELPER_VERSION) {
		fatal("unsupported version: received %d, expected %d",
		    version, SSH_SK_HELPER_VERSION);
	}

	if ((r = sshbuf_get_u32(req, &rtype)) != 0)
		fatal("%s: buffer error: %s", __progname, ssh_err(r));

	switch (rtype) {
	case SSH_SK_HELPER_SIGN:
		resp = process_sign(req);
		break;
	case SSH_SK_HELPER_ENROLL:
		resp = process_enroll(req);
		break;
	case SSH_SK_HELPER_LOAD_RESIDENT:
		resp = process_load_resident(req);
		break;
	default:
		fatal("%s: unsupported request type %u", __progname, rtype);
	}
	sshbuf_free(req);
	debug("%s: reply len %zu", __progname, sshbuf_len(resp));

	if (ssh_msg_send(out, SSH_SK_HELPER_VERSION, resp) == -1)
		fatal("ssh_msg_send failed");
	sshbuf_free(resp);
	close(out);

	return (0);
}
