/*	$Id: sa.h,v 1.37 1998/11/14 23:42:27 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
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

#ifndef _SA_H_
#define _SA_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include "isakmp.h"

/* Remove a SA if it has not been fully negotiated in this time.  */
#define SA_NEGOTIATION_MAX_TIME 120

struct crypto_xf;
struct doi;
struct event;
struct exchange;
struct keystate;
struct message;
struct payload;
struct sa;
struct transport;

/* A protection suite consists of a set of protocol descriptions like this.  */
struct proto {
  /* Link to the next protocol in the suite.  */
  TAILQ_ENTRY (proto) link;

  /* The SA we belong to.  */
  struct sa *sa;

  /* The protocol number as found in the proposal payload.  */
  u_int8_t no;
  
  /* The protocol this SA is for.  */
  u_int8_t proto;

  /* Security parameter index info.  Element 0 - responder, 1 - initiator.  */
  u_int8_t spi_sz[2];
  u_int8_t *spi[2];

  /*
   * The chosen transform, only valid while the incoming SA payload that held
   * it is available for duplicate testing.
   */
  struct payload *chosen;

  /* The chosen transform's ID.  */
  u_int8_t id;

  /* DOI-specific data.  */
  void *data;
};

struct sa {
  /* Link to SAs with the same hash value.  */
  LIST_ENTRY (sa) link;

  /*
   * When several SA's are being negotiated in one message we connect them
   * through this link.
   */
  TAILQ_ENTRY (sa) next;

  /* What transport does this SA denote (ISAKMP SA only).  */
  struct transport *transport;

  /* Both initiator and responder cookies.  */
  u_int8_t cookies[ISAKMP_HDR_COOKIES_LEN];

  /* The message ID signifying non-ISAKMP SAs.  */
  u_int8_t message_id[ISAKMP_HDR_MESSAGE_ID_LEN];

  /* The protection suite chosen.  */
  TAILQ_HEAD (proto_head, proto) protos;

  /* The exchange type we should use when rekeying.  */
  u_int8_t exch_type;

  /* Phase is 1 for ISAKMP SAs, and 2 for application ones.  */
  u_int8_t phase;

  /* Save last message from setup, which can be retransmitted for dups */
  struct message *last_sent_in_setup;
  
  /* Various flags, look below for descriptions.  */
  u_int32_t flags;

  /* The DOI that is to handle DOI-specific issues for this SA.  */
  struct doi *doi;

  /* Crypto info needed to encrypt/decrypt packets protected by this SA.  */
  struct crypto_xf *crypto;
  int key_length;
  struct keystate *keystate;

  /* DOI-specific opaque data.  */
  void *data;

  /* Lifetime data.  */
  u_int64_t seconds;
  u_int64_t kilobytes;

  /* The event that will occur when an SA has timed out.  */
  struct event *death;
};

/* This SA is alive.  */
#define SA_FLAG_READY	1

extern void proto_free (struct proto *proto);
extern int sa_add_transform (struct sa *, struct payload *, int,
			     struct proto **);
extern int sa_create (struct exchange *, struct transport *);
extern void sa_delete (struct sa *, int);
extern void sa_free (struct sa *);
extern void sa_free_aux (struct sa *);
extern void sa_init (void);
extern struct sa *sa_isakmp_lookup_by_peer (struct sockaddr *, size_t addr);
extern void sa_isakmp_upgrade (struct message *);
extern struct sa *sa_lookup (u_int8_t *, u_int8_t *);
extern struct sa *sa_lookup_by_header (u_int8_t *, int);
extern struct sa *sa_lookup_from_icookie (u_int8_t *);
extern void sa_rekey_p1 (struct sa *);
extern void sa_report (void);

#endif /* _SA_H_ */
