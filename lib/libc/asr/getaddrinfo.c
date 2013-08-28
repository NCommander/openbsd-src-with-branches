/*	$OpenBSD: getaddrinfo.c,v 1.2 2013/05/27 17:31:01 eric Exp $	*/
/*
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <netinet/in.h>

#include <errno.h>
#include <resolv.h>

#include "asr.h"

int
getaddrinfo(const char *hostname, const char *servname,
    const struct addrinfo *hints, struct addrinfo **res)
{
	struct async	*as;
	struct async_res ar;
	int		 saved_errno = errno;

	res_init();

	as = getaddrinfo_async(hostname, servname, hints, NULL);
	if (as == NULL) {
		if (errno == ENOMEM) {
			errno = saved_errno;
			return (EAI_MEMORY);
		}
		return (EAI_SYSTEM);
	}

	asr_async_run_sync(as, &ar);

	*res = ar.ar_addrinfo;
	if (ar.ar_gai_errno == EAI_SYSTEM)
		errno = ar.ar_errno;

	return (ar.ar_gai_errno);
}
