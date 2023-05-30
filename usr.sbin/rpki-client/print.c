/*	$OpenBSD: print.c,v 1.38 2023/04/26 18:17:50 tb Exp $ */
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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <openssl/evp.h>

#include "extern.h"
#include "json.h"

static const char *
pretty_key_id(const char *hex)
{
	static char buf[128];	/* bigger than SHA_DIGEST_LENGTH * 3 */
	size_t i;

	for (i = 0; i < sizeof(buf) && *hex != '\0'; i++) {
		if (i % 3 == 2)
			buf[i] = ':';
		else
			buf[i] = *hex++;
	}
	if (i == sizeof(buf))
		memcpy(buf + sizeof(buf) - 4, "...", 4);
	else
		buf[i] = '\0';
	return buf;
}

char *
time2str(time_t t)
{
	static char buf[64];
	struct tm tm;

	if (gmtime_r(&t, &tm) == NULL)
		return "could not convert time";

	strftime(buf, sizeof(buf), "%a %d %b %Y %T %z", &tm);

	return buf;
}

void
tal_print(const struct tal *p)
{
	char			*ski;
	EVP_PKEY		*pk;
	RSA			*r;
	const unsigned char	*der;
	unsigned char		*rder = NULL;
	unsigned char		 md[SHA_DIGEST_LENGTH];
	int			 rder_len;
	size_t			 i;

	der = p->pkey;
	pk = d2i_PUBKEY(NULL, &der, p->pkeysz);
	if (pk == NULL)
		errx(1, "d2i_PUBKEY failed in %s", __func__);

	r = EVP_PKEY_get0_RSA(pk);
	if (r == NULL)
		errx(1, "EVP_PKEY_get0_RSA failed in %s", __func__);
	if ((rder_len = i2d_RSAPublicKey(r, &rder)) <= 0)
		errx(1, "i2d_RSAPublicKey failed in %s", __func__);

	if (!EVP_Digest(rder, rder_len, md, NULL, EVP_sha1(), NULL))
		errx(1, "EVP_Digest failed in %s", __func__);

	ski = hex_encode(md, SHA_DIGEST_LENGTH);

	if (outformats & FORMAT_JSON) {
		json_do_string("type", "tal");
		json_do_string("name", p->descr);
		json_do_string("ski", pretty_key_id(ski));
		json_do_array("trust_anchor_locations");
		for (i = 0; i < p->urisz; i++)
			json_do_string("tal", p->uri[i]);
		json_do_end();
	} else {
		printf("Trust anchor name:        %s\n", p->descr);
		printf("Subject key identifier:   %s\n", pretty_key_id(ski));
		printf("Trust anchor locations:   ");
		for (i = 0; i < p->urisz; i++) {
			if (i > 0)
				printf("%26s", "");
			printf("%s\n", p->uri[i]);
		}
	}

	EVP_PKEY_free(pk);
	free(rder);
	free(ski);
}

void
x509_print(const X509 *x)
{
	const ASN1_INTEGER	*xserial;
	const X509_NAME		*xissuer;
	char			*issuer = NULL;
	char			*serial = NULL;

	if ((xissuer = X509_get_issuer_name(x)) == NULL) {
		warnx("X509_get_issuer_name failed");
		goto out;
	}

	if ((issuer = X509_NAME_oneline(xissuer, NULL, 0)) == NULL) {
		warnx("X509_NAME_oneline failed");
		goto out;
	}

	if ((xserial = X509_get0_serialNumber(x)) == NULL) {
		warnx("X509_get0_serialNumber failed");
		goto out;
	}

	if ((serial = x509_convert_seqnum(__func__, xserial)) == NULL)
		goto out;

	if (outformats & FORMAT_JSON) {
		json_do_string("cert_issuer", issuer);
		json_do_string("cert_serial", serial);
	} else {
		printf("Certificate issuer:       %s\n", issuer);
		printf("Certificate serial:       %s\n", serial);
	}

 out:
	free(issuer);
	free(serial);
}

static void
as_resources_print(struct cert_as *as, size_t asz)
{
	size_t i;

	for (i = 0; i < asz; i++) {
		if (outformats & FORMAT_JSON)
			json_do_object("resource");
		switch (as[i].type) {
		case CERT_AS_ID:
			if (outformats & FORMAT_JSON) {
				json_do_uint("asid", as[i].id);
			} else {
				if (i > 0)
					printf("%26s", "");
				printf("AS: %u", as[i].id);
			}
			break;
		case CERT_AS_INHERIT:
			if (outformats & FORMAT_JSON) {
				json_do_bool("asid_inherit", 1);
			} else {
				if (i > 0)
					printf("%26s", "");
				printf("AS: inherit");
			}
			break;
		case CERT_AS_RANGE:
			if (outformats & FORMAT_JSON) {
				json_do_object("asrange");
				json_do_uint("min", as[i].range.min);
				json_do_uint("max", as[i].range.max);
				json_do_end();
			} else {
				if (i > 0)
					printf("%26s", "");
				printf("AS: %u -- %u", as[i].range.min,
				    as[i].range.max);
			}
			break;
		}
		if (outformats & FORMAT_JSON)
			json_do_end();
		else
			printf("\n");
	}
}

static void
ip_resources_print(struct cert_ip *ips, size_t ipsz, size_t asz)
{
	char buf1[64], buf2[64];
	size_t i;
	int sockt;


	for (i = 0; i < ipsz; i++) {
		if (outformats & FORMAT_JSON)
			json_do_object("resource");
		switch (ips[i].type) {
		case CERT_IP_INHERIT:
			if (outformats & FORMAT_JSON) {
				json_do_bool("ip_inherit", 1);
			} else {
				if (i > 0 || asz > 0)
					printf("%26s", "");
				printf("IP: inherit");
			}
			break;
		case CERT_IP_ADDR:
			ip_addr_print(&ips[i].ip, ips[i].afi, buf1,
			    sizeof(buf1));
			if (outformats & FORMAT_JSON) {
				json_do_string("ip_prefix", buf1);
			} else {
				if (i > 0 || asz > 0)
					printf("%26s", "");
				printf("IP: %s", buf1);
			}
			break;
		case CERT_IP_RANGE:
			sockt = (ips[i].afi == AFI_IPV4) ?
			    AF_INET : AF_INET6;
			inet_ntop(sockt, ips[i].min, buf1, sizeof(buf1));
			inet_ntop(sockt, ips[i].max, buf2, sizeof(buf2));
			if (outformats & FORMAT_JSON) {
				json_do_object("ip_range");
				json_do_string("min", buf1);
				json_do_string("max", buf2);
				json_do_end();
			} else {
				if (i > 0 || asz > 0)
					printf("%26s", "");
				printf("IP: %s -- %s", buf1, buf2);
			}
			break;
		}
		if (outformats & FORMAT_JSON)
			json_do_end();
		else
			printf("\n");
	}
}

void
cert_print(const struct cert *p)
{
	if (outformats & FORMAT_JSON) {
		if (p->pubkey != NULL)
			json_do_string("type", "router_key");
		else
			json_do_string("type", "ca_cert");
		json_do_string("ski", pretty_key_id(p->ski));
		if (p->aki != NULL)
			json_do_string("aki", pretty_key_id(p->aki));
		x509_print(p->x509);
		if (p->aia != NULL)
			json_do_string("aia", p->aia);
		if (p->mft != NULL)
			json_do_string("manifest", p->mft);
		if (p->repo != NULL)
			json_do_string("carepository", p->repo);
		if (p->notify != NULL)
			json_do_string("notify_url", p->notify);
		if (p->pubkey != NULL)
			json_do_string("router_key", p->pubkey);
		json_do_int("valid_since", p->notbefore);
		json_do_int("valid_until", p->notafter);
		if (p->expires)
			json_do_int("expires", p->expires);
		json_do_array("subordinate_resources");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		if (p->aki != NULL)
			printf("Authority key identifier: %s\n",
			    pretty_key_id(p->aki));
		x509_print(p->x509);
		if (p->aia != NULL)
			printf("Authority info access:    %s\n", p->aia);
		if (p->mft != NULL)
			printf("Manifest:                 %s\n", p->mft);
		if (p->repo != NULL)
			printf("caRepository:             %s\n", p->repo);
		if (p->notify != NULL)
			printf("Notify URL:               %s\n", p->notify);
		if (p->pubkey != NULL) {
			printf("BGPsec ECDSA public key:  %s\n",
			    p->pubkey);
			printf("Router key not before:    %s\n",
			    time2str(p->notbefore));
			printf("Router key not after:     %s\n",
			    time2str(p->notafter));
		} else {
			printf("Certificate not before:   %s\n",
			    time2str(p->notbefore));
			printf("Certificate not after:    %s\n",
			    time2str(p->notafter));
		}
		printf("Subordinate resources:    ");
	}

	as_resources_print(p->as, p->asz);
	ip_resources_print(p->ips, p->ipsz, p->asz);

	if (outformats & FORMAT_JSON)
		json_do_end();
}

void
crl_print(const struct crl *p)
{
	STACK_OF(X509_REVOKED)	*revlist;
	X509_REVOKED *rev;
	ASN1_INTEGER *crlnum;
	X509_NAME *xissuer;
	int i;
	char *issuer, *serial;
	time_t t;

	if (outformats & FORMAT_JSON) {
		json_do_string("type", "crl");
		json_do_string("aki", pretty_key_id(p->aki));
	} else
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));

	xissuer = X509_CRL_get_issuer(p->x509_crl);
	issuer = X509_NAME_oneline(xissuer, NULL, 0);
	crlnum = X509_CRL_get_ext_d2i(p->x509_crl, NID_crl_number, NULL, NULL);
	serial = x509_convert_seqnum(__func__, crlnum);
	if (issuer != NULL && serial != NULL) {
		if (outformats & FORMAT_JSON) {
			json_do_string("crl_issuer", issuer);
			json_do_string("crl_serial", serial);
		} else {
			printf("CRL issuer:               %s\n", issuer);
			printf("CRL serial number:        %s\n", serial);
		}
	}
	free(issuer);
	free(serial);
	ASN1_INTEGER_free(crlnum);

	if (outformats & FORMAT_JSON) {
		json_do_int("valid_since", p->lastupdate);
		json_do_int("valid_until", p->nextupdate);
		json_do_array("revoked_certs");
	} else {
		printf("CRL last update:          %s\n",
		    time2str(p->lastupdate));
		printf("CRL next update:          %s\n",
		    time2str(p->nextupdate));
		printf("Revoked Certificates:\n");
	}

	revlist = X509_CRL_get_REVOKED(p->x509_crl);
	for (i = 0; i < sk_X509_REVOKED_num(revlist); i++) {
		rev = sk_X509_REVOKED_value(revlist, i);
		serial = x509_convert_seqnum(__func__,
		    X509_REVOKED_get0_serialNumber(rev));
		x509_get_time(X509_REVOKED_get0_revocationDate(rev), &t);
		if (serial != NULL) {
			if (outformats & FORMAT_JSON) {
				json_do_object("cert");
				json_do_string("serial", serial);
				json_do_string("date", time2str(t));
				json_do_end();
			} else
				printf("%25s Serial: %8s   Revocation Date: %s"
				    "\n", "", serial, time2str(t));
		}
		free(serial);
	}

	if (outformats & FORMAT_JSON)
		json_do_end();
	else if (i == 0)
		printf("No Revoked Certificates\n");
}

void
mft_print(const X509 *x, const struct mft *p)
{
	size_t i;
	char *hash;

	if (outformats & FORMAT_JSON) {
		json_do_string("type", "manifest");
		json_do_string("ski", pretty_key_id(p->ski));
		x509_print(x);
		json_do_string("aki", pretty_key_id(p->aki));
		json_do_string("aia", p->aia);
		json_do_string("sia", p->sia);
		json_do_string("manifest_number", p->seqnum);
		if (p->signtime != 0)
			json_do_int("signing_time", p->signtime);
		json_do_int("valid_since", p->thisupdate);
		json_do_int("valid_until", p->nextupdate);
		if (p->expires)
			json_do_int("expires", p->expires);
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		x509_print(x);
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		printf("Manifest number:          %s\n", p->seqnum);
		if (p->signtime != 0)
			printf("Signing time:             %s\n",
			    time2str(p->signtime));
		printf("Manifest this update:     %s\n", time2str(p->thisupdate));
		printf("Manifest next update:     %s\n", time2str(p->nextupdate));
		printf("Files and hashes:         ");
	}

	if (outformats & FORMAT_JSON)
		json_do_array("filesandhashes");
	for (i = 0; i < p->filesz; i++) {
		if (base64_encode(p->files[i].hash, sizeof(p->files[i].hash),
		    &hash) == -1)
			errx(1, "base64_encode failure");

		if (outformats & FORMAT_JSON) {
			json_do_object("filehash");
			json_do_string("filename", p->files[i].file);
			json_do_string("hash", hash);
			json_do_end();
		} else {
			if (i > 0)
				printf("%26s", "");
			printf("%zu: %s (hash: %s)\n", i + 1, p->files[i].file,
			    hash);
		}

		free(hash);
	}
	if (outformats & FORMAT_JSON)
		json_do_end();
}

void
roa_print(const X509 *x, const struct roa *p)
{
	char	 buf[128];
	size_t	 i;

	if (outformats & FORMAT_JSON) {
		json_do_string("type", "roa");
		json_do_string("ski", pretty_key_id(p->ski));
		x509_print(x);
		json_do_string("aki", pretty_key_id(p->aki));
		json_do_string("aia", p->aia);
		json_do_string("sia", p->sia);
		if (p->signtime != 0)
			json_do_int("signing_time", p->signtime);
		json_do_int("valid_since", p->notbefore);
		json_do_int("valid_until", p->notafter);
		if (p->expires)
			json_do_int("expires", p->expires);
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		if (p->signtime != 0)
			printf("Signing time:             %s\n",
			    time2str(p->signtime));
		printf("ROA not before:           %s\n",
		    time2str(p->notbefore));
		printf("ROA not after:            %s\n", time2str(p->notafter));
		printf("asID:                     %u\n", p->asid);
		printf("IP address blocks:        ");
	}

	if (outformats & FORMAT_JSON)
		json_do_array("vrps");
	for (i = 0; i < p->ipsz; i++) {
		ip_addr_print(&p->ips[i].addr,
		    p->ips[i].afi, buf, sizeof(buf));

		if (outformats & FORMAT_JSON) {
			json_do_object("vrp");
			json_do_string("prefix", buf);
			json_do_uint("asid", p->asid);
			json_do_uint("maxlen", p->ips[i].maxlength);
			json_do_end();
		} else {
			if (i > 0)
				printf("%26s", "");
			printf("%s maxlen: %hhu\n", buf, p->ips[i].maxlength);
		}
	}
	if (outformats & FORMAT_JSON)
		json_do_end();
}

void
gbr_print(const X509 *x, const struct gbr *p)
{
	if (outformats & FORMAT_JSON) {
		json_do_string("type", "gbr");
		json_do_string("ski", pretty_key_id(p->ski));
		x509_print(x);
		json_do_string("aki", pretty_key_id(p->aki));
		json_do_string("aia", p->aia);
		json_do_string("sia", p->sia);
		if (p->signtime != 0)
			json_do_int("signing_time", p->signtime);
		json_do_int("valid_since", p->notbefore);
		json_do_int("valid_until", p->notafter);
		if (p->expires)
			json_do_int("expires", p->expires);
		json_do_string("vcard", p->vcard);
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		if (p->signtime != 0)
			printf("Signing time:             %s\n",
			    time2str(p->signtime));
		printf("GBR not before:           %s\n",
		    time2str(p->notbefore));
		printf("GBR not after:            %s\n", time2str(p->notafter));
		printf("vcard:\n%s", p->vcard);
	}
}

void
rsc_print(const X509 *x, const struct rsc *p)
{
	char	*hash;
	size_t	 i;

	if (outformats & FORMAT_JSON) {
		json_do_string("type", "rsc");
		json_do_string("ski", pretty_key_id(p->ski));
		x509_print(x);
		json_do_string("aki", pretty_key_id(p->aki));
		json_do_string("aia", p->aia);
		if (p->signtime != 0)
			json_do_int("signing_time", p->signtime);
		json_do_int("valid_since", p->notbefore);
		json_do_int("valid_until", p->notafter);
		if (p->expires)
			json_do_int("expires", p->expires);
		json_do_array("signed_with_resources");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		x509_print(x);
		printf("Authority info access:    %s\n", p->aia);
		if (p->signtime != 0)
			printf("Signing time:             %s\n",
			    time2str(p->signtime));
		printf("RSC not before:           %s\n",
		    time2str(p->notbefore));
		printf("RSC not after:            %s\n", time2str(p->notafter));
		printf("Signed with resources:    ");
	}

	as_resources_print(p->as, p->asz);
	ip_resources_print(p->ips, p->ipsz, p->asz);

	if (outformats & FORMAT_JSON) {
		json_do_end();
		json_do_array("filenamesandhashes");
	} else
		printf("Filenames and hashes:     ");

	for (i = 0; i < p->filesz; i++) {
		if (base64_encode(p->files[i].hash, sizeof(p->files[i].hash),
		    &hash) == -1)
			errx(1, "base64_encode failure");

		if (outformats & FORMAT_JSON) {
			json_do_object("filehash");
			if (p->files[i].filename)
				json_do_string("filename",
				    p->files[i].filename);
			json_do_string("hash_digest", hash);
			json_do_end();
		} else {
			if (i > 0)
				printf("%26s", "");
			printf("%zu: %s (hash: %s)\n", i + 1,
			    p->files[i].filename ? p->files[i].filename
			    : "no filename", hash);
		}

		free(hash);
	}

	if (outformats & FORMAT_JSON)
		json_do_end();
}

static void
aspa_provider(uint32_t as, enum afi afi)
{
	if (outformats & FORMAT_JSON) {
		json_do_object("aspa");
		json_do_uint("asid", as);
		if (afi == AFI_IPV4)
			json_do_string("afi_limit", "ipv4");
		if (afi == AFI_IPV6)
			json_do_string("afi_limit", "ipv6");
		json_do_end();
	} else {
		printf("AS: %u", as);
		if (afi == AFI_IPV4)
			printf(" (IPv4 only)");
		if (afi == AFI_IPV6)
			printf(" (IPv6 only)");
		printf("\n");
	}
}

static void
aspa_providers(const struct aspa *a)
{
	size_t	i;
	int	hasv4 = 0, hasv6 = 0;

	for (i = 0; i < a->providersz; i++) {
		if ((outformats & FORMAT_JSON) == 0 && i > 0)
			printf("%26s", "");
		aspa_provider(a->providers[i].as, a->providers[i].afi);

		switch (a->providers[i].afi) {
		case AFI_IPV4:
			hasv4 = 1;
			break;
		case AFI_IPV6:
			hasv6 = 1;
			break;
		default:
			hasv4 = hasv6 = 1;
			break;
		}
	}

	if (!hasv4)
		aspa_provider(0, AFI_IPV4);
	if (!hasv6)
		aspa_provider(0, AFI_IPV6);
}

void
aspa_print(const X509 *x, const struct aspa *p)
{
	if (outformats & FORMAT_JSON) {
		json_do_string("type", "aspa");
		json_do_string("ski", pretty_key_id(p->ski));
		x509_print(x);
		json_do_string("aki", pretty_key_id(p->aki));
		json_do_string("aia", p->aia);
		json_do_string("sia", p->sia);
		if (p->signtime != 0)
			json_do_int("signing_time", p->signtime);
		json_do_int("valid_since", p->notbefore);
		json_do_int("valid_until", p->notafter);
		if (p->expires)
			json_do_int("expires", p->expires);
		json_do_uint("customer_asid", p->custasid);
		json_do_array("provider_set");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		if (p->signtime != 0)
			printf("Signing time:             %s\n",
			    time2str(p->signtime));
		printf("ASPA not before:          %s\n",
		    time2str(p->notbefore));
		printf("ASPA not after:           %s\n", time2str(p->notafter));
		printf("Customer ASID:            %u\n", p->custasid);
		printf("Provider set:             ");
	}

	aspa_providers(p);

	if (outformats & FORMAT_JSON)
		json_do_end();
}

static void
takey_print(char *name, const struct takey *t)
{
	char	*spki = NULL;
	size_t	 i, j = 0;

	if (base64_encode(t->pubkey, t->pubkeysz, &spki) != 0)
		errx(1, "base64_encode failed in %s", __func__);

	if (outformats & FORMAT_JSON) {
		json_do_object("takey");
		json_do_string("name", name);
		json_do_array("comments");
		for (i = 0; i < t->commentsz; i++)
			json_do_string("comment", t->comments[i]);
		json_do_end();
		json_do_array("uris");
		for (i = 0; i < t->urisz; i++)
			json_do_string("uri", t->uris[i]);
		json_do_end();
		json_do_string("spki", spki);
	} else {
		printf("TAL derived from the '%s' Trust Anchor Key:\n\n", name);

		for (i = 0; i < t->commentsz; i++)
			printf("\t# %s\n", t->comments[i]);
		printf("\n");
		for (i = 0; i < t->urisz; i++)
			printf("\t%s\n\t", t->uris[i]);
		for (i = 0; i < strlen(spki); i++) {
			printf("%c", spki[i]);
			if ((++j % 64) == 0)
				printf("\n\t");
		}
		printf("\n\n");
	}

	free(spki);
}

void
tak_print(const X509 *x, const struct tak *p)
{
	if (outformats & FORMAT_JSON) {
		json_do_string("type", "tak");
		json_do_string("ski", pretty_key_id(p->ski));
		x509_print(x);
		json_do_string("aki", pretty_key_id(p->aki));
		json_do_string("aia", p->aia);
		json_do_string("sia", p->sia);
		if (p->signtime != 0)
			json_do_int("signing_time", p->signtime);
		json_do_int("valid_since", p->notbefore);
		json_do_int("valid_until", p->notafter);
		if (p->expires)
			json_do_int("expires", p->expires);
		json_do_array("takeys");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		printf("Subject info access:      %s\n", p->sia);
		if (p->signtime != 0)
			printf("Signing time:             %s\n",
			    time2str(p->signtime));
		printf("TAK not before:           %s\n",
		    time2str(p->notbefore));
		printf("TAK not after:            %s\n", time2str(p->notafter));
	}

	takey_print("current", p->current);
	if (p->predecessor != NULL)
		takey_print("predecessor", p->predecessor);
	if (p->successor != NULL)
		takey_print("successor", p->successor);

	if (outformats & FORMAT_JSON)
		json_do_end();
}

void
geofeed_print(const X509 *x, const struct geofeed *p)
{
	char	 buf[128];
	size_t	 i;

	if (outformats & FORMAT_JSON) {
		json_do_string("type", "geofeed");
		json_do_string("ski", pretty_key_id(p->ski));
		x509_print(x);
		json_do_string("aki", pretty_key_id(p->aki));
		json_do_string("aia", p->aia);
		if (p->signtime != 0)
			json_do_int("signing_time", p->signtime);
		json_do_int("valid_since", p->notbefore);
		json_do_int("valid_until", p->notafter);
		if (p->expires)
			json_do_int("expires", p->expires);
		json_do_array("records");
	} else {
		printf("Subject key identifier:   %s\n", pretty_key_id(p->ski));
		x509_print(x);
		printf("Authority key identifier: %s\n", pretty_key_id(p->aki));
		printf("Authority info access:    %s\n", p->aia);
		if (p->signtime != 0)
			printf("Signing time:             %s\n",
			    time2str(p->signtime));
		printf("Geofeed not before:       %s\n",
		    time2str(p->notbefore));
		printf("Geofeed not after:        %s\n", time2str(p->notafter));
		printf("Geofeed CSV records:      ");
	}

	for (i = 0; i < p->geoipsz; i++) {
		if (p->geoips[i].ip->type != CERT_IP_ADDR)
			continue;

		ip_addr_print(&p->geoips[i].ip->ip, p->geoips[i].ip->afi, buf,
		    sizeof(buf));
		if (outformats & FORMAT_JSON) {
			json_do_object("geoip");
			json_do_string("prefix", buf);
			json_do_string("location", p->geoips[i].loc);
			json_do_end();
		} else {
			if (i > 0)
				printf("%26s", "");
			printf("IP: %s (%s)\n", buf, p->geoips[i].loc);
		}
	}

	if (outformats & FORMAT_JSON)
		json_do_end();
}
