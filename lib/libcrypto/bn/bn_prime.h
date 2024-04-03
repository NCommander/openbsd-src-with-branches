/* $OpenBSD: bn_prime.h,v 1.8 2022/11/09 22:52:51 tb Exp $ */
/*
 * Public domain.
 */

#include <stdint.h>

__BEGIN_HIDDEN_DECLS

#define NUMPRIMES 2048

extern const uint16_t primes[NUMPRIMES];

__END_HIDDEN_DECLS
