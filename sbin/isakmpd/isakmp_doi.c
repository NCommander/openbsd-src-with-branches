/*	$OpenBSD: isakmp_doi.c,v 1.15 2003/06/03 14:28:16 ho Exp $	*/
/*	$EOM: isakmp_doi.c,v 1.42 2000/09/12 16:29:41 ho Exp $	*/

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

/*
 * XXX This DOI is very fuzzily defined, and should perhaps be short-circuited
 * to the IPsec DOI instead.  At the moment I will have it as its own DOI,
 * as the ISAKMP architecture seems to imply it should be done like this.
 */

#include <sys/types.h>

#include "sysdep.h"

#include "doi.h"
#include "exchange.h"
#include "isakmp.h"
#include "isakmp_doi.h"
#include "ipsec.h"
#include "log.h"
#include "message.h"
#include "sa.h"
#include "util.h"

#ifdef USE_DEBUG
static int isakmp_debug_attribute (u_int16_t, u_int8_t *, u_int16_t, void *);
#endif
static void isakmp_finalize_exchange (struct message *);
static struct keystate *isakmp_get_keystate (struct message *);
static int isakmp_initiator (struct message *);
static int isakmp_responder (struct message *);
static void isakmp_setup_situation (u_int8_t *);
static size_t isakmp_situation_size (void);
static u_int8_t isakmp_spi_size (u_int8_t);
static int isakmp_validate_attribute (u_int16_t, u_int8_t *, u_int16_t,
				      void *);
static int isakmp_validate_exchange (u_int8_t);
static int isakmp_validate_id_information (u_int8_t, u_int8_t *, u_int8_t *,
					   size_t, struct exchange *);
static int isakmp_validate_key_information (u_int8_t *, size_t);
static int isakmp_validate_notification (u_int16_t);
static int isakmp_validate_proto (u_int8_t);
static int isakmp_validate_situation (u_int8_t *, size_t *);
static int isakmp_validate_transform_id (u_int8_t, u_int8_t);

static struct doi isakmp_doi = {
  { 0 }, ISAKMP_DOI_ISAKMP, 0, 0, 0,
#ifdef USE_DEBUG
  isakmp_debug_attribute,
#endif
  0,				/* delete_spi not needed.  */
  0,				/* exchange_script not needed.  */
  isakmp_finalize_exchange,
  0,				/* free_exchange_data not needed.  */
  0,				/* free_proto_data not needed.  */
  0,				/* free_sa_data not needed.  */
  isakmp_get_keystate,
  0,				/* get_spi not needed.  */
  0,				/* handle_leftover_payload not needed.  */
  0,				/* informational_post_hook not needed.  */
  0,				/* informational_pre_hook not needed.  */
  0,				/* XXX need maybe be filled-in.  */
  0,				/* proto_init not needed.  */
  isakmp_setup_situation,
  isakmp_situation_size,
  isakmp_spi_size,
  isakmp_validate_attribute,
  isakmp_validate_exchange,
  isakmp_validate_id_information,
  isakmp_validate_key_information,
  isakmp_validate_notification,
  isakmp_validate_proto,
  isakmp_validate_situation,
  isakmp_validate_transform_id,
  isakmp_initiator,
  isakmp_responder,
#ifdef USE_DEBUG
  ipsec_decode_ids
#endif
};

/* Requires doi_init to already have been called.  */
void
isakmp_doi_init (void)
{
  doi_register (&isakmp_doi);
}

#ifdef USE_DEBUG
int
isakmp_debug_attribute (u_int16_t type, u_int8_t *value, u_int16_t len,
			void *vmsg)
{
  /* XXX Not implemented yet.  */
  return 0;
}
#endif /* USE_DEBUG */

static void
isakmp_finalize_exchange (struct message *msg)
{
}

static struct keystate *
isakmp_get_keystate (struct message *msg)
{
  return 0;
}

static void
isakmp_setup_situation (u_int8_t *buf)
{
  /* Nothing to do.  */
}

static size_t
isakmp_situation_size (void)
{
  return 0;
}

static u_int8_t
isakmp_spi_size (u_int8_t proto)
{
  /* One way to specify ISAKMP SPIs is to say they're zero-sized.  */
  return 0;
}

static int
isakmp_validate_attribute (u_int16_t type, u_int8_t *value, u_int16_t len,
			   void *vmsg)
{
  /* XXX Not implemented yet.  */
  return -1;
}

static int
isakmp_validate_exchange (u_int8_t exch)
{
  /* If we get here the exchange is invalid.  */
  return -1;
}

static int
isakmp_validate_id_information (u_int8_t type, u_int8_t *extra, u_int8_t *buf,
				size_t sz, struct exchange *exchange)
{
  return zero_test (extra, ISAKMP_ID_DOI_DATA_LEN);
}

static int
isakmp_validate_key_information (u_int8_t *buf, size_t sz)
{
  /* Nothing to do.  */
  return 0;
}

static int
isakmp_validate_notification (u_int16_t type)
{
  /* If we get here the message type is invalid.  */
  return -1;
}

static int
isakmp_validate_proto (u_int8_t proto)
{
  /* If we get here the protocol is invalid.  */
  return -1;
}

static int
isakmp_validate_situation (u_int8_t *buf, size_t *sz)
{
  /* There are no situations in the ISAKMP DOI.  */
  *sz = 0;
  return 0;
}

static int
isakmp_validate_transform_id (u_int8_t proto, u_int8_t transform_id)
{
  /* XXX Not yet implemented.  */
  return -1;
}

static int
isakmp_initiator (struct message *msg)
{
  if (msg->exchange->type != ISAKMP_EXCH_INFO)
    {
      log_print ("isakmp_initiator: unsupported exchange type %d in phase %d",
		 msg->exchange->type, msg->exchange->phase);
      return -1;
    }

  return message_send_info (msg);
}

static int
isakmp_responder (struct message *msg)
{
  struct payload *p;
  char *tag;

  switch (msg->exchange->type)
    {
    case ISAKMP_EXCH_INFO:
      for (p = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_NOTIFY]); p;
	   p = TAILQ_NEXT (p, link))
	{
	  tag = constant_lookup (isakmp_notify_cst,
				 GET_ISAKMP_NOTIFY_MSG_TYPE (p->p));
	  LOG_DBG ((LOG_EXCHANGE, 10,
		    "isakmp_responder: got NOTIFY of type %s, ignoring",
		    tag ? tag : "<unknown>"));
	  p->flags |= PL_MARK;
	}

      for (p = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_DELETE]); p;
	   p = TAILQ_NEXT (p, link))
	{
	  LOG_DBG ((LOG_EXCHANGE, 10,
		    "isakmp_responder: got DELETE, ignoring"));
	  p->flags |= PL_MARK;
	}
      return 0;

#ifdef USE_ISAKMP_CFG
    case ISAKMP_EXCH_TRANSACTION:
      /* return 0 isakmp_cfg_responder (msg); */
#endif /* USE_ISAKMP_CFG */

    default:
      /* XXX So far we don't accept any proposals.  */
      if (TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_SA]))
	{
	  message_drop (msg, ISAKMP_NOTIFY_NO_PROPOSAL_CHOSEN, 0, 1, 0);
	  return -1;
	}
    }
  return 0;
}
