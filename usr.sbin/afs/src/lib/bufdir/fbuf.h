/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska H�gskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

/* $KTH: fbuf.h,v 1.6 2000/10/02 22:59:38 lha Exp $ */

#ifndef _FBUF_H_
#define _FBUF_H_

#include <rx/rx.h>

typedef enum {
    FBUF_READ    = 0x01,
    FBUF_WRITE   = 0x02,
    FBUF_PRIVATE = 0x04,
    FBUF_SHARED  = 0x08
} fbuf_flags;

struct fbuf {
    void *buf;
    int fd;
    size_t len;
    fbuf_flags flags;
};

typedef struct fbuf fbuf;

int fbuf_create (fbuf *fbuf, int fd, size_t len, fbuf_flags flags);
int fbuf_truncate (fbuf *fbuf, size_t new_len);
int fbuf_end (fbuf *fbuf);
size_t fbuf_len (fbuf *f);
void *fbuf_buf (fbuf *f);

int copyrx2fd (struct rx_call *call, int fd, off_t off, size_t len);
int copyfd2rx (int fd, struct rx_call *call, off_t off, size_t len);

#endif /* _FBUF_H_ */
