/* 	$OpenBSD: common.h,v 1.1 2014/06/24 01:14:18 djm Exp $ */
/*
 * Helpers for key API tests
 *
 * Placed in the public domain
 */

/* Load a binary file into a buffer */
struct sshbuf *load_file(const char *name);

/* Load a text file into a buffer */
struct sshbuf *load_text_file(const char *name);

/* Load a bignum from a file */
BIGNUM *load_bignum(const char *name);

/* Accessors for key components */
const BIGNUM *rsa_n(struct sshkey *k);
const BIGNUM *rsa_e(struct sshkey *k);
const BIGNUM *rsa_p(struct sshkey *k);
const BIGNUM *rsa_q(struct sshkey *k);
const BIGNUM *dsa_g(struct sshkey *k);
const BIGNUM *dsa_pub_key(struct sshkey *k);
const BIGNUM *dsa_priv_key(struct sshkey *k);

