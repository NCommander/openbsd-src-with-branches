/*	$OpenBSD: x509.c,v 1.26 2021/10/24 13:45:19 job Exp $ */
/*
 * Copyright (c) 2021 Claudio Jeker <claudio@openbsd.org>
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

#include <assert.h>
#include <err.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/x509v3.h>

#include "extern.h"

static ASN1_OBJECT	*bgpsec_oid;	/* id-kp-bgpsec-router */

static void
init_oid(void)
{
	if ((bgpsec_oid = OBJ_txt2obj("1.3.6.1.5.5.7.3.30", 1)) == NULL)
		errx(1, "OBJ_txt2obj for %s failed", "1.3.6.1.5.5.7.3.30");
}

/*
 * Parse X509v3 authority key identifier (AKI), RFC 6487 sec. 4.8.3.
 * Returns the AKI or NULL if it could not be parsed.
 * The AKI is formatted as a hex string.
 */
char *
x509_get_aki(X509 *x, int ta, const char *fn)
{
	const unsigned char	*d;
	AUTHORITY_KEYID		*akid;
	ASN1_OCTET_STRING	*os;
	int			 dsz, crit;
	char			*res = NULL;

	akid = X509_get_ext_d2i(x, NID_authority_key_identifier, &crit, NULL);
	if (akid == NULL) {
		if (!ta)
			warnx("%s: RFC 6487 section 4.8.3: AKI: "
			    "extension missing", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.3: "
		    "AKI: extension not non-critical", fn);
		goto out;
	}
	if (akid->issuer != NULL || akid->serial != NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "authorityCertIssuer or authorityCertSerialNumber present",
		    fn);
		goto out;
	}

	os = akid->keyid;
	if (os == NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "Key Identifier missing", fn);
		goto out;
	}

	d = os->data;
	dsz = os->length;

	if (dsz != SHA_DIGEST_LENGTH) {
		warnx("%s: RFC 6487 section 4.8.2: AKI: "
		    "want %d bytes SHA1 hash, have %d bytes",
		    fn, SHA_DIGEST_LENGTH, dsz);
		goto out;
	}

	res = hex_encode(d, dsz);

out:
	AUTHORITY_KEYID_free(akid);
	return res;
}

/*
 * Parse X509v3 subject key identifier (SKI), RFC 6487 sec. 4.8.2.
 * Returns the SKI or NULL if it could not be parsed.
 * The SKI is formatted as a hex string.
 */
char *
x509_get_ski(X509 *x, const char *fn)
{
	const unsigned char	*d;
	ASN1_OCTET_STRING	*os;
	int			 dsz, crit;
	char			*res = NULL;

	os = X509_get_ext_d2i(x, NID_subject_key_identifier, &crit, NULL);
	if (os == NULL) {
		warnx("%s: RFC 6487 section 4.8.2: SKI: extension missing", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.2: "
		    "SKI: extension not non-critical", fn);
		goto out;
	}

	d = os->data;
	dsz = os->length;

	if (dsz != SHA_DIGEST_LENGTH) {
		warnx("%s: RFC 6487 section 4.8.2: SKI: "
		    "want %d bytes SHA1 hash, have %d bytes",
		    fn, SHA_DIGEST_LENGTH, dsz);
		goto out;
	}

	res = hex_encode(d, dsz);
out:
	ASN1_OCTET_STRING_free(os);
	return res;
}

/*
 * Check the certificate's purpose: CA or BGPsec Router.
 * Return a member of enum cert_purpose.
 */
enum cert_purpose
x509_get_purpose(X509 *x, const char *fn)
{
	EXTENDED_KEY_USAGE		*eku = NULL;
	int				 crit;
	enum cert_purpose		 purpose = 0;

	if (X509_check_ca(x) == 1) {
		purpose = CERT_PURPOSE_CA;
		goto out;
	}

	eku = X509_get_ext_d2i(x, NID_ext_key_usage, &crit, NULL);
	if (eku == NULL) {
		warnx("%s: EKU: extension missing", fn);
		goto out;
	}
	if (crit != 0) {
		warnx("%s: EKU: extension must not be marked critical", fn);
		goto out;
	}
	if (sk_ASN1_OBJECT_num(eku) != 1) {
		warnx("%s: EKU: expected 1 purpose, have %d", fn,
		    sk_ASN1_OBJECT_num(eku));
		goto out;
	}

	if (bgpsec_oid == NULL)
		init_oid();

	if (OBJ_cmp(bgpsec_oid, sk_ASN1_OBJECT_value(eku, 0)) == 0) {
		purpose = CERT_PURPOSE_BGPSEC_ROUTER;
		goto out;
	}

 out:
	EXTENDED_KEY_USAGE_free(eku);
	return purpose;
}

/*
 * Extract Subject Public Key Info (SPKI) from BGPsec X.509 Certificate.
 * Returns NULL on failure, on success return the SPKI as base64 encoded pubkey
 */
char *
x509_get_pubkey(X509 *x, const char *fn)
{
	EVP_PKEY	*pkey;
	EC_KEY		*eckey;
	int		 nid;
	const char	*cname;
	uint8_t		*pubkey = NULL;
	char		*res = NULL;
	int		 len;

	pkey = X509_get0_pubkey(x);
	if (pkey == NULL) {
		warnx("%s: X509_get_pubkey failed in %s", fn, __func__);
		goto out;
	}
	if (EVP_PKEY_base_id(pkey) != EVP_PKEY_EC) {
		warnx("%s: Expected EVP_PKEY_EC, got %d", fn,
		    EVP_PKEY_base_id(pkey));
		goto out;
	}

	eckey = EVP_PKEY_get0_EC_KEY(pkey);
	if (eckey == NULL) {
		warnx("%s: Incorrect key type", fn);
		goto out;
	}

	nid = EC_GROUP_get_curve_name(EC_KEY_get0_group(eckey));
	if (nid != NID_X9_62_prime256v1) {
		if ((cname = EC_curve_nid2nist(nid)) == NULL)
			cname = OBJ_nid2sn(nid);
		warnx("%s: Expected P-256, got %s", fn, cname);
		goto out;
	}

	if (!EC_KEY_check_key(eckey)) {
		warnx("%s: EC_KEY_check_key failed in %s", fn, __func__);
		goto out;
	}

	len = i2d_PUBKEY(pkey, &pubkey);
	if (len <= 0) {
		warnx("%s: i2d_PUBKEY failed in %s", fn, __func__);
		goto out;
	}

	if (base64_encode(pubkey, len, &res) == -1)
		errx(1, "base64_encode failed in %s", __func__);

 out:
	free(pubkey);
	return res;
}

/*
 * Parse the Authority Information Access (AIA) extension
 * See RFC 6487, section 4.8.7 for details.
 * Returns NULL on failure, on success returns the AIA URI
 * (which has to be freed after use).
 */
char *
x509_get_aia(X509 *x, const char *fn)
{
	ACCESS_DESCRIPTION		*ad;
	AUTHORITY_INFO_ACCESS		*info;
	char				*aia = NULL;
	int				 crit;

	info = X509_get_ext_d2i(x, NID_info_access, &crit, NULL);
	if (info == NULL) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: extension missing", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.7: "
		    "AIA: extension not non-critical", fn);
		goto out;
	}
	if (sk_ACCESS_DESCRIPTION_num(info) != 1) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: "
		    "want 1 element, have %d", fn,
		    sk_ACCESS_DESCRIPTION_num(info));
		goto out;
	}

	ad = sk_ACCESS_DESCRIPTION_value(info, 0);
	if (OBJ_obj2nid(ad->method) != NID_ad_ca_issuers) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: "
		    "expected caIssuers, have %d", fn, OBJ_obj2nid(ad->method));
		goto out;
	}
	if (ad->location->type != GEN_URI) {
		warnx("%s: RFC 6487 section 4.8.7: AIA: "
		    "want GEN_URI type, have %d", fn, ad->location->type);
		goto out;
	}

	aia = strndup(
	    ASN1_STRING_get0_data(ad->location->d.uniformResourceIdentifier),
	    ASN1_STRING_length(ad->location->d.uniformResourceIdentifier));
	if (aia == NULL)
		err(1, NULL);

out:
	AUTHORITY_INFO_ACCESS_free(info);
	return aia;
}

/*
 * Extract the expire time (not-after) of a certificate.
 */
time_t
x509_get_expire(X509 *x, const char *fn)
{
	const ASN1_TIME	*at;
	struct tm	 expires_tm;
	time_t		 expires;

	at = X509_get0_notAfter(x);
	if (at == NULL)
		errx(1, "%s: X509_get0_notafter failed", fn);
	memset(&expires_tm, 0, sizeof(expires_tm));
	if (ASN1_time_parse(at->data, at->length, &expires_tm, 0) == -1)
		errx(1, "%s: ASN1_time_parse failed", fn);

	if ((expires = mktime(&expires_tm)) == -1)
		errx(1, "%s: mktime failed", fn);

	return expires;
}

/*
 * Parse the very specific subset of information in the CRL distribution
 * point extension.
 * See RFC 6487, sectoin 4.8.6 for details.
 * Returns NULL on failure, the crl URI on success which has to be freed
 * after use.
 */
char *
x509_get_crl(X509 *x, const char *fn)
{
	CRL_DIST_POINTS		*crldp;
	DIST_POINT		*dp;
	GENERAL_NAME		*name;
	char			*crl = NULL;
	int			 crit;

	crldp = X509_get_ext_d2i(x, NID_crl_distribution_points, &crit, NULL);
	if (crldp == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "no CRL distribution point extension", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.6: "
		    "CRL distribution point: extension not non-critical", fn);
		goto out;
	}

	if (sk_DIST_POINT_num(crldp) != 1) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want 1 element, have %d", fn,
		    sk_DIST_POINT_num(crldp));
		goto out;
	}

	dp = sk_DIST_POINT_value(crldp, 0);
	if (dp->distpoint == NULL) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "no distribution point name", fn);
		goto out;
	}
	if (dp->distpoint->type != 0) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "expected GEN_OTHERNAME, have %d", fn, dp->distpoint->type);
		goto out;
	}

	if (sk_GENERAL_NAME_num(dp->distpoint->name.fullname) != 1) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want 1 full name, have %d", fn,
		    sk_GENERAL_NAME_num(dp->distpoint->name.fullname));
		goto out;
	}

	name = sk_GENERAL_NAME_value(dp->distpoint->name.fullname, 0);
	if (name->type != GEN_URI) {
		warnx("%s: RFC 6487 section 4.8.6: CRL: "
		    "want URI type, have %d", fn, name->type);
		goto out;
	}

	crl = strndup(ASN1_STRING_get0_data(name->d.uniformResourceIdentifier),
	    ASN1_STRING_length(name->d.uniformResourceIdentifier));
	if (crl == NULL)
		err(1, NULL);

out:
	CRL_DIST_POINTS_free(crldp);
	return crl;
}

/*
 * Parse X509v3 authority key identifier (AKI) from the CRL.
 * This is matched against the string from x509_get_ski() above.
 * Returns the AKI or NULL if it could not be parsed.
 * The AKI is formatted as a hex string.
 */
char *
x509_crl_get_aki(X509_CRL *crl, const char *fn)
{
	const unsigned char	*d;
	AUTHORITY_KEYID		*akid;
	ASN1_OCTET_STRING	*os;
	int			 dsz, crit;
	char			*res = NULL;

	akid = X509_CRL_get_ext_d2i(crl, NID_authority_key_identifier, &crit,
	    NULL);
	if (akid == NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: extension missing", fn);
		return NULL;
	}
	if (crit != 0) {
		warnx("%s: RFC 6487 section 4.8.3: "
		    "AKI: extension not non-critical", fn);
		goto out;
	}
	if (akid->issuer != NULL || akid->serial != NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "authorityCertIssuer or authorityCertSerialNumber present",
		    fn);
		goto out;
	}

	os = akid->keyid;
	if (os == NULL) {
		warnx("%s: RFC 6487 section 4.8.3: AKI: "
		    "Key Identifier missing", fn);
		goto out;
	}

	d = os->data;
	dsz = os->length;

	if (dsz != SHA_DIGEST_LENGTH) {
		warnx("%s: RFC 6487 section 4.8.2: AKI: "
		    "want %d bytes SHA1 hash, have %d bytes",
		    fn, SHA_DIGEST_LENGTH, dsz);
		goto out;
	}

	res = hex_encode(d, dsz);
out:
	AUTHORITY_KEYID_free(akid);
	return res;
}
