/*

rsa.h

Author: Tatu Ylonen <ylo@cs.hut.fi>

Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
                   All rights reserved

Created: Fri Mar  3 22:01:06 1995 ylo

RSA key generation, encryption and decryption.

*/

/* RCSID("$Id: rsa.h,v 1.2 1999/05/04 11:59:06 bg Exp $"); */

#ifndef RSA_H
#define RSA_H

#include <ssl/bn.h>
#include <ssl/rsa.h>

/* Calls SSL RSA_generate_key, only copies to prv and pub */
void rsa_generate_key(RSA *prv, RSA *pub, unsigned int bits);

/* Indicates whether the rsa module is permitted to show messages on
   the terminal. */
void rsa_set_verbose(int verbose);

void rsa_public_encrypt(BIGNUM *out, BIGNUM *in, RSA *prv);
void rsa_private_decrypt(BIGNUM *out, BIGNUM *in, RSA *prv);

#endif /* RSA_H */
