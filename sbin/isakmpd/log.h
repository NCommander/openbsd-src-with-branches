/*	$OpenBSD: log.h,v 1.10 2001/04/09 21:21:57 ho Exp $	*/
/*	$EOM: log.h,v 1.19 2000/03/30 14:27:23 ho Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _LOG_H_
#define _LOG_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <stdio.h>

/*
 * We cannot do the log strings dynamically sizeable as out of memory is one
 * of the situations we need to report about.
 */
#define LOG_SIZE	200

enum log_classes {
  LOG_MISC, LOG_TRANSPORT, LOG_MESSAGE, LOG_CRYPTO, LOG_TIMER, LOG_SYSDEP,
  LOG_SA, LOG_EXCHANGE, LOG_NEGOTIATION, LOG_POLICY, LOG_ENDCLASS
};
#define LOG_CLASSES_TEXT \
  { "Misc", "Trpt", "Mesg", "Cryp", "Timr", "Sdep", "SA  ", "Exch", "Negt", \
    "Plcy" }

/*
 * "Class" LOG_REPORT will always be logged to the current log channel,
 * regardless of level.
 */
#define LOG_PRINT  -1
#define LOG_REPORT -2

#ifdef USE_DEBUG

#define LOG_DBG(x)	log_debug x
#define LOG_DBG_BUF(x)	log_debug_buf x

extern void log_debug (int, int, const char *, ...);
extern void log_debug_buf (int, int, const char *, const u_int8_t *, size_t);
extern void log_debug_cmd (int, int);

#define PCAP_FILE_DEFAULT "/var/run/isakmpd.pcap"
extern void log_packet_init (char *);
extern void log_packet_iov (struct sockaddr *, struct sockaddr *, 
			    struct iovec *, int);
extern void log_packet_restart (char *);
extern void log_packet_stop (void);

#else /* !USE_DEBUG */

#define LOG_DBG(x)
#define LOG_DBG_BUF(x)

#endif /* USE_DEBUG */

extern FILE *log_current (void);
extern void log_error (const char *, ...);
extern void log_fatal (const char *, ...);
extern void log_print (const char *, ...);
extern void log_to (FILE *);
extern void log_init (void);

#endif /* _LOG_H_ */
