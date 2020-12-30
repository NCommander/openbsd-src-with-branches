/*  $OpenBSD: $ */

/*
 * Public Domain, Authors:
 * - Daniel J. Bernstein
 * - Chitchanok Chuengsatiansup
 * - Tanja Lange
 * - Christine van Vredendaal
 */

#define int32_MINMAX(a,b) \
do { \
  int64_t ab = (int64_t)b ^ (int64_t)a; \
  int64_t c = (int64_t)b - (int64_t)a; \
  c ^= ab & (c ^ b); \
  c >>= 31; \
  c &= ab; \
  a ^= c; \
  b ^= c; \
} while(0)
