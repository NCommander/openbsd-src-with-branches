/* osdep.h

   Operating system dependencies... */

/*
 * Copyright (c) 1996, 1997, 1998, 1999 The Internet Software Consortium.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE INTERNET SOFTWARE CONSORTIUM OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software was written for the Internet Software Consortium by Ted Lemon
 * under a contract with Vixie Laboratories.
 */

#if !defined (USE_SOCKETS) && \
    !defined (USE_SOCKET_SEND) && \
    !defined (USE_SOCKET_RECEIVE) && \
    !defined (USE_RAW_SOCKETS) && \
    !defined (USE_RAW_SEND) && \
    !defined (USE_SOCKET_RECEIVE) && \
    !defined (USE_BPF) && \
    !defined (USE_BPF_SEND) && \
    !defined (USE_BPF_RECEIVE) && \
#  define USE_DEFAULT_NETWORK
#endif

#include <syslog.h>
#include <sys/types.h>
#include <string.h>
#include <paths.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <limits.h>

#include <sys/wait.h>
#include <signal.h>

extern int h_errno;

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sockio.h>

#define ifr_netmask ifr_addr

/* Varargs stuff... */
#include <stdarg.h>
#define VA_DOTDOTDOT ...
#define va_dcl
#define VA_start(list, last) va_start (list, last)

#define EOL	'\n'
#define VOIDPTR void *

/* Time stuff... */
#include <sys/time.h>
#define TIME time_t
#define GET_TIME(x)	time ((x))

#define HAVE_SA_LEN
#define HAVE_MKSTEMP

#define USE_BPF

#if defined(__alpha__) || (defined(__sparc64__) && defined(__arch64__))
#define PTRSIZE_64BIT
#endif

#if !defined (TIME_MAX)
# define TIME_MAX 2147483647
#endif

/* Porting::

   If you add a new network API, and have it set up so that it can be
   used for sending or receiving, but doesn't have to be used for both,
   then set up an ifdef like the ones below: */

#ifdef USE_SOCKETS
#  define USE_SOCKET_SEND
#  define USE_SOCKET_RECEIVE
#endif

#ifdef USE_RAW_SOCKETS
#  define USE_RAW_SEND
#  define USE_SOCKET_RECEIVE
#endif

#ifdef USE_BPF
#  define USE_BPF_SEND
#  define USE_BPF_RECEIVE
#endif

/* Porting::

   If you add support for sending packets directly out an interface,
   and your support does not do ARP or routing, you must use a fallback
   mechanism to deal with packets that need to be sent to routers.
   Currently, all low-level packet interfaces use BSD sockets as a
   fallback. */

#if defined (USE_BPF_SEND) || defined (USE_NIT_SEND) || \
    defined (USE_DLPI_SEND) || defined (USE_UPF_SEND) || defined (USE_LPF_SEND)
#  define USE_SOCKET_FALLBACK
#  define USE_FALLBACK
#endif

/* Porting::

   If you add support for sending packets directly out an interface
   and need to be able to assemble packets, add the USE_XXX_SEND
   definition for your interface to the list tested below. */

#if defined (USE_RAW_SEND) || defined (USE_BPF_SEND) || \
		defined (USE_NIT_SEND) || defined (USE_UPF_SEND) || \
		defined (USE_DLPI_SEND) || defined (USE_LPF_SEND)
#  define PACKET_ASSEMBLY
#endif

/* Porting::

   If you add support for receiving packets directly from an interface
   and need to be able to decode raw packets, add the USE_XXX_RECEIVE
   definition for your interface to the list tested below. */

#if defined (USE_RAW_RECEIVE) || defined (USE_BPF_SEND) || \
		defined (USE_NIT_RECEIVE) || defined (USE_UPF_RECEIVE) || \
		defined (USE_DLPI_RECEIVE) || \
    defined (USE_LPF_SEND) || \
    (defined (USE_SOCKET_SEND) && defined (SO_BINDTODEVICE))
#  define PACKET_DECODING
#endif

/* jmp_buf is assumed to be a struct unless otherwise defined in the
   system header. */
#ifndef jbp_decl
# define jbp_decl(x)	jmp_buf *x
#endif
#ifndef jref
# define jref(x)	(&(x))
#endif
#ifndef jdref
# define jdref(x)	(*(x))
#endif
#ifndef jrefproto
# define jrefproto	jmp_buf *
#endif

#ifndef BPF_FORMAT
# define BPF_FORMAT "/dev/bpf%d"
#endif

#if defined (IFF_POINTOPOINT) && !defined (HAVE_IFF_POINTOPOINT)
# define HAVE_IFF_POINTOPOINT
#endif

#if defined (AF_LINK) && !defined (HAVE_AF_LINK)
# define HAVE_AF_LINK
#endif

#if defined (ARPHRD_TUNNEL) && !defined (HAVE_ARPHRD_TUNNEL)
# define HAVE_ARPHRD_TUNNEL
#endif

#if defined (ARPHRD_LOOPBACK) && !defined (HAVE_ARPHRD_LOOPBACK)
# define HAVE_ARPHRD_LOOPBACK
#endif

#if defined (ARPHRD_ROSE) && !defined (HAVE_ARPHRD_ROSE)
# define HAVE_ARPHRD_ROSE
#endif

#if defined (ARPHRD_IEEE802) && !defined (HAVE_ARPHRD_IEEE802)
# define HAVE_ARPHRD_IEEE802
#endif

#if defined (ARPHRD_FDDI) && !defined (HAVE_ARPHRD_FDDI)
# define HAVE_ARPHRD_FDDI
#endif

#if defined (ARPHRD_AX25) && !defined (HAVE_ARPHRD_AX25)
# define HAVE_ARPHRD_AX25
#endif

#if defined (ARPHRD_NETROM) && !defined (HAVE_ARPHRD_NETROM)
# define HAVE_ARPHRD_NETROM
#endif

#if defined (ARPHRD_METRICOM) && !defined (HAVE_ARPHRD_METRICOM)
# define HAVE_ARPHRD_METRICOM
#endif

#if defined (SO_BINDTODEVICE) && !defined (HAVE_SO_BINDTODEVICE)
# define HAVE_SO_BINDTODEVICE
#endif

#if defined (SIOCGIFHWADDR) && !defined (HAVE_SIOCGIFHWADDR)
# define HAVE_SIOCGIFHWADDR
#endif

#if defined (AF_LINK) && !defined (HAVE_AF_LINK)
# define HAVE_AF_LINK
#endif
