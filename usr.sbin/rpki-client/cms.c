/*	$OpenBSD: cms.c,v 1.11 2021/10/26 10:52:49 claudio Exp $ */
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

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/cms.h>

#include "extern.h"

/*
 * Parse and validate a self-signed CMS message, where the signing X509
 * certificate has been hashed to dgst (optional).
 * Conforms to RFC 6488.
 * The eContentType of the message must be an oid object.
 * Return the eContent as a string and set "rsz" to be its length.
 */
unsigned char *
cms_parse_validate(X509 **xp, const char *fn, const unsigned char *der,
    size_t derlen, const ASN1_OBJECT *oid, size_t *rsz)
{
	const ASN1_OBJECT	*obj;
	ASN1_OCTET_STRING	**os = NULL;
	CMS_ContentInfo		*cms;
	int			 rc = 0;
	STACK_OF(X509)		*certs = NULL;
	unsigned char		*res = NULL;

	*rsz = 0;
	*xp = NULL;

	/* just fail for empty buffers, the warning was printed elsewhere */
	if (der == NULL)
		return NULL;

	if ((cms = d2i_CMS_ContentInfo(NULL, &der, derlen)) == NULL) {
		cryptowarnx("%s: RFC 6488: failed CMS parse", fn);
		goto out;
	}

	/*
	 * The CMS is self-signed with a signing certifiate.
	 * Verify that the self-signage is correct.
	 */

	if (!CMS_verify(cms, NULL, NULL, NULL, NULL,
	    CMS_NO_SIGNER_CERT_VERIFY)) {
		cryptowarnx("%s: RFC 6488: CMS not self-signed", fn);
		goto out;
	}

	/* RFC 6488 section 2.1.3.1: check the object's eContentType. */

	obj = CMS_get0_eContentType(cms);
	if (obj == NULL) {
		warnx("%s: RFC 6488 section 2.1.3.1: eContentType: "
		    "OID object is NULL", fn);
		goto out;
	}
	if (OBJ_cmp(obj, oid) != 0) {
		char buf[128], obuf[128];

		OBJ_obj2txt(buf, sizeof(buf), obj, 1);
		OBJ_obj2txt(obuf, sizeof(obuf), oid, 1);
		warnx("%s: RFC 6488 section 2.1.3.1: eContentType: "
		    "unknown OID: %s, want %s", fn, buf, obuf);
		goto out;
	}

	/*
	 * The self-signing certificate is further signed by the input
	 * signing authority according to RFC 6488, 2.1.4.
	 * We extract that certificate now for later verification.
	 */

	certs = CMS_get0_signers(cms);
	if (certs == NULL || sk_X509_num(certs) != 1) {
		warnx("%s: RFC 6488 section 2.1.4: eContent: "
		    "want 1 signer, have %d", fn, sk_X509_num(certs));
		goto out;
	}
	*xp = X509_dup(sk_X509_value(certs, 0));

	/* Verify that we have eContent to disseminate. */

	if ((os = CMS_get0_content(cms)) == NULL || *os == NULL) {
		warnx("%s: RFC 6488 section 2.1.4: "
		    "eContent: zero-length content", fn);
		goto out;
	}

	/*
	 * Extract and duplicate the eContent.
	 * The CMS framework offers us no other way of easily managing
	 * this information; and since we're going to d2i it anyway,
	 * simply pass it as the desired underlying types.
	 */

	if ((res = malloc((*os)->length)) == NULL)
		err(1, NULL);
	memcpy(res, (*os)->data, (*os)->length);
	*rsz = (*os)->length;

	rc = 1;
out:
	sk_X509_free(certs);
	CMS_ContentInfo_free(cms);

	if (rc == 0) {
		X509_free(*xp);
		*xp = NULL;
	}

	return res;
}

/*
 * Wrapper around ASN1_get_object() that preserves the current start
 * state and returns a more meaningful value.
 * Return zero on failure, non-zero on success.
 */
int
ASN1_frame(const char *fn, size_t sz,
	const unsigned char **cnt, long *cntsz, int *tag)
{
	int	 ret, pcls;

	ret = ASN1_get_object(cnt, cntsz, tag, &pcls, sz);
	if ((ret & 0x80)) {
		cryptowarnx("%s: ASN1_get_object", fn);
		return 0;
	}
	return ASN1_object_size((ret & 0x01) ? 2 : 0, *cntsz, *tag);
}

/*
 * Check the version field in eContent.
 * Returns -1 on failure, zero on success.
 */
int
cms_econtent_version(const char *fn, const unsigned char **d, size_t dsz,
	long *version)
{
	ASN1_INTEGER	*aint = NULL;
	long		 plen;
	int		 ptag, rc = -1;

	if (!ASN1_frame(fn, dsz, d, &plen, &ptag))
		goto out;
	if (ptag != 0) {
		warnx("%s: eContent version: expected explicit tag [0]", fn);
		goto out;
	}

	aint = d2i_ASN1_INTEGER(NULL, d, plen);
	if (aint == NULL) {
		cryptowarnx("%s: eContent version: failed d2i_ASN1_INTEGER",
		    fn);
		goto out;
	}

	*version = ASN1_INTEGER_get(aint);
	if (*version < 0) {
		warnx("%s: eContent version: expected positive integer, got:"
		    " %ld", fn, *version);
		goto out;
	}

	rc = 0;
out:
	ASN1_INTEGER_free(aint);
	return rc;
}
