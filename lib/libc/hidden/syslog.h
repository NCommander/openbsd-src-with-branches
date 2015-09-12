/*	$OpenBSD$	*/
/*
 * Copyright (c) 2015 Philip Guenther <guenther@openbsd.org>
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

#ifndef _LIBC_SYSLOG_H_
#define	_LIBC_SYSLOG_H_

#include_next <syslog.h>

int	sendsyslog(const char *, __size_t);
PROTO_NORMAL(sendsyslog);

__BEGIN_HIDDEN_DECLS
void	__vsyslog_r(int, struct syslog_data *, __size_t (*)(char *, __size_t),
	    const char *, __va_list);
__END_HIDDEN_DECLS

PROTO_DEPRECATED(closelog);
PROTO_NORMAL(closelog_r);
PROTO_DEPRECATED(openlog);
PROTO_NORMAL(openlog_r);
PROTO_DEPRECATED(setlogmask);
PROTO_NORMAL(setlogmask_r);
PROTO_NORMAL(syslog);
PROTO_NORMAL(syslog_r);
PROTO_NORMAL(vsyslog);
PROTO_NORMAL(vsyslog_r);

#endif /* !_LIBC_SYSLOG_H_ */
