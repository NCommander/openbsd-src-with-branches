/*	$OpenBSD: mft.c,v 1.107 2024/02/13 22:44:21 job Exp $ */
/*
 * Copyright (c) 2022 Theo Buehler <tb@openbsd.org>
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

#include <assert.h>
#include <err.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/bn.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/safestack.h>
#include <openssl/sha.h>
#include <openssl/stack.h>
#include <openssl/x509.h>

#include "extern.h"

/*
 * Parse results and data of the manifest file.
 */
struct	parse {
	const char	*fn; /* manifest file name */
	struct mft	*res; /* result object */
	int		 found_crl;
};

extern ASN1_OBJECT	*mft_oid;

/*
 * Types and templates for the Manifest eContent, RFC 6486, section 4.2.
 */

ASN1_ITEM_EXP FileAndHash_it;
ASN1_ITEM_EXP Manifest_it;

typedef struct {
	ASN1_IA5STRING	*file;
	ASN1_BIT_STRING	*hash;
} FileAndHash;

DECLARE_STACK_OF(FileAndHash);

#ifndef DEFINE_STACK_OF
#define sk_FileAndHash_dup(sk)		SKM_sk_dup(FileAndHash, (sk))
#define sk_FileAndHash_free(sk)		SKM_sk_free(FileAndHash, (sk))
#define sk_FileAndHash_num(sk)		SKM_sk_num(FileAndHash, (sk))
#define sk_FileAndHash_value(sk, i)	SKM_sk_value(FileAndHash, (sk), (i))
#define sk_FileAndHash_sort(sk)		SKM_sk_sort(FileAndHash, (sk))
#define sk_FileAndHash_set_cmp_func(sk, cmp) \
    SKM_sk_set_cmp_func(FileAndHash, (sk), (cmp))
#endif

typedef struct {
	ASN1_INTEGER		*version;
	ASN1_INTEGER		*manifestNumber;
	ASN1_GENERALIZEDTIME	*thisUpdate;
	ASN1_GENERALIZEDTIME	*nextUpdate;
	ASN1_OBJECT		*fileHashAlg;
	STACK_OF(FileAndHash)	*fileList;
} Manifest;

ASN1_SEQUENCE(FileAndHash) = {
	ASN1_SIMPLE(FileAndHash, file, ASN1_IA5STRING),
	ASN1_SIMPLE(FileAndHash, hash, ASN1_BIT_STRING),
} ASN1_SEQUENCE_END(FileAndHash);

ASN1_SEQUENCE(Manifest) = {
	ASN1_EXP_OPT(Manifest, version, ASN1_INTEGER, 0),
	ASN1_SIMPLE(Manifest, manifestNumber, ASN1_INTEGER),
	ASN1_SIMPLE(Manifest, thisUpdate, ASN1_GENERALIZEDTIME),
	ASN1_SIMPLE(Manifest, nextUpdate, ASN1_GENERALIZEDTIME),
	ASN1_SIMPLE(Manifest, fileHashAlg, ASN1_OBJECT),
	ASN1_SEQUENCE_OF(Manifest, fileList, FileAndHash),
} ASN1_SEQUENCE_END(Manifest);

DECLARE_ASN1_FUNCTIONS(Manifest);
IMPLEMENT_ASN1_FUNCTIONS(Manifest);

#define GENTIME_LENGTH 15

/*
 * Determine rtype corresponding to file extension. Returns RTYPE_INVALID
 * on error or unkown extension.
 */
enum rtype
rtype_from_file_extension(const char *fn)
{
	size_t	 sz;

	sz = strlen(fn);
	if (sz < 5)
		return RTYPE_INVALID;

	if (strcasecmp(fn + sz - 4, ".tal") == 0)
		return RTYPE_TAL;
	if (strcasecmp(fn + sz - 4, ".cer") == 0)
		return RTYPE_CER;
	if (strcasecmp(fn + sz - 4, ".crl") == 0)
		return RTYPE_CRL;
	if (strcasecmp(fn + sz - 4, ".mft") == 0)
		return RTYPE_MFT;
	if (strcasecmp(fn + sz - 4, ".roa") == 0)
		return RTYPE_ROA;
	if (strcasecmp(fn + sz - 4, ".gbr") == 0)
		return RTYPE_GBR;
	if (strcasecmp(fn + sz - 4, ".sig") == 0)
		return RTYPE_RSC;
	if (strcasecmp(fn + sz - 4, ".asa") == 0)
		return RTYPE_ASPA;
	if (strcasecmp(fn + sz - 4, ".tak") == 0)
		return RTYPE_TAK;
	if (strcasecmp(fn + sz - 4, ".csv") == 0)
		return RTYPE_GEOFEED;

	return RTYPE_INVALID;
}

/*
 * Validate that a filename listed on a Manifest only contains characters
 * permitted in draft-ietf-sidrops-6486bis section 4.2.2
 * Also ensure that there is exactly one '.'.
 */
static int
valid_mft_filename(const char *fn, size_t len)
{
	const unsigned char *c;

	if (!valid_filename(fn, len))
		return 0;

	c = memchr(fn, '.', len);
	if (c == NULL || c != memrchr(fn, '.', len))
		return 0;

	return 1;
}

/*
 * Check that the file is allowed to be part of a manifest and the parser
 * for this type is implemented in rpki-client.
 * Returns corresponding rtype or RTYPE_INVALID to mark the file as unknown.
 */
static enum rtype
rtype_from_mftfile(const char *fn)
{
	enum rtype		 type;

	type = rtype_from_file_extension(fn);
	switch (type) {
	case RTYPE_CER:
	case RTYPE_CRL:
	case RTYPE_GBR:
	case RTYPE_ROA:
	case RTYPE_ASPA:
	case RTYPE_TAK:
		return type;
	default:
		return RTYPE_INVALID;
	}
}

/*
 * Parse an individual "FileAndHash", RFC 6486, sec. 4.2.
 * Return zero on failure, non-zero on success.
 */
static int
mft_parse_filehash(struct parse *p, const FileAndHash *fh)
{
	char			*fn = NULL;
	int			 rc = 0;
	struct mftfile		*fent;
	enum rtype		 type;
	size_t			 new_idx = 0;

	if (!valid_mft_filename(fh->file->data, fh->file->length)) {
		warnx("%s: RFC 6486 section 4.2.2: bad filename", p->fn);
		goto out;
	}
	fn = strndup(fh->file->data, fh->file->length);
	if (fn == NULL)
		err(1, NULL);

	if (fh->hash->length != SHA256_DIGEST_LENGTH) {
		warnx("%s: RFC 6486 section 4.2.1: hash: "
		    "invalid SHA256 length, have %d",
		    p->fn, fh->hash->length);
		goto out;
	}

	type = rtype_from_mftfile(fn);
	/* remember the filehash for the CRL in struct mft */
	if (type == RTYPE_CRL && strcmp(fn, p->res->crl) == 0) {
		memcpy(p->res->crlhash, fh->hash->data, SHA256_DIGEST_LENGTH);
		p->found_crl = 1;
	}

	if (filemode)
		fent = &p->res->files[p->res->filesz++];
	else {
		/* Fisher-Yates shuffle */
		new_idx = arc4random_uniform(p->res->filesz + 1);
		p->res->files[p->res->filesz++] = p->res->files[new_idx];
		fent = &p->res->files[new_idx];
	}

	fent->type = type;
	fent->file = fn;
	fn = NULL;
	memcpy(fent->hash, fh->hash->data, SHA256_DIGEST_LENGTH);

	rc = 1;
 out:
	free(fn);
	return rc;
}

static int
mft_fh_cmp_name(const FileAndHash *const *a, const FileAndHash *const *b)
{
	if ((*a)->file->length < (*b)->file->length)
		return -1;
	if ((*a)->file->length > (*b)->file->length)
		return 1;

	return memcmp((*a)->file->data, (*b)->file->data, (*b)->file->length);
}

static int
mft_fh_cmp_hash(const FileAndHash *const *a, const FileAndHash *const *b)
{
	assert((*a)->hash->length == SHA256_DIGEST_LENGTH);
	assert((*b)->hash->length == SHA256_DIGEST_LENGTH);

	return memcmp((*a)->hash->data, (*b)->hash->data, (*b)->hash->length);
}

/*
 * Assuming that the hash lengths are validated, this checks that all file names
 * and hashes in a manifest are unique. Returns 1 on success, 0 on failure.
 */
static int
mft_has_unique_names_and_hashes(const char *fn, const Manifest *mft)
{
	STACK_OF(FileAndHash)	*fhs;
	int			 i, ret = 0;

	if ((fhs = sk_FileAndHash_dup(mft->fileList)) == NULL)
		err(1, NULL);

	(void)sk_FileAndHash_set_cmp_func(fhs, mft_fh_cmp_name);
	sk_FileAndHash_sort(fhs);

	for (i = 0; i < sk_FileAndHash_num(fhs) - 1; i++) {
		const FileAndHash *curr = sk_FileAndHash_value(fhs, i);
		const FileAndHash *next = sk_FileAndHash_value(fhs, i + 1);

		if (mft_fh_cmp_name(&curr, &next) == 0) {
			warnx("%s: duplicate name: %.*s", fn,
			    curr->file->length, curr->file->data);
			goto err;
		}
	}

	(void)sk_FileAndHash_set_cmp_func(fhs, mft_fh_cmp_hash);
	sk_FileAndHash_sort(fhs);

	for (i = 0; i < sk_FileAndHash_num(fhs) - 1; i++) {
		const FileAndHash *curr = sk_FileAndHash_value(fhs, i);
		const FileAndHash *next = sk_FileAndHash_value(fhs, i + 1);

		if (mft_fh_cmp_hash(&curr, &next) == 0) {
			warnx("%s: duplicate hash for %.*s and %.*s", fn,
			    curr->file->length, curr->file->data,
			    next->file->length, next->file->data);
			goto err;
		}
	}

	ret = 1;

 err:
	sk_FileAndHash_free(fhs);

	return ret;
}

/*
 * Handle the eContent of the manifest object, RFC 6486 sec. 4.2.
 * Returns 0 on failure and 1 on success.
 */
static int
mft_parse_econtent(const unsigned char *d, size_t dsz, struct parse *p)
{
	const unsigned char	*oder;
	Manifest		*mft;
	FileAndHash		*fh;
	int			 i, rc = 0;

	oder = d;
	if ((mft = d2i_Manifest(NULL, &d, dsz)) == NULL) {
		warnx("%s: RFC 6486 section 4: failed to parse Manifest",
		    p->fn);
		goto out;
	}
	if (d != oder + dsz) {
		warnx("%s: %td bytes trailing garbage in eContent", p->fn,
		    oder + dsz - d);
		goto out;
	}

	if (!valid_econtent_version(p->fn, mft->version, 0))
		goto out;

	p->res->seqnum = x509_convert_seqnum(p->fn, mft->manifestNumber);
	if (p->res->seqnum == NULL)
		goto out;

	/*
	 * OpenSSL's DER decoder implementation will accept a GeneralizedTime
	 * which doesn't conform to RFC 5280. So, double check.
	 */
	if (ASN1_STRING_length(mft->thisUpdate) != GENTIME_LENGTH) {
		warnx("%s: embedded from time format invalid", p->fn);
		goto out;
	}
	if (ASN1_STRING_length(mft->nextUpdate) != GENTIME_LENGTH) {
		warnx("%s: embedded until time format invalid", p->fn);
		goto out;
	}

	if (!x509_get_time(mft->thisUpdate, &p->res->thisupdate)) {
		warn("%s: parsing manifest thisUpdate failed", p->fn);
		goto out;
	}
	if (!x509_get_time(mft->nextUpdate, &p->res->nextupdate)) {
		warn("%s: parsing manifest nextUpdate failed", p->fn);
		goto out;
	}

	if (p->res->thisupdate > p->res->nextupdate) {
		warnx("%s: bad update interval", p->fn);
		goto out;
	}

	if (OBJ_obj2nid(mft->fileHashAlg) != NID_sha256) {
		warnx("%s: RFC 6486 section 4.2.1: fileHashAlg: "
		    "want SHA256 object, have %s (NID %d)", p->fn,
		    ASN1_tag2str(OBJ_obj2nid(mft->fileHashAlg)),
		    OBJ_obj2nid(mft->fileHashAlg));
		goto out;
	}

	if (sk_FileAndHash_num(mft->fileList) >= MAX_MANIFEST_ENTRIES) {
		warnx("%s: %d exceeds manifest entry limit (%d)", p->fn,
		    sk_FileAndHash_num(mft->fileList), MAX_MANIFEST_ENTRIES);
		goto out;
	}

	p->res->files = calloc(sk_FileAndHash_num(mft->fileList),
	    sizeof(struct mftfile));
	if (p->res->files == NULL)
		err(1, NULL);

	for (i = 0; i < sk_FileAndHash_num(mft->fileList); i++) {
		fh = sk_FileAndHash_value(mft->fileList, i);
		if (!mft_parse_filehash(p, fh))
			goto out;
	}

	if (!p->found_crl) {
		warnx("%s: CRL not part of MFT fileList", p->fn);
		goto out;
	}

	if (!mft_has_unique_names_and_hashes(p->fn, mft))
		goto out;

	rc = 1;
 out:
	Manifest_free(mft);
	return rc;
}

/*
 * Parse the objects that have been published in the manifest.
 * Return mft if it conforms to RFC 6486, otherwise NULL.
 */
struct mft *
mft_parse(X509 **x509, const char *fn, int talid, const unsigned char *der,
    size_t len)
{
	struct parse	 p;
	struct cert	*cert = NULL;
	int		 rc = 0;
	size_t		 cmsz;
	unsigned char	*cms;
	char		*crldp = NULL, *crlfile;
	time_t		 signtime = 0;

	memset(&p, 0, sizeof(struct parse));
	p.fn = fn;

	cms = cms_parse_validate(x509, fn, der, len, mft_oid, &cmsz, &signtime);
	if (cms == NULL)
		return NULL;
	assert(*x509 != NULL);

	if ((p.res = calloc(1, sizeof(struct mft))) == NULL)
		err(1, NULL);
	p.res->signtime = signtime;

	if (!x509_get_aia(*x509, fn, &p.res->aia))
		goto out;
	if (!x509_get_aki(*x509, fn, &p.res->aki))
		goto out;
	if (!x509_get_sia(*x509, fn, &p.res->sia))
		goto out;
	if (!x509_get_ski(*x509, fn, &p.res->ski))
		goto out;
	if (p.res->aia == NULL || p.res->aki == NULL || p.res->sia == NULL ||
	    p.res->ski == NULL) {
		warnx("%s: RFC 6487 section 4.8: "
		    "missing AIA, AKI, SIA, or SKI X509 extension", fn);
		goto out;
	}

	if (!x509_inherits(*x509)) {
		warnx("%s: RFC 3779 extension not set to inherit", fn);
		goto out;
	}

	/* get CRL info for later */
	if (!x509_get_crl(*x509, fn, &crldp))
		goto out;
	if (crldp == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "missing CRL distribution point extension", fn);
		goto out;
	}
	crlfile = strrchr(crldp, '/');
	if (crlfile == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: "
		    "invalid CRL distribution point", fn);
		goto out;
	}
	crlfile++;
	if (!valid_mft_filename(crlfile, strlen(crlfile)) ||
	    rtype_from_file_extension(crlfile) != RTYPE_CRL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "bad CRL distribution point extension", fn);
		goto out;
	}
	if ((p.res->crl = strdup(crlfile)) == NULL)
		err(1, NULL);

	if (mft_parse_econtent(cms, cmsz, &p) == 0)
		goto out;

	if ((cert = cert_parse_ee_cert(fn, talid, *x509)) == NULL)
		goto out;

	if (p.res->signtime > p.res->nextupdate) {
		warnx("%s: dating issue: CMS signing-time after MFT nextUpdate",
		    fn);
		goto out;
	}

	rc = 1;
out:
	if (rc == 0) {
		mft_free(p.res);
		p.res = NULL;
		X509_free(*x509);
		*x509 = NULL;
	}
	free(crldp);
	cert_free(cert);
	free(cms);
	return p.res;
}

/*
 * Free an MFT pointer.
 * Safe to call with NULL.
 */
void
mft_free(struct mft *p)
{
	size_t	 i;

	if (p == NULL)
		return;

	if (p->files != NULL)
		for (i = 0; i < p->filesz; i++)
			free(p->files[i].file);

	free(p->aia);
	free(p->aki);
	free(p->sia);
	free(p->ski);
	free(p->path);
	free(p->files);
	free(p->seqnum);
	free(p);
}

/*
 * Serialise MFT parsed content into the given buffer.
 * See mft_read() for the other side of the pipe.
 */
void
mft_buffer(struct ibuf *b, const struct mft *p)
{
	size_t		 i;

	io_simple_buffer(b, &p->repoid, sizeof(p->repoid));
	io_simple_buffer(b, &p->talid, sizeof(p->talid));
	io_str_buffer(b, p->path);

	io_str_buffer(b, p->aia);
	io_str_buffer(b, p->aki);
	io_str_buffer(b, p->ski);

	io_simple_buffer(b, &p->filesz, sizeof(size_t));
	for (i = 0; i < p->filesz; i++) {
		io_str_buffer(b, p->files[i].file);
		io_simple_buffer(b, &p->files[i].type,
		    sizeof(p->files[i].type));
		io_simple_buffer(b, &p->files[i].location,
		    sizeof(p->files[i].location));
		io_simple_buffer(b, p->files[i].hash, SHA256_DIGEST_LENGTH);
	}
}

/*
 * Read an MFT structure from the file descriptor.
 * Result must be passed to mft_free().
 */
struct mft *
mft_read(struct ibuf *b)
{
	struct mft	*p = NULL;
	size_t		 i;

	if ((p = calloc(1, sizeof(struct mft))) == NULL)
		err(1, NULL);

	io_read_buf(b, &p->repoid, sizeof(p->repoid));
	io_read_buf(b, &p->talid, sizeof(p->talid));
	io_read_str(b, &p->path);

	io_read_str(b, &p->aia);
	io_read_str(b, &p->aki);
	io_read_str(b, &p->ski);
	assert(p->aia && p->aki && p->ski);

	io_read_buf(b, &p->filesz, sizeof(size_t));
	if ((p->files = calloc(p->filesz, sizeof(struct mftfile))) == NULL)
		err(1, NULL);

	for (i = 0; i < p->filesz; i++) {
		io_read_str(b, &p->files[i].file);
		io_read_buf(b, &p->files[i].type, sizeof(p->files[i].type));
		io_read_buf(b, &p->files[i].location,
		    sizeof(p->files[i].location));
		io_read_buf(b, p->files[i].hash, SHA256_DIGEST_LENGTH);
	}

	return p;
}

/*
 * Compare the thisupdate time of two mft files.
 */
int
mft_compare_issued(const struct mft *a, const struct mft *b)
{
	if (a->thisupdate > b->thisupdate)
		return 1;
	if (a->thisupdate < b->thisupdate)
		return -1;
	return 0;
}

/*
 * Compare the manifestNumber of two mft files.
 */
int
mft_compare_seqnum(const struct mft *a, const struct mft *b)
{
	int r;

	r = strlen(a->seqnum) - strlen(b->seqnum);
	if (r > 0)	/* seqnum in a is longer -> higher */
		return 1;
	if (r < 0)	/* seqnum in a is shorter -> smaller */
		return -1;

	r = strcmp(a->seqnum, b->seqnum);
	if (r > 0)	/* a is greater, prefer a */
		return 1;
	if (r < 0)	/* b is greater, prefer b */
		return -1;

	return 0;
}
