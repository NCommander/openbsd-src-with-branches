/*	$OpenBSD$	*/
/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      H�gskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: hstrerror.c,v 1.2 1998/03/28 23:16:45 rb Exp $");
#endif

#include "roken.h"

#ifndef HAVE_HSTRERROR

#include <stdio.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifndef HAVE_H_ERRNO
int h_errno = -17; /* Some magic number */
#endif

#if !(defined(HAVE_H_ERRLIST) && defined(HAVE_H_NERR))
static const char *const h_errlist[] = {
    "Resolver Error 0 (no error)",
    "Unknown host",		/* 1 HOST_NOT_FOUND */
    "Host name lookup failure",	/* 2 TRY_AGAIN */
    "Unknown server error",	/* 3 NO_RECOVERY */
    "No address associated with name", /* 4 NO_ADDRESS */
};

static
const
int h_nerr = { sizeof h_errlist / sizeof h_errlist[0] };
#else

#ifndef HAVE_H_ERRLIST_DECLARATION
extern const char *h_errlist[];
extern int h_nerr;
#endif

#endif

#ifdef NEED_HSTRERROR_CONST
const
#endif
char *
hstrerror(int herr)
{
    if (0 <= herr && herr < h_nerr)
	return (char *) h_errlist[herr];
    else if(herr == -17)
	return "unknown error";
    else
	return "Error number out of range (hstrerror)";
}

#endif
