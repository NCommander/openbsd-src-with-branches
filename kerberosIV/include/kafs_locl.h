/* $KTH: kafs_locl.h,v 1.7 1997/10/14 22:57:11 joda Exp $ */

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

#ifndef __KAFS_LOCL_H__
#define __KAFS_LOCL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <errno.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/filio.h>

#include <sys/syscall.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <netdb.h>

#include <arpa/nameser.h>
#include <resolv.h>

#include <kerberosIV/krb.h>
#include <kerberosIV/kafs.h>

#include "afssysdefs.h"

struct kafs_data;
typedef int (*afslog_uid_func_t)(struct kafs_data*, const char*, uid_t);

typedef int (*get_cred_func_t)(struct kafs_data*, const char*, const char*, 
				    const char*, CREDENTIALS*);

typedef char* (*get_realm_func_t)(struct kafs_data*, const char*);

typedef struct kafs_data {
    afslog_uid_func_t afslog_uid;
    get_cred_func_t get_cred;
    get_realm_func_t get_realm;
    void *data;
} kafs_data;

int _kafs_afslog_all_local_cells(kafs_data*, uid_t);

int _kafs_get_cred(kafs_data*, const char*, const char*, const char *, 
		  CREDENTIALS*);

#endif /* __KAFS_LOCL_H__ */
