/*	$Id: pkcs.c,v 1.20 1999/06/15 11:20:19 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/param.h>
#include <gmp.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "gmp_util.h"
#include "log.h"
#include "asn.h"
#include "asn_useful.h"
#include "pkcs.h"

struct norm_type RSAPublicKey[] = {
  { TAG_INTEGER, UNIVERSAL, "modulus", 0, 0 },		/* modulus */
  { TAG_INTEGER, UNIVERSAL, "publicExponent", 0, 0 },	/* public exponent */
  { TAG_STOP, 0, 0, 0, 0 }
};

struct norm_type RSAPrivateKey[] = {
  { TAG_INTEGER, UNIVERSAL, "version", 1, "" },		/* version */
  { TAG_INTEGER, UNIVERSAL, "modulus", 0, 0 },		/* modulus */
  { TAG_INTEGER, UNIVERSAL, "publicExponent", 0, 0 },	/* public exponent */
  { TAG_INTEGER, UNIVERSAL, "privateExponent", 0, 0 },	/* private exponent */
  { TAG_INTEGER, UNIVERSAL, "prime1", 0, 0 },		/* p */
  { TAG_INTEGER, UNIVERSAL, "prime2", 0, 0 },		/* q */
  { TAG_INTEGER, UNIVERSAL, "exponent1", 0, 0 },	/* d mod (p-1) */
  { TAG_INTEGER, UNIVERSAL, "exponent2", 0, 0 },	/* d mod (q-1) */
  { TAG_INTEGER, UNIVERSAL, "coefficient", 0, 0 },	/* inv. of q mod p */
  { TAG_STOP, 0, 0, 0, 0 }
};

/*
 * Fill in the data field in struct norm_type with the octet data
 * from n.
 */
int
pkcs_mpz_to_norm_type (struct norm_type *obj, mpz_ptr n)
{
  mpz_ptr p;

  p = malloc (sizeof *p);
  if (!p)
    {
      log_error ("pkcs_mpz_to_norm_type: malloc (%d) failed", sizeof *p);
      return 0;
    }

  mpz_init_set (p, n);

  obj->len = sizeof *p;
  obj->data = p;

  return 1;
}

/*
 * Given the modulus and the public key, return an BER ASN.1 encoded
 * PKCS#1 compliant RSAPublicKey object.
 */
u_int8_t *
pkcs_public_key_to_asn (struct rsa_public_key *pub)
{
  u_int8_t *erg;
  struct norm_type *key, seq = {TAG_SEQUENCE, UNIVERSAL, 0, 0, 0 };

  seq.data = &RSAPublicKey;
  asn_template_clone (&seq, 1);
  key = seq.data;
  if (!key)
    return 0;

  if (!pkcs_mpz_to_norm_type (&key[0], pub->n))
    {
      free (key);
      return 0;
    }

  if (!pkcs_mpz_to_norm_type (&key[1], pub->e))
    {
      free (key[0].data); 
      free (key);
      return 0;
    }

  erg = asn_encode_sequence (&seq, 0);

  asn_free (&seq);

  return erg;
}

/*
 * Initalizes and Set's a Public Key Structure from an ASN BER encoded
 * Public Key.
 */
int
pkcs_public_key_from_asn (struct rsa_public_key *pub, u_int8_t *asn,
			  u_int32_t len)
{
  struct norm_type *key, seq = {TAG_SEQUENCE, UNIVERSAL, 0, 0, 0 };

  mpz_init (pub->n);
  mpz_init (pub->e);

  seq.data = RSAPublicKey;
  asn_template_clone (&seq, 1);

  if (!seq.data)
    return 0;

  if (!asn_decode_sequence (asn, len, &seq))
    {
      asn_free (&seq);
      return 0;
    }

  key = seq.data;
  mpz_set (pub->n, (mpz_ptr)key[0].data);
  mpz_set (pub->e, (mpz_ptr)key[1].data);

  asn_free (&seq);
      
  return 1;
}

void
pkcs_free_public_key (struct rsa_public_key *pub)
{
  mpz_clear (pub->n);
  mpz_clear (pub->e);
}

/*
 * Get ASN.1 representation of PrivateKey.
 * XXX I am not sure if we need this.
 */
u_int8_t *
pkcs_private_key_to_asn (struct rsa_private_key *priv)
{
  struct norm_type *key, seq = { TAG_SEQUENCE, UNIVERSAL, 0, 0, 0 };
  u_int8_t *erg = 0;

  mpz_t tmp;

  seq.data = RSAPrivateKey;
  asn_template_clone (&seq, 1);
  key = seq.data;
  if (!key)
    return 0;

  if (!pkcs_mpz_to_norm_type (&key[1], priv->n))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[2], priv->e))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[3], priv->d))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[4], priv->p))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[5], priv->q))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[6], priv->d1))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[7], priv->d2))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[8], priv->qinv))
    goto done;

  mpz_init (tmp);
  mpz_set_ui (tmp, 0);

  if (!pkcs_mpz_to_norm_type (&key[0], tmp))
    goto done;

  erg = asn_encode_sequence (&seq, 0);

 done:
  mpz_clear (tmp);
  asn_free (&seq);


  return erg;
}

/*
 * Initalizes and Set's a Private Key Structure from an ASN BER encoded
 * Private Key.
 */
int
pkcs_private_key_from_asn (struct rsa_private_key *priv, u_int8_t *asn,
			   u_int32_t len)
{
  struct norm_type *key, seq = { TAG_SEQUENCE, UNIVERSAL, 0, 0, 0 };
  u_int8_t *erg;

  mpz_init (priv->n);
  mpz_init (priv->p);
  mpz_init (priv->q);
  mpz_init (priv->e);
  mpz_init (priv->d);
  mpz_init (priv->d1);
  mpz_init (priv->d2);
  mpz_init (priv->qinv);

  seq.data = RSAPrivateKey;
  asn_template_clone (&seq, 1);
  if (!seq.data)
    return 0;

  if (!(erg = asn_decode_sequence (asn, len, &seq)))
    goto done;

  key = seq.data;
  if (mpz_cmp_ui ((mpz_ptr)key[0].data, 0))
    {
      log_print ("pkcs_set_private_key: version too high");
      erg = 0;
      goto done;
    }

  mpz_set (priv->n, key[1].data);
  mpz_set (priv->e, key[2].data);
  mpz_set (priv->d, key[3].data);
  mpz_set (priv->p, key[4].data);
  mpz_set (priv->q, key[5].data);
  mpz_set (priv->d1, key[6].data);
  mpz_set (priv->d2, key[7].data);
  mpz_set (priv->qinv, key[8].data);

  mpz_init (priv->qinv_mul_q);

  mpz_mul (priv->qinv_mul_q, priv->qinv, priv->q);

 done:
  asn_free (&seq);

  return erg ? 1 : 0;
}

void
pkcs_free_private_key (struct rsa_private_key *priv)
{
  mpz_clear (priv->n);
  mpz_clear (priv->e);
  mpz_clear (priv->d);
  mpz_clear (priv->p);
  mpz_clear (priv->q);
  mpz_clear (priv->d1);
  mpz_clear (priv->d2);
  mpz_clear (priv->qinv);
  mpz_clear (priv->qinv_mul_q);
}

/*
 * Creates a PKCS#1 block with data and then uses the private
 * exponent to do RSA encryption, returned is an allocated buffer
 * with the encryption result.
 *
 * Either pub_key or priv_key must be specified
 *
 * XXX CRIPPLED in the OpenBSD version as RSA is patented in the US.
 */
int
pkcs_rsa_encrypt (int art, struct rsa_public_key *pub_key,
		  struct rsa_private_key *priv_key, u_int8_t *data,
		  u_int32_t len, u_int8_t **out, u_int32_t *outlen)
{
  /* XXX Always fail until we interface legal (in the US) RSA code.  */
  return 0;
}

/*
 * Private Key Decryption, the 'in'-buffer is being destroyed 
 * Either pub_key or priv_key must be specified
 *
 * XXX CRIPPLED in the OpenBSD version as RSA is patented in the US.
 */
int
pkcs_rsa_decrypt (int art, struct rsa_public_key *pub_key,
		  struct rsa_private_key *priv_key, u_int8_t *in,
		  u_int8_t **out, u_int16_t *outlen)
{
  /* XXX Always fail until we interface legal (in the US) RSA code.  */
  return 0;
}

/*
 * Generates a keypair suitable to be used for RSA. No checks are done
 * on the generated key material. The following criteria might be
 * enforced: p and q chosen randomly, |p-q| should be large, (p+1), (q+1),
 * (p-1), (q-1) should have a large prime factor to be resistant e.g. 
 * against Pollard p-1 and Pollard p+1 factoring algorithms.
 * For p-1 and q-1 the large prime factor itself - 1 should have a large
 * prime factor.
 *
 * XXX CRIPPLED in the OpenBSD version as RSA is patented in the US.
 */
int
pkcs_generate_rsa_keypair (struct rsa_public_key *pubk, 
			   struct rsa_private_key *seck, u_int32_t bits)
{
-  /* XXX Always fail until we interface legal (in the US) RSA code.  */
-  return 0;
}

/* Generate a random prime with at most bits significant bits */
int
pkcs_generate_prime (mpz_ptr p, u_int32_t bits)
{
  u_int32_t tmp, i;

  mpz_set_ui (p, 0);
  i = tmp = 0;
  while (bits > 0)
    {
      tmp = sysdep_random ();

      if (i++ == 0)
	{ 
	  if (bits & 0x1f)
	    tmp &= (1 << (bits & 0x1f)) - 1;
	  tmp |= 1 << ((bits - 1) & 0x1f);
	}

      mpz_mul_2exp (p, p, 32);
      mpz_add_ui (p, p, tmp);

      bits -= (bits & 0x1f ? bits & 0x1f : 32);
    }

  /* Make p odd */
  mpz_setbit (p, 0);

  /* Iterate as long as p is not a probable prime */
  while (!mpz_probab_prime_p (p, 50))
    mpz_add_ui (p, p, 2);

  return 1;
}
