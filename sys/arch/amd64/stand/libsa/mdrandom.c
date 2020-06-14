/*	$OpenBSD: random_amd64.S,v 1.6 2020/05/25 14:58:01 deraadt Exp $	*/

/*
 * Copyright (c) 2020 Theo de Raadt 
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

#include <sys/param.h>
#include <machine/psl.h>
#include <machine/specialreg.h>

#include "libsa.h"

int
mdrandom(char *buf, size_t buflen)
{
	u_int eax, ebx, ecx, edx;
	uint32_t hi, lo;
	int i;

	for (i = 0; i < buflen; i++) {
		__asm volatile("rdtsc" : "=d" (hi), "=a" (lo));
		hi ^= (hi >> 8) ^ (hi >> 16) ^ (hi >> 24);
		lo ^= (lo >> 8) ^ (lo >> 16) ^ (lo >> 24);
		buf[i] ^= hi;
		buf[i] ^= lo;
	}

	CPUID(1, eax, ebx, ecx, edx);
	if (ecx & CPUIDECX_RDRAND) {
		unsigned long rand;

		for (i = 0; i < buflen / sizeof(rand); i++) {
			__asm volatile("rdrand	%0\n" : "=r" (rand));
			((unsigned long *)buf)[i] ^= rand;
		}
	}

	CPUID(0, eax, ebx, ecx, edx);
	if (eax >= 7) {
		CPUID_LEAF(7, 0, eax, ebx, ecx, edx);
		if (ebx & SEFF0EBX_RDSEED) {
			unsigned long rand;

			for (i = 0; i < buflen / sizeof(rand); i++) {
				__asm volatile("rdseed	%0\n" : "=r" (rand));
				((unsigned long *)buf)[i] ^= rand;
			}
		}
	}
	return (0);
}
