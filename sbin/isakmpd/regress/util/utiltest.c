/*	$OpenBSD: x509.c,v 1.58 2001/06/22 16:21:43 provos Exp $	*/

/*
 * Copyright (c) 2001 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Ericsson Radio Systems.
 * 4. The name of the author may not be used to endorse or promote products
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#include "sysdep.h"

#include "util.h"

int test_1 (char *, char *, int);

int
main (int argc, char *argv[])
{
  test_1 ("10.0.0.1", "10", 0);
  test_1 ("10.0.0.1", "isakmp", 0);
  test_1 ("10::1", "10", 0);
  test_1 ("10::1", "isakmp", 0);
  test_1 ("10.0x0.1", "10", -1);
  test_1 ("10.0.0.1", "telnet", -1);
  test_1 ("10::x:1", "10", -1);
  test_1 ("10::1", "telnet", -1);
  return 0;
}

int test_1 (char *address, char *port, int ok)
{
  struct sockaddr *sa;
  struct sockaddr_in *sai;
  struct sockaddr_in6 *sai6;
  int i, rv;

  printf ("test_1 (\"%s\", \"%s\") ", address, port);
  rv = text2sockaddr (address, port, &sa) == ok;
  printf (rv ? "OK" : "FAIL");
  printf ("\n");

#ifdef DEBUG
  printf ("af %d len %d ", sa->sa_family, sa->sa_len);
  if (sa->sa_family == AF_INET)
    {
      sai = (struct sockaddr_in *)sa;
      printf ("addr %08x port %d\n", ntohl (sai->sin_addr.s_addr),
	      ntohs (sai->sin_port));
    }
  else
    {
      sai6 = (struct sockaddr_in6 *)sa;
      printf ("addr ");
      for (i = 0; i < sizeof sai6->sin6_addr; i++)
	printf ("%02x", sai6->sin6_addr.s6_addr[i]);
      printf (" port %d\n", ntohs (sai6->sin6_port));
    }
  return rv;
#endif
}
