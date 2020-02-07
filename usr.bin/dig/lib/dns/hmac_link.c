/*
 * Portions Copyright (C) Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * See the COPYRIGHT file distributed with this work for additional
 * information regarding copyright ownership.
 *
 * Portions Copyright (C) Network Associates, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Principal Author: Brian Wellington
 * $Id: hmac_link.c,v 1.10 2020/01/26 11:22:33 florian Exp $
 */



#include <isc/buffer.h>
#include <isc/hmacmd5.h>
#include <isc/hmacsha.h>
#include <isc/md5.h>
#include <isc/sha1.h>

#include <isc/safe.h>
#include <string.h>
#include <isc/util.h>



#include <dst/result.h>

#include "dst_internal.h"
#include "dst_parse.h"

static isc_result_t
getkeybits(dst_key_t *key, struct dst_private_element *element) {

	if (element->length != 2)
		return (DST_R_INVALIDPRIVATEKEY);

	key->key_bits =	(element->data[0] << 8) + element->data[1];

	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha1_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha1_key {
	unsigned char key[ISC_SHA1_BLOCK_LENGTH];
};

static isc_result_t
hmacsha1_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha1_t *hmacsha1ctx;
	dst_hmacsha1_key_t *hkey = key->keydata.hmacsha1;

	hmacsha1ctx = malloc(sizeof(isc_hmacsha1_t));
	if (hmacsha1ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha1_init(hmacsha1ctx, hkey->key, ISC_SHA1_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha1ctx = hmacsha1ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha1_destroyctx(dst_context_t *dctx) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->ctxdata.hmacsha1ctx;

	if (hmacsha1ctx != NULL) {
		isc_hmacsha1_invalidate(hmacsha1ctx);
		free(hmacsha1ctx);
		dctx->ctxdata.hmacsha1ctx = NULL;
	}
}

static isc_result_t
hmacsha1_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->ctxdata.hmacsha1ctx;

	isc_hmacsha1_update(hmacsha1ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->ctxdata.hmacsha1ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA1_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha1_sign(hmacsha1ctx, digest, ISC_SHA1_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA1_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha1_t *hmacsha1ctx = dctx->ctxdata.hmacsha1ctx;

	if (sig->length > ISC_SHA1_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha1_verify(hmacsha1ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha1_compare(const dst_key_t *key1, const dst_key_t *key2) {
	dst_hmacsha1_key_t *hkey1, *hkey2;

	hkey1 = key1->keydata.hmacsha1;
	hkey2 = key2->keydata.hmacsha1;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (isc_safe_memequal(hkey1->key, hkey2->key, ISC_SHA1_BLOCK_LENGTH))
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha1_generate(dst_key_t *key, int pseudorandom_ok, void (*callback)(int)) {
	isc_buffer_t b;
	isc_result_t ret;
	unsigned int bytes;
	unsigned char data[ISC_SHA1_BLOCK_LENGTH];

	UNUSED(pseudorandom_ok);
	UNUSED(callback);

	bytes = (key->key_size + 7) / 8;
	if (bytes > ISC_SHA1_BLOCK_LENGTH) {
		bytes = ISC_SHA1_BLOCK_LENGTH;
		key->key_size = ISC_SHA1_BLOCK_LENGTH * 8;
	}

	memset(data, 0, ISC_SHA1_BLOCK_LENGTH);
	arc4random_buf(data, bytes);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha1_fromdns(key, &b);
	isc_safe_memwipe(data, sizeof(data));

	return (ret);
}

static isc_boolean_t
hmacsha1_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha1_destroy(dst_key_t *key) {
	dst_hmacsha1_key_t *hkey = key->keydata.hmacsha1;

	isc_safe_memwipe(hkey, sizeof(*hkey));
	free(hkey);
	key->keydata.hmacsha1 = NULL;
}

static isc_result_t
hmacsha1_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha1_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha1 != NULL);

	hkey = key->keydata.hmacsha1;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha1_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha1_t sha1ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha1_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA1_BLOCK_LENGTH) {
		isc_sha1_init(&sha1ctx);
		isc_sha1_update(&sha1ctx, r.base, r.length);
		isc_sha1_final(&sha1ctx, hkey->key);
		keylen = ISC_SHA1_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha1 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha1_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	unsigned int i;

	UNUSED(pub);
	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA1, lexer,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (key->external)
		result = DST_R_EXTERNALKEY;

	key->key_bits = 0;
	for (i = 0; i < priv.nelements && result == ISC_R_SUCCESS; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA1_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
					priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha1_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA1_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv);
	isc_safe_memwipe(&priv, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha1_functions = {
	hmacsha1_createctx,
	NULL, /*%< createctx2 */
	hmacsha1_destroyctx,
	hmacsha1_adddata,
	hmacsha1_sign,
	hmacsha1_verify,
	NULL, /* verify2 */
	NULL, /* computesecret */
	hmacsha1_compare,
	NULL, /* paramcompare */
	hmacsha1_generate,
	hmacsha1_isprivate,
	hmacsha1_destroy,
	hmacsha1_todns,
	hmacsha1_fromdns,
	NULL, /* hmacsha1_tofile */
	hmacsha1_parse,
	NULL, /* cleanup */
	NULL, /* fromlabel */
	NULL, /* dump */
	NULL, /* restore */
};

isc_result_t
dst__hmacsha1_init(dst_func_t **funcp) {
	/*
	 * Prevent use of incorrect crypto
	 */
	RUNTIME_CHECK(isc_sha1_check(ISC_FALSE));
	RUNTIME_CHECK(isc_hmacsha1_check(0));

	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha1_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha224_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha224_key {
	unsigned char key[ISC_SHA224_BLOCK_LENGTH];
};

static isc_result_t
hmacsha224_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha224_t *hmacsha224ctx;
	dst_hmacsha224_key_t *hkey = key->keydata.hmacsha224;

	hmacsha224ctx = malloc(sizeof(isc_hmacsha224_t));
	if (hmacsha224ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha224_init(hmacsha224ctx, hkey->key, ISC_SHA224_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha224ctx = hmacsha224ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha224_destroyctx(dst_context_t *dctx) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->ctxdata.hmacsha224ctx;

	if (hmacsha224ctx != NULL) {
		isc_hmacsha224_invalidate(hmacsha224ctx);
		free(hmacsha224ctx);
		dctx->ctxdata.hmacsha224ctx = NULL;
	}
}

static isc_result_t
hmacsha224_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->ctxdata.hmacsha224ctx;

	isc_hmacsha224_update(hmacsha224ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->ctxdata.hmacsha224ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA224_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha224_sign(hmacsha224ctx, digest, ISC_SHA224_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA224_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha224_t *hmacsha224ctx = dctx->ctxdata.hmacsha224ctx;

	if (sig->length > ISC_SHA224_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha224_verify(hmacsha224ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha224_compare(const dst_key_t *key1, const dst_key_t *key2) {
	dst_hmacsha224_key_t *hkey1, *hkey2;

	hkey1 = key1->keydata.hmacsha224;
	hkey2 = key2->keydata.hmacsha224;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (isc_safe_memequal(hkey1->key, hkey2->key, ISC_SHA224_BLOCK_LENGTH))
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha224_generate(dst_key_t *key, int pseudorandom_ok,
		    void (*callback)(int))
{
	isc_buffer_t b;
	isc_result_t ret;
	unsigned int bytes;
	unsigned char data[ISC_SHA224_BLOCK_LENGTH];

	UNUSED(pseudorandom_ok);
	UNUSED(callback);

	bytes = (key->key_size + 7) / 8;
	if (bytes > ISC_SHA224_BLOCK_LENGTH) {
		bytes = ISC_SHA224_BLOCK_LENGTH;
		key->key_size = ISC_SHA224_BLOCK_LENGTH * 8;
	}

	memset(data, 0, ISC_SHA224_BLOCK_LENGTH);
	arc4random_buf(data, bytes);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha224_fromdns(key, &b);
	isc_safe_memwipe(data, sizeof(data));

	return (ret);
}

static isc_boolean_t
hmacsha224_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha224_destroy(dst_key_t *key) {
	dst_hmacsha224_key_t *hkey = key->keydata.hmacsha224;

	isc_safe_memwipe(hkey, sizeof(*hkey));
	free(hkey);
	key->keydata.hmacsha224 = NULL;
}

static isc_result_t
hmacsha224_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha224_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha224 != NULL);

	hkey = key->keydata.hmacsha224;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha224_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha224_t sha224ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha224_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA224_BLOCK_LENGTH) {
		isc_sha224_init(&sha224ctx);
		isc_sha224_update(&sha224ctx, r.base, r.length);
		isc_sha224_final(hkey->key, &sha224ctx);
		keylen = ISC_SHA224_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha224 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha224_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	unsigned int i;

	UNUSED(pub);
	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA224, lexer,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (key->external)
		result = DST_R_EXTERNALKEY;

	key->key_bits = 0;
	for (i = 0; i < priv.nelements && result == ISC_R_SUCCESS; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA224_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
					priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha224_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA224_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv);
	isc_safe_memwipe(&priv, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha224_functions = {
	hmacsha224_createctx,
	NULL, /*%< createctx2 */
	hmacsha224_destroyctx,
	hmacsha224_adddata,
	hmacsha224_sign,
	hmacsha224_verify,
	NULL, /* verify2 */
	NULL, /* computesecret */
	hmacsha224_compare,
	NULL, /* paramcompare */
	hmacsha224_generate,
	hmacsha224_isprivate,
	hmacsha224_destroy,
	hmacsha224_todns,
	hmacsha224_fromdns,
	NULL, /* hmacsha224_tofile */
	hmacsha224_parse,
	NULL, /* cleanup */
	NULL, /* fromlabel */
	NULL, /* dump */
	NULL, /* restore */
};

isc_result_t
dst__hmacsha224_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha224_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha256_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha256_key {
	unsigned char key[ISC_SHA256_BLOCK_LENGTH];
};

static isc_result_t
hmacsha256_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha256_t *hmacsha256ctx;
	dst_hmacsha256_key_t *hkey = key->keydata.hmacsha256;

	hmacsha256ctx = malloc(sizeof(isc_hmacsha256_t));
	if (hmacsha256ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha256_init(hmacsha256ctx, hkey->key, ISC_SHA256_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha256ctx = hmacsha256ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha256_destroyctx(dst_context_t *dctx) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->ctxdata.hmacsha256ctx;

	if (hmacsha256ctx != NULL) {
		isc_hmacsha256_invalidate(hmacsha256ctx);
		free(hmacsha256ctx);
		dctx->ctxdata.hmacsha256ctx = NULL;
	}
}

static isc_result_t
hmacsha256_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->ctxdata.hmacsha256ctx;

	isc_hmacsha256_update(hmacsha256ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->ctxdata.hmacsha256ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA256_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha256_sign(hmacsha256ctx, digest, ISC_SHA256_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA256_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha256_t *hmacsha256ctx = dctx->ctxdata.hmacsha256ctx;

	if (sig->length > ISC_SHA256_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha256_verify(hmacsha256ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha256_compare(const dst_key_t *key1, const dst_key_t *key2) {
	dst_hmacsha256_key_t *hkey1, *hkey2;

	hkey1 = key1->keydata.hmacsha256;
	hkey2 = key2->keydata.hmacsha256;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (isc_safe_memequal(hkey1->key, hkey2->key, ISC_SHA256_BLOCK_LENGTH))
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha256_generate(dst_key_t *key, int pseudorandom_ok,
		    void (*callback)(int))
{
	isc_buffer_t b;
	isc_result_t ret;
	unsigned int bytes;
	unsigned char data[ISC_SHA256_BLOCK_LENGTH];

	UNUSED(pseudorandom_ok);
	UNUSED(callback);

	bytes = (key->key_size + 7) / 8;
	if (bytes > ISC_SHA256_BLOCK_LENGTH) {
		bytes = ISC_SHA256_BLOCK_LENGTH;
		key->key_size = ISC_SHA256_BLOCK_LENGTH * 8;
	}

	memset(data, 0, ISC_SHA256_BLOCK_LENGTH);
	arc4random_buf(data, bytes);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha256_fromdns(key, &b);
	isc_safe_memwipe(data, sizeof(data));

	return (ret);
}

static isc_boolean_t
hmacsha256_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha256_destroy(dst_key_t *key) {
	dst_hmacsha256_key_t *hkey = key->keydata.hmacsha256;

	isc_safe_memwipe(hkey, sizeof(*hkey));
	free(hkey);
	key->keydata.hmacsha256 = NULL;
}

static isc_result_t
hmacsha256_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha256_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha256 != NULL);

	hkey = key->keydata.hmacsha256;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha256_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha256_t sha256ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha256_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA256_BLOCK_LENGTH) {
		isc_sha256_init(&sha256ctx);
		isc_sha256_update(&sha256ctx, r.base, r.length);
		isc_sha256_final(hkey->key, &sha256ctx);
		keylen = ISC_SHA256_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha256 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha256_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	unsigned int i;

	UNUSED(pub);
	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA256, lexer,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (key->external)
		result = DST_R_EXTERNALKEY;

	key->key_bits = 0;
	for (i = 0; i < priv.nelements && result == ISC_R_SUCCESS; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA256_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
					priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha256_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA256_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv);
	isc_safe_memwipe(&priv, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha256_functions = {
	hmacsha256_createctx,
	NULL, /*%< createctx2 */
	hmacsha256_destroyctx,
	hmacsha256_adddata,
	hmacsha256_sign,
	hmacsha256_verify,
	NULL, /* verify2 */
	NULL, /* computesecret */
	hmacsha256_compare,
	NULL, /* paramcompare */
	hmacsha256_generate,
	hmacsha256_isprivate,
	hmacsha256_destroy,
	hmacsha256_todns,
	hmacsha256_fromdns,
	NULL, /* hmacsha256_tofile */
	hmacsha256_parse,
	NULL, /* cleanup */
	NULL, /* fromlabel */
	NULL, /* dump */
	NULL, /* restore */
};

isc_result_t
dst__hmacsha256_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha256_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha384_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha384_key {
	unsigned char key[ISC_SHA384_BLOCK_LENGTH];
};

static isc_result_t
hmacsha384_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha384_t *hmacsha384ctx;
	dst_hmacsha384_key_t *hkey = key->keydata.hmacsha384;

	hmacsha384ctx = malloc(sizeof(isc_hmacsha384_t));
	if (hmacsha384ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha384_init(hmacsha384ctx, hkey->key, ISC_SHA384_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha384ctx = hmacsha384ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha384_destroyctx(dst_context_t *dctx) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->ctxdata.hmacsha384ctx;

	if (hmacsha384ctx != NULL) {
		isc_hmacsha384_invalidate(hmacsha384ctx);
		free(hmacsha384ctx);
		dctx->ctxdata.hmacsha384ctx = NULL;
	}
}

static isc_result_t
hmacsha384_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->ctxdata.hmacsha384ctx;

	isc_hmacsha384_update(hmacsha384ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->ctxdata.hmacsha384ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA384_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha384_sign(hmacsha384ctx, digest, ISC_SHA384_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA384_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha384_t *hmacsha384ctx = dctx->ctxdata.hmacsha384ctx;

	if (sig->length > ISC_SHA384_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha384_verify(hmacsha384ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha384_compare(const dst_key_t *key1, const dst_key_t *key2) {
	dst_hmacsha384_key_t *hkey1, *hkey2;

	hkey1 = key1->keydata.hmacsha384;
	hkey2 = key2->keydata.hmacsha384;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (isc_safe_memequal(hkey1->key, hkey2->key, ISC_SHA384_BLOCK_LENGTH))
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha384_generate(dst_key_t *key, int pseudorandom_ok,
		    void (*callback)(int))
{
	isc_buffer_t b;
	isc_result_t ret;
	unsigned int bytes;
	unsigned char data[ISC_SHA384_BLOCK_LENGTH];

	UNUSED(pseudorandom_ok);
	UNUSED(callback);

	bytes = (key->key_size + 7) / 8;
	if (bytes > ISC_SHA384_BLOCK_LENGTH) {
		bytes = ISC_SHA384_BLOCK_LENGTH;
		key->key_size = ISC_SHA384_BLOCK_LENGTH * 8;
	}

	memset(data, 0, ISC_SHA384_BLOCK_LENGTH);
	arc4random_buf(data, bytes);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha384_fromdns(key, &b);
	isc_safe_memwipe(data, sizeof(data));

	return (ret);
}

static isc_boolean_t
hmacsha384_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha384_destroy(dst_key_t *key) {
	dst_hmacsha384_key_t *hkey = key->keydata.hmacsha384;

	isc_safe_memwipe(hkey, sizeof(*hkey));
	free(hkey);
	key->keydata.hmacsha384 = NULL;
}

static isc_result_t
hmacsha384_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha384_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha384 != NULL);

	hkey = key->keydata.hmacsha384;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha384_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha384_t sha384ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha384_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA384_BLOCK_LENGTH) {
		isc_sha384_init(&sha384ctx);
		isc_sha384_update(&sha384ctx, r.base, r.length);
		isc_sha384_final(hkey->key, &sha384ctx);
		keylen = ISC_SHA384_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha384 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha384_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	unsigned int i;

	UNUSED(pub);
	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA384, lexer,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (key->external)
		result = DST_R_EXTERNALKEY;

	key->key_bits = 0;
	for (i = 0; i < priv.nelements && result == ISC_R_SUCCESS; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA384_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
					priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha384_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA384_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv);
	isc_safe_memwipe(&priv, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha384_functions = {
	hmacsha384_createctx,
	NULL, /*%< createctx2 */
	hmacsha384_destroyctx,
	hmacsha384_adddata,
	hmacsha384_sign,
	hmacsha384_verify,
	NULL, /* verify2 */
	NULL, /* computesecret */
	hmacsha384_compare,
	NULL, /* paramcompare */
	hmacsha384_generate,
	hmacsha384_isprivate,
	hmacsha384_destroy,
	hmacsha384_todns,
	hmacsha384_fromdns,
	NULL, /* hmacsha384_tofile */
	hmacsha384_parse,
	NULL, /* cleanup */
	NULL, /* fromlabel */
	NULL, /* dump */
	NULL, /* restore */
};

isc_result_t
dst__hmacsha384_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha384_functions;
	return (ISC_R_SUCCESS);
}

static isc_result_t hmacsha512_fromdns(dst_key_t *key, isc_buffer_t *data);

struct dst_hmacsha512_key {
	unsigned char key[ISC_SHA512_BLOCK_LENGTH];
};

static isc_result_t
hmacsha512_createctx(dst_key_t *key, dst_context_t *dctx) {
	isc_hmacsha512_t *hmacsha512ctx;
	dst_hmacsha512_key_t *hkey = key->keydata.hmacsha512;

	hmacsha512ctx = malloc(sizeof(isc_hmacsha512_t));
	if (hmacsha512ctx == NULL)
		return (ISC_R_NOMEMORY);
	isc_hmacsha512_init(hmacsha512ctx, hkey->key, ISC_SHA512_BLOCK_LENGTH);
	dctx->ctxdata.hmacsha512ctx = hmacsha512ctx;
	return (ISC_R_SUCCESS);
}

static void
hmacsha512_destroyctx(dst_context_t *dctx) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->ctxdata.hmacsha512ctx;

	if (hmacsha512ctx != NULL) {
		isc_hmacsha512_invalidate(hmacsha512ctx);
		free(hmacsha512ctx);
		dctx->ctxdata.hmacsha512ctx = NULL;
	}
}

static isc_result_t
hmacsha512_adddata(dst_context_t *dctx, const isc_region_t *data) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->ctxdata.hmacsha512ctx;

	isc_hmacsha512_update(hmacsha512ctx, data->base, data->length);
	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->ctxdata.hmacsha512ctx;
	unsigned char *digest;

	if (isc_buffer_availablelength(sig) < ISC_SHA512_DIGESTLENGTH)
		return (ISC_R_NOSPACE);
	digest = isc_buffer_used(sig);
	isc_hmacsha512_sign(hmacsha512ctx, digest, ISC_SHA512_DIGESTLENGTH);
	isc_buffer_add(sig, ISC_SHA512_DIGESTLENGTH);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_verify(dst_context_t *dctx, const isc_region_t *sig) {
	isc_hmacsha512_t *hmacsha512ctx = dctx->ctxdata.hmacsha512ctx;

	if (sig->length > ISC_SHA512_DIGESTLENGTH || sig->length == 0)
		return (DST_R_VERIFYFAILURE);

	if (isc_hmacsha512_verify(hmacsha512ctx, sig->base, sig->length))
		return (ISC_R_SUCCESS);
	else
		return (DST_R_VERIFYFAILURE);
}

static isc_boolean_t
hmacsha512_compare(const dst_key_t *key1, const dst_key_t *key2) {
	dst_hmacsha512_key_t *hkey1, *hkey2;

	hkey1 = key1->keydata.hmacsha512;
	hkey2 = key2->keydata.hmacsha512;

	if (hkey1 == NULL && hkey2 == NULL)
		return (ISC_TRUE);
	else if (hkey1 == NULL || hkey2 == NULL)
		return (ISC_FALSE);

	if (isc_safe_memequal(hkey1->key, hkey2->key, ISC_SHA512_BLOCK_LENGTH))
		return (ISC_TRUE);
	else
		return (ISC_FALSE);
}

static isc_result_t
hmacsha512_generate(dst_key_t *key, int pseudorandom_ok,
		    void (*callback)(int))
{
	isc_buffer_t b;
	isc_result_t ret;
	unsigned int bytes;
	unsigned char data[ISC_SHA512_BLOCK_LENGTH];

	UNUSED(pseudorandom_ok);
	UNUSED(callback);

	bytes = (key->key_size + 7) / 8;
	if (bytes > ISC_SHA512_BLOCK_LENGTH) {
		bytes = ISC_SHA512_BLOCK_LENGTH;
		key->key_size = ISC_SHA512_BLOCK_LENGTH * 8;
	}

	memset(data, 0, ISC_SHA512_BLOCK_LENGTH);
	arc4random_buf(data, bytes);

	isc_buffer_init(&b, data, bytes);
	isc_buffer_add(&b, bytes);
	ret = hmacsha512_fromdns(key, &b);
	isc_safe_memwipe(data, sizeof(data));

	return (ret);
}

static isc_boolean_t
hmacsha512_isprivate(const dst_key_t *key) {
	UNUSED(key);
	return (ISC_TRUE);
}

static void
hmacsha512_destroy(dst_key_t *key) {
	dst_hmacsha512_key_t *hkey = key->keydata.hmacsha512;

	isc_safe_memwipe(hkey, sizeof(*hkey));
	free(hkey);
	key->keydata.hmacsha512 = NULL;
}

static isc_result_t
hmacsha512_todns(const dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha512_key_t *hkey;
	unsigned int bytes;

	REQUIRE(key->keydata.hmacsha512 != NULL);

	hkey = key->keydata.hmacsha512;

	bytes = (key->key_size + 7) / 8;
	if (isc_buffer_availablelength(data) < bytes)
		return (ISC_R_NOSPACE);
	isc_buffer_putmem(data, hkey->key, bytes);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_fromdns(dst_key_t *key, isc_buffer_t *data) {
	dst_hmacsha512_key_t *hkey;
	int keylen;
	isc_region_t r;
	isc_sha512_t sha512ctx;

	isc_buffer_remainingregion(data, &r);
	if (r.length == 0)
		return (ISC_R_SUCCESS);

	hkey = malloc(sizeof(dst_hmacsha512_key_t));
	if (hkey == NULL)
		return (ISC_R_NOMEMORY);

	memset(hkey->key, 0, sizeof(hkey->key));

	if (r.length > ISC_SHA512_BLOCK_LENGTH) {
		isc_sha512_init(&sha512ctx);
		isc_sha512_update(&sha512ctx, r.base, r.length);
		isc_sha512_final(hkey->key, &sha512ctx);
		keylen = ISC_SHA512_DIGESTLENGTH;
	} else {
		memmove(hkey->key, r.base, r.length);
		keylen = r.length;
	}

	key->key_size = keylen * 8;
	key->keydata.hmacsha512 = hkey;

	isc_buffer_forward(data, r.length);

	return (ISC_R_SUCCESS);
}

static isc_result_t
hmacsha512_parse(dst_key_t *key, isc_lex_t *lexer, dst_key_t *pub) {
	dst_private_t priv;
	isc_result_t result, tresult;
	isc_buffer_t b;
	unsigned int i;

	UNUSED(pub);
	/* read private key file */
	result = dst__privstruct_parse(key, DST_ALG_HMACSHA512, lexer,
				       &priv);
	if (result != ISC_R_SUCCESS)
		return (result);

	if (key->external)
		result = DST_R_EXTERNALKEY;

	key->key_bits = 0;
	for (i = 0; i < priv.nelements && result == ISC_R_SUCCESS; i++) {
		switch (priv.elements[i].tag) {
		case TAG_HMACSHA512_KEY:
			isc_buffer_init(&b, priv.elements[i].data,
					priv.elements[i].length);
			isc_buffer_add(&b, priv.elements[i].length);
			tresult = hmacsha512_fromdns(key, &b);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		case TAG_HMACSHA512_BITS:
			tresult = getkeybits(key, &priv.elements[i]);
			if (tresult != ISC_R_SUCCESS)
				result = tresult;
			break;
		default:
			result = DST_R_INVALIDPRIVATEKEY;
			break;
		}
	}
	dst__privstruct_free(&priv);
	isc_safe_memwipe(&priv, sizeof(priv));
	return (result);
}

static dst_func_t hmacsha512_functions = {
	hmacsha512_createctx,
	NULL, /*%< createctx2 */
	hmacsha512_destroyctx,
	hmacsha512_adddata,
	hmacsha512_sign,
	hmacsha512_verify,
	NULL, /* verify2 */
	NULL, /* computesecret */
	hmacsha512_compare,
	NULL, /* paramcompare */
	hmacsha512_generate,
	hmacsha512_isprivate,
	hmacsha512_destroy,
	hmacsha512_todns,
	hmacsha512_fromdns,
	NULL, /* hmacsha512_tofile */
	hmacsha512_parse,
	NULL, /* cleanup */
	NULL, /* fromlabel */
	NULL, /* dump */
	NULL, /* restore */
};

isc_result_t
dst__hmacsha512_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &hmacsha512_functions;
	return (ISC_R_SUCCESS);
}

/*! \file */
