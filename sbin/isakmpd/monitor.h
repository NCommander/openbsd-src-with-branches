/*	$OpenBSD: ui.h,v 1.4 1998/12/21 01:02:28 niklas Exp $	*/

/*
 * Copyright (c) 2003 H�kan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined (USE_PRIVSEP)
#include <stdio.h>

#define ISAKMPD_PRIVSEP_USER "_isakmpd"

enum monitor_reqtypes
{
  MONITOR_GET_FD,
  MONITOR_GET_SOCKET,
  MONITOR_SETSOCKOPT,
  MONITOR_BIND,
  MONITOR_MKFIFO,
  MONITOR_CLOSE,
  MONITOR_SHUTDOWN,
};

pid_t	monitor_init (void);
void	monitor_loop (int);

void	mm_send_fd (int, int);
int	mm_receive_fd (int);

struct stat;
FILE 	*monitor_fopen (const char *, const char *);
int	monitor_open (const char *, int, mode_t);
int	monitor_stat (const char *, struct stat *);
int	monitor_socket (int, int, int);
int	monitor_setsockopt (int, int, int, const void *, socklen_t);
int	monitor_bind (int, const struct sockaddr *, socklen_t);
int	monitor_mkfifo (const char *, mode_t);
int	monitor_close (int);
int	monitor_fclose (FILE *);
#else
#define monitor_fopen fopen
#define monitor_open open
#define monitor_stat stat
#define monitor_socket socket
#define monitor_setsockopt setsockopt
#define monitor_bind bind
#define monitor_mkfifo mkfifo
#define monitor_close close
#define monitor_fclose fclose
#endif
