/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska H�gskolan
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

/*
 * $KTH: xfs.h,v 1.8 2000/10/02 22:33:30 lha Exp $
 */

#ifndef __XFS_H_V
#define __XFS_H_V 1

void xfs_probe_version (int fd, int version);

void xfs_message_init (void);
int  xfs_message_receive (int fd, struct xfs_message_header *h, u_int size);
int  xfs_message_receive (int fd, struct xfs_message_header *h, u_int size);
void xfs_send_message_gc_nodes (int fd, int num, VenusFid *fids);
int  xfs_message_wakeup (int fd, struct xfs_message_wakeup *h, u_int size);
int  xfs_message_sleep (u_int seqnum);
int  xfs_send_message_wakeup (int fd, u_int seqnum, int error);
int  xfs_send_message_wakeup_vmultiple (int fd,	u_int seqnum,
					int error, va_list args);
int  xfs_send_message_wakeup_multiple (int fd, u_int seqnum,
				       int error, ...);
int  xfs_send_message_wakeup_data (int fd, u_int seqnum, int error,
				   void *data, int size);
int  xfs_send_message_multiple_list (int fd, struct xfs_message_header *h,
				     size_t size, u_int num);
int  xfs_send_message_multiple (int fd, ...);
int  xfs_send_message_vmultiple (int fd, va_list args);

int  xfs_message_send (int fd, struct xfs_message_header *h, u_int size);
int  xfs_message_rpc (int fd, struct xfs_message_header *h, u_int size);

typedef int 
(*xfs_message_function) (int, struct xfs_message_header*, u_int);

extern xfs_message_function rcvfuncs[XFS_MSG_COUNT];

#endif /* __XFS_H_V */
