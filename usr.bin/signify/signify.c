/* $OpenBSD: signify.c,v 1.24 2014/01/10 04:34:24 tedu Exp $ */
/*
 * Copyright (c) 2013 Ted Unangst <tedu@openbsd.org>
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
#include <sys/stat.h>

#include <netinet/in.h>
#include <resolv.h>

#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <readpassphrase.h>
#include <util.h>
#include <sha2.h>

#include "crypto_api.h"

#define SIGBYTES crypto_sign_ed25519_BYTES
#define SECRETBYTES crypto_sign_ed25519_SECRETKEYBYTES
#define PUBLICBYTES crypto_sign_ed25519_PUBLICKEYBYTES

#define PKALG "Ed"
#define KDFALG "BK"
#define FPLEN 8

#define COMMENTHDR "untrusted comment: "
#define COMMENTHDRLEN 19
#define COMMENTMAXLEN 1024

struct enckey {
	uint8_t pkalg[2];
	uint8_t kdfalg[2];
	uint32_t kdfrounds;
	uint8_t salt[16];
	uint8_t checksum[8];
	uint8_t fingerprint[FPLEN];
	uint8_t seckey[SECRETBYTES];
};

struct pubkey {
	uint8_t pkalg[2];
	uint8_t fingerprint[FPLEN];
	uint8_t pubkey[PUBLICBYTES];
};

struct sig {
	uint8_t pkalg[2];
	uint8_t fingerprint[FPLEN];
	uint8_t sig[SIGBYTES];
};

extern char *__progname;

static void
usage(void)
{
	fprintf(stderr, "usage:"
#ifndef VERIFYONLY
	    "\t%1$s -G [-n] -p pubkey -s seckey\n"
	    "\t%1$s -I [-o sigfile] [-p pubkey] [-s seckey]\n"
	    "\t%1$s -S [-e] [-o sigfile] -s seckey message\n"
#endif
	    "\t%1$s -V [-e] [-o sigfile] -p pubkey message\n",
	    __progname);
	exit(1);
}

static int
xopen(const char *fname, int flags, mode_t mode)
{
	int fd;

	fd = open(fname, flags, mode);
	if (fd == -1)
		err(1, "open %s", fname);
	return fd;
}

static void *
xmalloc(size_t len)
{
	void *p;

	p = malloc(len);
	if (!p)
		err(1, "malloc %zu", len);
	return p;
}

static void
readall(int fd, void *buf, size_t len, const char *filename)
{
	ssize_t x;
	
	x = read(fd, buf, len);
	if (x == -1) {
		err(1, "read from %s", filename);
	} else if (x != len) {
		errx(1, "short read from %s", filename);
	}
}

static size_t
parseb64file(const char *filename, char *b64, void *buf, size_t len,
    char *comment)
{
	int rv;
	char *commentend, *b64end;

	commentend = strchr(b64, '\n');
	if (!commentend || commentend - b64 <= COMMENTHDRLEN ||
	    memcmp(b64, COMMENTHDR, COMMENTHDRLEN))
		errx(1, "invalid comment in %s; must start with '%s'",
		    filename, COMMENTHDR);
	*commentend = 0;
	if (comment)
		strlcpy(comment, b64 + COMMENTHDRLEN, COMMENTMAXLEN);
	b64end = strchr(commentend + 1, '\n');
	if (!b64end)
		errx(1, "missing new line after b64 in %s", filename);
	*b64end = 0;
	rv = b64_pton(commentend + 1, buf, len);
	if (rv != len)
		errx(1, "invalid b64 encoding in %s", filename);
	if (memcmp(buf, PKALG, 2))
		errx(1, "unsupported file %s", filename);
	return b64end - b64 + 1;
}

static void
readb64file(const char *filename, void *buf, size_t len, char *comment)
{
	char b64[2048];
	int rv, fd;

	fd = xopen(filename, O_RDONLY | O_NOFOLLOW, 0);
	memset(b64, 0, sizeof(b64));
	rv = read(fd, b64, sizeof(b64) - 1);
	if (rv == -1)
		err(1, "read from %s", filename);
	parseb64file(filename, b64, buf, len, comment);
	memset(b64, 0, sizeof(b64));
	close(fd);
}

uint8_t *
readmsg(const char *filename, unsigned long long *msglenp)
{
	unsigned long long msglen;
	uint8_t *msg;
	struct stat sb;
	int fd;

	fd = xopen(filename, O_RDONLY | O_NOFOLLOW, 0);
	fstat(fd, &sb);
	msglen = sb.st_size;
	if (msglen > (1UL << 30))
		errx(1, "msg too large in %s", filename);
	msg = xmalloc(msglen);
	readall(fd, msg, msglen, filename);
	close(fd);

	*msglenp = msglen;
	return msg;
}

static void
writeall(int fd, const void *buf, size_t len, const char *filename)
{
	ssize_t x;
	
	x = write(fd, buf, len);
	if (x == -1) {
		err(1, "write to %s", filename);
	} else if (x != len) {
		errx(1, "short write to %s", filename);
	}
}

#ifndef VERIFYONLY
static void
appendall(const char *filename, const void *buf, size_t len)
{
	int fd;

	fd = xopen(filename, O_NOFOLLOW | O_RDWR | O_APPEND, 0);
	writeall(fd, buf, len, filename);
	close(fd);
}

static void
writeb64file(const char *filename, const char *comment, const void *buf,
    size_t len, int flags, mode_t mode)
{
	char header[1024];
	char b64[1024];
	int fd, rv;

	fd = xopen(filename, O_CREAT|flags|O_NOFOLLOW|O_RDWR, mode);
	snprintf(header, sizeof(header), "%ssignify %s\n", COMMENTHDR,
	    comment);
	writeall(fd, header, strlen(header), filename);
	if ((rv = b64_ntop(buf, len, b64, sizeof(b64)-1)) == -1)
		errx(1, "b64 encode failed");
	b64[rv++] = '\n';
	writeall(fd, b64, rv, filename);
	memset(b64, 0, sizeof(b64));
	close(fd);
}

static void
kdf(uint8_t *salt, size_t saltlen, int rounds, uint8_t *key, size_t keylen)
{
	char pass[1024];

	if (rounds == 0) {
		memset(key, 0, keylen);
		return;
	}

	if (!readpassphrase("passphrase: ", pass, sizeof(pass), 0))
		errx(1, "readpassphrase");
	if (bcrypt_pbkdf(pass, strlen(pass), salt, saltlen, key,
	    keylen, rounds) == -1)
		errx(1, "bcrypt pbkdf");
	memset(pass, 0, sizeof(pass));
}

static void
signmsg(uint8_t *seckey, uint8_t *msg, unsigned long long msglen,
    uint8_t *sig)
{
	unsigned long long siglen;
	uint8_t *sigbuf;

	sigbuf = xmalloc(msglen + SIGBYTES);
	crypto_sign_ed25519(sigbuf, &siglen, msg, msglen, seckey);
	memcpy(sig, sigbuf, SIGBYTES);
	free(sigbuf);
}

static void
generate(const char *pubkeyfile, const char *seckeyfile, int rounds)
{
	uint8_t digest[SHA512_DIGEST_LENGTH];
	struct pubkey pubkey;
	struct enckey enckey;
	uint8_t xorkey[sizeof(enckey.seckey)];
	uint8_t fingerprint[FPLEN];
	SHA2_CTX ctx;
	int i;

	crypto_sign_ed25519_keypair(pubkey.pubkey, enckey.seckey);
	arc4random_buf(fingerprint, sizeof(fingerprint));

	SHA512Init(&ctx);
	SHA512Update(&ctx, enckey.seckey, sizeof(enckey.seckey));
	SHA512Final(digest, &ctx);

	memcpy(enckey.pkalg, PKALG, 2);
	memcpy(enckey.kdfalg, KDFALG, 2);
	enckey.kdfrounds = htonl(rounds);
	memcpy(enckey.fingerprint, fingerprint, FPLEN);
	arc4random_buf(enckey.salt, sizeof(enckey.salt));
	kdf(enckey.salt, sizeof(enckey.salt), rounds, xorkey, sizeof(xorkey));
	memcpy(enckey.checksum, digest, sizeof(enckey.checksum));
	for (i = 0; i < sizeof(enckey.seckey); i++)
		enckey.seckey[i] ^= xorkey[i];
	memset(digest, 0, sizeof(digest));
	memset(xorkey, 0, sizeof(xorkey));

	writeb64file(seckeyfile, "secret key", &enckey,
	    sizeof(enckey), O_EXCL, 0600);
	memset(&enckey, 0, sizeof(enckey));

	memcpy(pubkey.pkalg, PKALG, 2);
	memcpy(pubkey.fingerprint, fingerprint, FPLEN);
	writeb64file(pubkeyfile, "public key", &pubkey,
	    sizeof(pubkey), O_EXCL, 0666);
}

static void
sign(const char *seckeyfile, const char *msgfile, const char *sigfile,
    int embedded)
{
	struct sig sig;
	uint8_t digest[SHA512_DIGEST_LENGTH];
	struct enckey enckey;
	uint8_t xorkey[sizeof(enckey.seckey)];
	uint8_t *msg;
	char comment[COMMENTMAXLEN], sigcomment[1024];
	unsigned long long msglen;
	int i, rounds;
	SHA2_CTX ctx;

	readb64file(seckeyfile, &enckey, sizeof(enckey), comment);

	if (memcmp(enckey.kdfalg, KDFALG, 2))
		errx(1, "unsupported KDF");
	rounds = ntohl(enckey.kdfrounds);
	kdf(enckey.salt, sizeof(enckey.salt), rounds, xorkey, sizeof(xorkey));
	for (i = 0; i < sizeof(enckey.seckey); i++)
		enckey.seckey[i] ^= xorkey[i];
	memset(xorkey, 0, sizeof(xorkey));
	SHA512Init(&ctx);
	SHA512Update(&ctx, enckey.seckey, sizeof(enckey.seckey));
	SHA512Final(digest, &ctx);
	if (memcmp(enckey.checksum, digest, sizeof(enckey.checksum)))
	    errx(1, "incorrect passphrase");
	memset(digest, 0, sizeof(digest));

	msg = readmsg(msgfile, &msglen);

	signmsg(enckey.seckey, msg, msglen, sig.sig);
	memcpy(sig.fingerprint, enckey.fingerprint, FPLEN);
	memset(&enckey, 0, sizeof(enckey));

	memcpy(sig.pkalg, PKALG, 2);
	snprintf(sigcomment, sizeof(sigcomment), "signature from %s", comment);
	writeb64file(sigfile, sigcomment, &sig, sizeof(sig), O_TRUNC, 0666);
	if (embedded)
		appendall(sigfile, msg, msglen);

	free(msg);
}

static void
inspect(const char *seckeyfile, const char *pubkeyfile, const char *sigfile)
{
	struct sig sig;
	struct enckey enckey;
	struct pubkey pubkey;
	char fp[(FPLEN + 2) / 3 * 4 + 1];

	if (seckeyfile) {
		readb64file(seckeyfile, &enckey, sizeof(enckey), NULL);
		b64_ntop(enckey.fingerprint, FPLEN, fp, sizeof(fp));
		printf("sec fp: %s\n", fp);
	}
	if (pubkeyfile) {
		readb64file(pubkeyfile, &pubkey, sizeof(pubkey), NULL);
		b64_ntop(pubkey.fingerprint, FPLEN, fp, sizeof(fp));
		printf("pub fp: %s\n", fp);
	}
	if (sigfile) {
		readb64file(sigfile, &sig, sizeof(sig), NULL);
		b64_ntop(sig.fingerprint, FPLEN, fp, sizeof(fp));
		printf("sig fp: %s\n", fp);
	}
}
#endif

static void
verifymsg(uint8_t *pubkey, uint8_t *msg, unsigned long long msglen,
    uint8_t *sig)
{
	uint8_t *sigbuf, *dummybuf;
	unsigned long long siglen, dummylen;

	siglen = SIGBYTES + msglen;
	sigbuf = xmalloc(siglen);
	dummybuf = xmalloc(siglen);
	memcpy(sigbuf, sig, SIGBYTES);
	memcpy(sigbuf + SIGBYTES, msg, msglen);
	if (crypto_sign_ed25519_open(dummybuf, &dummylen, sigbuf, siglen,
	    pubkey) == -1)
		errx(1, "signature verification failed");
	free(sigbuf);
	free(dummybuf);
}


static void
verify(const char *pubkeyfile, const char *msgfile, const char *sigfile,
    int embedded)
{
	struct sig sig;
	struct pubkey pubkey;
	unsigned long long msglen, siglen = 0;
	uint8_t *msg;
	int fd;

	msg = readmsg(embedded ? sigfile : msgfile, &msglen);

	readb64file(pubkeyfile, &pubkey, sizeof(pubkey), NULL);
	if (embedded) {
		siglen = parseb64file(sigfile, msg, &sig, sizeof(sig), NULL);
		msg += siglen;
		msglen -= siglen;
	} else {
		readb64file(sigfile, &sig, sizeof(sig), NULL);
	}

	if (memcmp(pubkey.fingerprint, sig.fingerprint, FPLEN)) {
#ifndef VERIFYONLY
		inspect(NULL, pubkeyfile, sigfile);
#endif
		errx(1, "verification failed: checked against wrong key");
	}

	verifymsg(pubkey.pubkey, msg, msglen, sig.sig);
	if (embedded) {
		fd = xopen(msgfile, O_CREAT|O_TRUNC|O_NOFOLLOW|O_RDWR, 0666);
		writeall(fd, msg, msglen, msgfile);
		close(fd);
	}

	printf("verified\n");

	free(msg - siglen);
}

int
main(int argc, char **argv)
{
	const char *pubkeyfile = NULL, *seckeyfile = NULL, *msgfile = NULL,
	    *sigfile = NULL;
	char sigfilebuf[1024];
	int ch, rounds;
	int embedded = 0;
	enum {
		NONE,
		GENERATE,
		INSPECT,
		SIGN,
		VERIFY
	} verb = NONE;


	rounds = 42;

	while ((ch = getopt(argc, argv, "GISVeno:p:s:")) != -1) {
		switch (ch) {
#ifndef VERIFYONLY
		case 'G':
			if (verb)
				usage();
			verb = GENERATE;
			break;
		case 'I':
			if (verb)
				usage();
			verb = INSPECT;
			break;
		case 'S':
			if (verb)
				usage();
			verb = SIGN;
			break;
#endif
		case 'V':
			if (verb)
				usage();
			verb = VERIFY;
			break;
		case 'e':
			embedded = 1;
			break;
		case 'n':
			rounds = 0;
			break;
		case 'o':
			sigfile = optarg;
			break;
		case 'p':
			pubkeyfile = optarg;
			break;
		case 's':
			seckeyfile = optarg;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

#ifdef VERIFYONLY
	if (verb != VERIFY)
#else
	if (verb == NONE)
#endif
		usage();

#ifndef VERIFYONLY
	if (verb == GENERATE) {
		if (!pubkeyfile || !seckeyfile || argc != 0)
			usage();
		generate(pubkeyfile, seckeyfile, rounds);
	} else if (verb == INSPECT) {
		if (argc != 0)
			usage();
		inspect(seckeyfile, pubkeyfile, sigfile);
	} else
#endif
	{
		if (argc != 1)
			usage();

		msgfile = argv[0];

		if (!sigfile) {
			if (snprintf(sigfilebuf, sizeof(sigfilebuf), "%s.sig",
			    msgfile) >= sizeof(sigfilebuf))
				errx(1, "path too long");
			sigfile = sigfilebuf;
		}
#ifndef VERIFYONLY
		if (verb == SIGN) {
			if (!seckeyfile)
				usage();
			sign(seckeyfile, msgfile, sigfile, embedded);
		} else
#endif
		if (verb == VERIFY) {
			if (!pubkeyfile)
				usage();
			verify(pubkeyfile, msgfile, sigfile, embedded);
		}
	}

	return 0;
}
