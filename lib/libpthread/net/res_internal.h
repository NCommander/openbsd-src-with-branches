/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	  must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)res_internal.h	6.22 (Berkeley) 3/19/91";*/
static char *rcsid = "$Id: res_internal.h,v 1.7.4.1 1996/02/09 05:39:55 ghudson Exp $";
#endif /* LIBC_SCCS and not lint */

#ifndef _RES_INTERNAL_H
#define _RES_INTERNAL_H

#include <pthread.h>
#include <netdb.h>
#include <resolv.h>

#define HOST_BUFSIZE 4096
#define ALIGN(p, t) ((char *)(((((long)(p) - 1) / sizeof(t)) + 1) * sizeof(t)))
#define SP(p, t, n) (ALIGN(p, t) + (n) * sizeof(t))

struct	res_data {
	char *buf;
	struct __res_state state;
	int errval;
	int sock;
};

#if PACKETSZ > 1024
#define MAXPACKET	PACKETSZ
#else
#define MAXPACKET	1024
#endif

typedef union {
	HEADER hdr;
	unsigned char buf[MAXPACKET];
} querybuf;

typedef union {
	long al;
	char ac;
} align;

extern pthread_mutex_t host_iterate_lock;

__BEGIN_DECLS
struct hostent *_res_parse_answer(querybuf *answer, int anslen, int iquery,
								  struct hostent *result, char *buf,
								  int buflen, int *errval);
void _res_set_error(int val);
struct res_data *_res_init(void);
__END_DECLS

#endif

