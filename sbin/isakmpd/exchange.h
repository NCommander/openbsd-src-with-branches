/*	$OpenBSD: exchange.h,v 1.18 2001/02/24 03:59:55 angelos Exp $	*/
/*	$EOM: exchange.h,v 1.28 2000/09/28 12:54:28 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999, 2001 Niklas Hallqvist.  All rights reserved.
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

#ifndef _EXCHANGE_H_
#define _EXCHANGE_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>

#include "exchange_num.h"
#include "isakmp.h"

/* Remove an exchange if it has not been fully negotiated in this time.  */
#define EXCHANGE_MAX_TIME 120

struct crypto_xf;
struct certreq_aca;
struct doi;
struct event;
struct keystate;
struct message;
struct payload;
struct transport;
struct sa;

struct exchange {
  /* Link to exchanges with the same hash value.  */
  LIST_ENTRY (exchange) link;

  /* A name of the SAs this exchange will result in.  XXX non unique?  */
  char *name;

  /* A name of the major policy deciding offers and acceptable proposals.  */
  char *policy;

  /*
   * A function with a polymorphic argument called after the exchange
   * has been run to its end, successfully.  The 2nd argument is true
   * if the finalization hook is called due to the exchange not running
   * to its end normally.
   */
  void (*finalize) (struct exchange *, void *, int);
  void *finalize_arg;

  /* When several SA's are being negotiated we keep them here.  */
  TAILQ_HEAD (sa_head, sa) sa_list;

  /*
   * The event that will occur when it has taken too long time to try to
   * run the exchange and which will trigger auto-destruction.
   */
  struct event *death;

  /*
   * Both initiator and responder cookies.
   * XXX For code clarity we might split this into two fields.
   */
  u_int8_t cookies[ISAKMP_HDR_COOKIES_LEN];

  /* The message ID signifying phase 2 exchanges.  */
  u_int8_t message_id[ISAKMP_HDR_MESSAGE_ID_LEN];

  /* The exchange type we are using.  */
  u_int8_t type;

  /* Phase is 1 for ISAKMP SA exchanges, and 2 for application ones.  */
  u_int8_t phase;

  /* The "step counter" of the exchange, starting from zero.  */
  u_int8_t step;

  /* 1 if we are the initiator, 0 if we are the responder.  */
  u_int8_t initiator;

  /* Various flags, look below for descriptions.  */
  u_int32_t flags;

  /* The DOI that is to handle DOI-specific issues for this exchange.  */
  struct doi *doi;

  /*
   * A "program counter" into the script that validate message contents for
   * this exchange.
   */
  int16_t *exch_pc;

  /* The last message received, used for checking for duplicates.  */
  struct message *last_received;

  /* The last message sent, to be acked when something new is received.  */
  struct message *last_sent;

  /*
   * If some message is queued up for sending, we want to be able to remove
   * it from the queue, when the exchange is deleted.
   */
  struct message *in_transit;

  /*
   * Initiator's & responder's nonces respectively, with lengths.
   * XXX Should this be in the DOI-specific parts instead?
   */
  u_int8_t *nonce_i;
  size_t nonce_i_len;
  u_int8_t *nonce_r;
  size_t nonce_r_len;

  /* The ID payload contents for the initiator & responder, respectively.  */
  u_int8_t *id_i;
  size_t id_i_len;
  u_int8_t *id_r;
  size_t id_r_len;

  /* Policy session identifier, where applicable */
  int policy_id;

  /* Crypto info needed to encrypt/decrypt packets in this exchange.  */
  struct crypto_xf *crypto;
  int key_length;
  struct keystate *keystate;

  /* 
   * Received certificate - used to verify signatures on packet,
   * stored here for later policy processing. 
   * a type of ISAKMP_CERTENC_NONE implies pre-shared key.
   */
  int recv_certtype, recv_certlen;
  void *recv_cert;
  void *recv_key;

  /* ACQUIRE sequence number */
  u_int32_t seq;

  /* XXX This is no longer necessary, it is covered by policy.  */

  /* Acceptable authorities for cert requests */
  TAILQ_HEAD (aca_head, certreq_aca) aca_list;

  /* DOI-specific opaque data.  */
  void *data;
};

/* The flag bits.  */
#define EXCHANGE_FLAG_I_COMMITTED	1
#define EXCHANGE_FLAG_HE_COMMITTED	2
#define EXCHANGE_FLAG_COMMITTED		(EXCHANGE_FLAG_I_COMMITTED \
					 | EXCHANGE_FLAG_HE_COMMITTED)
#define EXCHANGE_FLAG_ENCRYPT		4

extern int exchange_add_certs (struct message *);
extern void exchange_finalize (struct message *);
extern void exchange_free (struct exchange *);
extern void exchange_free_aca_list (struct exchange *);
extern void exchange_establish (char *name,
				void (*) (struct exchange *, void *, int),
				void *);
extern void exchange_establish_p1 (struct transport *, u_int8_t, u_int32_t,
				   char *, void *,
				   void (*) (struct exchange *, void *, int),
				   void *);
extern void exchange_establish_p2 (struct sa *, u_int8_t, char *, void *,
				   void (*) (struct exchange *, void *, int),
				   void *);
extern int exchange_gen_nonce (struct message *, size_t);
extern void exchange_init (void);
extern struct exchange *exchange_lookup (u_int8_t *, int);
extern struct exchange *exchange_lookup_by_name (char *, int);
extern struct exchange *exchange_lookup_from_icookie (u_int8_t *);
extern void exchange_report (void);
extern void exchange_run (struct message *);
extern int exchange_save_nonce (struct message *);
extern int exchange_save_certreq (struct message *);
extern u_int16_t *exchange_script (struct exchange *);
extern struct exchange *exchange_setup_p1 (struct message *, u_int32_t);
extern struct exchange *exchange_setup_p2 (struct message *, u_int8_t);
extern void exchange_upgrade_p1 (struct message *);

#endif /* _EXCHANGE_H_ */
