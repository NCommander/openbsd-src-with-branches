/*	$OpenBSD: exchange.c,v 1.45 2001/04/24 07:27:36 niklas Exp $	*/
/*	$EOM: exchange.c,v 1.143 2000/12/04 00:02:25 angelos Exp $	*/

/*
 * Copyright (c) 1998, 1999, 2000, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 1999, 2001 Angelos D. Keromytis.  All rights reserved.
 * Copyright (c) 1999, 2000 H�kan Olsson.  All rights reserved.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "cert.h"
#include "conf.h"
#include "connection.h"
#include "constants.h"
#include "cookie.h"
#include "crypto.h"
#include "doi.h"
#include "exchange.h"
#include "ipsec_num.h"
#include "isakmp.h"
#include "libcrypto.h"
#include "log.h"
#include "message.h"
#include "timer.h"
#include "transport.h"
#include "ipsec.h"
#include "sa.h"
#include "util.h"
#ifdef USE_X509
#include "x509.h"
#endif

/* Initial number of bits from the cookies used as hash.  */
#define INITIAL_BUCKET_BITS 6

/*
 * Don't try to use more bits than this as a hash.
 * We only XOR 16 bits so going above that means changing the code below
 * too.
 */
#define MAX_BUCKET_BITS 16

static void exchange_dump (char *, struct exchange *);
static void exchange_free_aux (void *);

static LIST_HEAD (exchange_list, exchange) *exchange_tab;

/* Works both as a maximum index and a mask.  */
static int bucket_mask;

/*
 * Validation scripts used to test messages for correct content of
 * payloads depending on the exchange type.
 */
int16_t script_base[] = {
  ISAKMP_PAYLOAD_SA,		/* Initiator -> responder.  */
  ISAKMP_PAYLOAD_NONCE,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_SA,		/* Responder -> initiator.  */
  ISAKMP_PAYLOAD_NONCE,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_KEY_EXCH,	/* Initiator -> responder.  */
  ISAKMP_PAYLOAD_ID,
  EXCHANGE_SCRIPT_AUTH,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_KEY_EXCH,	/* Responder -> initiator.  */
  ISAKMP_PAYLOAD_ID,
  EXCHANGE_SCRIPT_AUTH,
  EXCHANGE_SCRIPT_END
};

int16_t script_identity_protection[] = {
  ISAKMP_PAYLOAD_SA,		/* Initiator -> responder.  */
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_SA,		/* Responder -> initiator.  */
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_KEY_EXCH,	/* Initiator -> responder.  */
  ISAKMP_PAYLOAD_NONCE,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_KEY_EXCH,	/* Responder -> initiator.  */
  ISAKMP_PAYLOAD_NONCE,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_ID,		/* Initiator -> responder.  */
  EXCHANGE_SCRIPT_AUTH,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_ID,		/* Responder -> initiator.  */
  EXCHANGE_SCRIPT_AUTH,
  EXCHANGE_SCRIPT_END
};

int16_t script_authentication_only[] = {
  ISAKMP_PAYLOAD_SA,		/* Initiator -> responder.  */
  ISAKMP_PAYLOAD_NONCE,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_SA,		/* Responder -> initiator.  */
  ISAKMP_PAYLOAD_NONCE,
  ISAKMP_PAYLOAD_ID,
  EXCHANGE_SCRIPT_AUTH,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_ID,		/* Initiator -> responder.  */
  EXCHANGE_SCRIPT_AUTH,
  EXCHANGE_SCRIPT_END
};

#ifdef USE_AGGRESSIVE
int16_t script_aggressive[] = {
  ISAKMP_PAYLOAD_SA,		/* Initiator -> responder.  */
  ISAKMP_PAYLOAD_KEY_EXCH,
  ISAKMP_PAYLOAD_NONCE,
  ISAKMP_PAYLOAD_ID,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_SA,		/* Responder -> initiator.  */
  ISAKMP_PAYLOAD_KEY_EXCH,
  ISAKMP_PAYLOAD_NONCE,
  ISAKMP_PAYLOAD_ID,
  EXCHANGE_SCRIPT_AUTH,
  EXCHANGE_SCRIPT_SWITCH,
  EXCHANGE_SCRIPT_AUTH,		/* Initiator -> responder.  */
  EXCHANGE_SCRIPT_END
};
#endif /* USE_AGGRESSIVE */

int16_t script_informational[] = {
  EXCHANGE_SCRIPT_INFO,		/* Initiator -> responder.  */
  EXCHANGE_SCRIPT_END
};

/*
 * Check what exchange SA is negotiated with and return a suitable validation
 * script.
 */
u_int16_t *
exchange_script (struct exchange *exchange)
{
  switch (exchange->type)
    {
    case ISAKMP_EXCH_BASE:
      return script_base;
    case ISAKMP_EXCH_ID_PROT:
      return script_identity_protection;
    case ISAKMP_EXCH_AUTH_ONLY:
      return script_authentication_only;
#ifdef USE_AGGRESSIVE
    case ISAKMP_EXCH_AGGRESSIVE:
      return script_aggressive;
#endif
    case ISAKMP_EXCH_INFO:
      return script_informational;
    default:
      if (exchange->type >= ISAKMP_EXCH_DOI_MIN
	  && exchange->type <= ISAKMP_EXCH_DOI_MAX)
	return exchange->doi->exchange_script (exchange->type);
    }
  return 0;
}

/*
 * Validate the message MSG's contents wrt what payloads the exchange type
 * requires at this point in the dialogoue.  Return -1 if the validation fails,
 * 0 if it succeeds and the script is not finished and 1 if it's ready.
 */
static int
exchange_validate (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  int16_t *pc = exchange->exch_pc;

  while (*pc != EXCHANGE_SCRIPT_END && *pc != EXCHANGE_SCRIPT_SWITCH)
    {
      LOG_DBG ((LOG_EXCHANGE, 90, 
		"exchange_validate: checking for required %s",
		*pc >= ISAKMP_PAYLOAD_NONE
		? constant_name (isakmp_payload_cst, *pc)
		: constant_name (exchange_script_cst, *pc)));

      /* Check for existence of the required payloads.  */
      if ((*pc > 0 && !TAILQ_FIRST (&msg->payload[*pc]))
	  || (*pc == EXCHANGE_SCRIPT_AUTH
	      && !TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_HASH])
	      && !TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_SIG]))
	  || (*pc == EXCHANGE_SCRIPT_INFO
	      && !TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_NOTIFY])
	      && !TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_DELETE])))
	{
	  /* Missing payload.  */
	  LOG_DBG ((LOG_MESSAGE, 70,
		    "exchange_validate: msg %p requires missing %s", msg,
		    *pc >= ISAKMP_PAYLOAD_NONE
		    ? constant_name (isakmp_payload_cst, *pc)
		    : constant_name (exchange_script_cst, *pc)));
	  return -1;
	}
      pc++;
    }
  if (*pc == EXCHANGE_SCRIPT_END)
    /* Cleanup.  */
    return 1;

  return 0;
}

/*
 * Run the exchange script from a point given by the "program counter"
 * upto either the script's end or a transmittal of a message.  If we are
 * at the point of a reception of a message, that message should be handed
 * in here in the MSG argument.  Otherwise we are the initiator and should
 * expect MSG to be a half-cooked message without payloads.
 */
void
exchange_run (struct message *msg)
{
  int i, done = 0;
  struct exchange *exchange = msg->exchange;
  struct doi *doi = exchange->doi;
  int (*handler) (struct message *)
    = exchange->initiator ? doi->initiator : doi->responder;
  struct payload *payload;

  while (!done)
    {
      /*
       * It's our turn if we're either the initiator on an even step,
       * or the responder on an odd step of the dialogue.
       */
      if (exchange->initiator ^ (exchange->step % 2))
	{
	  done = 1;
	  if (exchange->step)
	    msg = message_alloc_reply (msg);
	  message_setup_header (msg, exchange->type, 0, exchange->message_id);
	  if (handler (msg))
	    {
	      /*
	       * This can happen when transient starvation of memory occurs.
	       * XXX The peer's retransmit ought to kick-start this exchange
	       * again.  If he's stopped retransmitting he's likely dropped
	       * the SA at his side so we need to do that too, i.e.
	       * implement automatic SA teardown after a certain amount
	       * of inactivity.
	       */
	      log_print ("exchange_run: doi->%s (%p) failed",
			 exchange->initiator ? "initiator" : "responder", msg);
	      message_free (msg);
	      return;
	    }

	  switch (exchange_validate (msg))
	    {
	    case 1:
	      /*
	       * The last message of a multi-message exchange should
	       * not be retransmitted other than "on-demand", i.e. if we
	       * see retransmits of the last message of the peer
	       * later.
	       */
	      msg->flags |= MSG_LAST;
	      if (exchange->step > 0)
		{
		  if (exchange->last_sent)
		    message_free (exchange->last_sent);
		  exchange->last_sent = msg;
		}

	      /*
	       * After we physically have sent our last message we need to
	       * do SA-specific finalization, like telling our application
	       * the SA is ready to be used, or issuing a CONNECTED notify
	       * if we set the COMMIT bit.
	       */
	      message_register_post_send (msg, exchange_finalize);

	      /* Fallthrough.  */

	    case 0:
	      /* XXX error handling.  */
	      message_send (msg);
	      break;

	    default:
	      log_print ("exchange_run: exchange_validate failed, DOI error");
	      exchange_free (exchange);
	      message_free (msg);
	      return;
	    }
	}      
      else
	{
	  done = exchange_validate (msg);
	  switch (done)
	    {
	    case 0:
	    case 1:
	      /* Feed the message to the DOI.  */
	      if (handler (msg))
		{
		  /*
		   * Trust the peer to retransmit.
		   * XXX We have to implement SA aging with automatic teardown.
		   */
		  message_free (msg);
		  return;
		}

	      /*
	       * Go over the yet unhandled payloads and feed them to DOI
	       * for handling.
	       */
	      for (i = ISAKMP_PAYLOAD_SA; i < ISAKMP_PAYLOAD_RESERVED_MIN; i++)
		if (i != ISAKMP_PAYLOAD_PROPOSAL
		    && i != ISAKMP_PAYLOAD_TRANSFORM)
		  for (payload = TAILQ_FIRST (&msg->payload[i]); payload;
		       payload = TAILQ_NEXT (payload, link))
		    if ((payload->flags & PL_MARK) == 0)
		      if (!doi->handle_leftover_payload
			  || doi->handle_leftover_payload (msg, i, payload))
			LOG_DBG ((LOG_EXCHANGE, 10, 
				  "exchange_run: unexpected payload %s",
				  constant_name (isakmp_payload_cst, i)));

	      /*
	       * We have advanced the state.  If we have been processing an
	       * incoming message, record that message as the one to do
	       * duplication tests against.
	       */
	      if (exchange->last_received)
		message_free (exchange->last_received);
	      exchange->last_received = msg;
	      if (exchange->flags & EXCHANGE_FLAG_ENCRYPT)
		crypto_update_iv (exchange->keystate);

	      if (done)
		{
		  exchange_finalize (msg);
		  return;
		}
	      break;

	    case -1:
	      log_print ("exchange_run: exchange_validate failed");
	      /* XXX Is this the best error notification type?  */
	      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 1, 1);
	      return;
	    }
	}

      LOG_DBG ((LOG_EXCHANGE, 40, 
		"exchange_run: exchange %p finished step %d, advancing...",
		exchange, exchange->step));
      exchange->step++;
      while (*exchange->exch_pc != EXCHANGE_SCRIPT_SWITCH
	     && *exchange->exch_pc != EXCHANGE_SCRIPT_END)
	exchange->exch_pc++;
      exchange->exch_pc++;
    }
}

void
exchange_init ()
{
  int i;

  bucket_mask = (1 << INITIAL_BUCKET_BITS) - 1;
  exchange_tab = malloc ((bucket_mask + 1) * sizeof (struct exchange_list));
  if (!exchange_tab)
    log_fatal ("exchange_init: out of memory");
  for (i = 0; i <= bucket_mask; i++)
    {
      LIST_INIT (&exchange_tab[i]);
    }
 
}

void
exchange_resize ()
{
  int new_mask = (bucket_mask + 1) * 2 - 1;
  int i;
  struct exchange_list *new_tab;

  new_tab
    = realloc (exchange_tab, (new_mask + 1) * sizeof (struct exchange_list));
  if (!new_tab)
    return;
  for (i = bucket_mask + 1; i <= new_mask; i++)
    {
      LIST_INIT (&new_tab[i]);
    }
  bucket_mask = new_mask;
  /* XXX Rehash existing entries.  */
}

/* Lookup a phase 1 exchange out of just the initiator cookie.  */
struct exchange *
exchange_lookup_from_icookie (u_int8_t *cookie)
{
  int i;
  struct exchange *exchange;

  for (i = 0; i <= bucket_mask; i++)
    for (exchange = LIST_FIRST (&exchange_tab[i]); exchange;
	 exchange = LIST_NEXT (exchange, link))
      if (memcmp (exchange->cookies, cookie, ISAKMP_HDR_ICOOKIE_LEN) == 0
	  && exchange->phase == 1)
	return exchange;
  return 0;
}

/* Lookup an exchange out of the name and phase.  */
struct exchange *
exchange_lookup_by_name (char *name, int phase)
{
  int i;
  struct exchange *exchange;

  /* If we search for nothing, we will find nothing.  */
  if (!name)
    return 0;

  for (i = 0; i <= bucket_mask; i++)
    for (exchange = LIST_FIRST (&exchange_tab[i]); exchange;
	 exchange = LIST_NEXT (exchange, link))
      {
	LOG_DBG ((LOG_EXCHANGE, 90,
		  "exchange_lookup_by_name: %s == %s && %d == %d?", name,
		  exchange->name ? exchange->name : "<unnamed>", phase,
		  exchange->phase));

	/* 
	 * Match by name, but don't select finished exchanges, i.e
	 * where MSG_LAST are set in last_sent msg.
	 */
	if (exchange->name && strcasecmp (exchange->name, name) == 0
	    && exchange->phase == phase
	    && (!exchange->last_sent
		|| (exchange->last_sent->flags & MSG_LAST) == 0))
	  return exchange;
      }
  return 0;
}

/* Lookup an exchange out of the name, phase and step > 1.  */
struct exchange *
exchange_lookup_active (char *name, int phase)
{
  int i;
  struct exchange *exchange;

  /* XXX Almost identical to exchange_lookup_by_name.  */

  if (!name)
    return 0;

  for (i = 0; i <= bucket_mask; i++)
    for (exchange = LIST_FIRST (&exchange_tab[i]); exchange;
	 exchange = LIST_NEXT (exchange, link))
      {
	LOG_DBG ((LOG_EXCHANGE, 90,
		  "exchange_lookup_active: %s == %s && %d == %d?",
		  name, exchange->name ? exchange->name : "<unnamed>", phase,
		  exchange->phase));
	if (exchange->name && strcasecmp (exchange->name, name) == 0
	    && exchange->phase == phase)
	  {
	    if (exchange->step > 1)
	      return exchange;
	    else
	      LOG_DBG ((LOG_EXCHANGE, 80, 
			"exchange_lookup_active: avoided early (pre-step 1) "
			"exchange %p", exchange));
	  }
      }
  return 0;
}

static void
exchange_enter (struct exchange *exchange)
{
  u_int16_t bucket = 0;
  int i;
  u_int8_t *cp;

  /* XXX We might resize if we are crossing a certain threshold */

  for (i = 0; i < ISAKMP_HDR_COOKIES_LEN; i += 2)
    {
      cp = exchange->cookies + i;
      /* Doing it this way avoids alignment problems.  */
      bucket ^= cp[0] | cp[1] << 8;
    }
  for (i = 0; i < ISAKMP_HDR_MESSAGE_ID_LEN; i += 2)
    {
      cp = exchange->message_id + i;
      /* Doing it this way avoids alignment problems.  */
      bucket ^= cp[0] | cp[1] << 8;
    }
  bucket &= bucket_mask;
  LIST_INSERT_HEAD (&exchange_tab[bucket], exchange, link);
}

/*
 * Lookup the exchange given by the header fields MSG.  PHASE2 is false when
 * looking for phase 1 exchanges and true otherwise.
 */
struct exchange *
exchange_lookup (u_int8_t *msg, int phase2)
{
  u_int16_t bucket = 0;
  int i;
  struct exchange *exchange;
  u_int8_t *cp;

  /*
   * We use the cookies to get bits to use as an index into exchange_tab, as at
   * least one (our cookie) is a good hash, xoring all the bits, 16 at a
   * time, and then masking, should do.  Doing it this way means we can
   * validate cookies very fast thus delimiting the effects of "Denial of
   * service"-attacks using packet flooding.
   */
  for (i = 0; i < ISAKMP_HDR_COOKIES_LEN; i += 2)
    {
      cp = msg + ISAKMP_HDR_COOKIES_OFF + i;
      /* Doing it this way avoids alignment problems.  */
      bucket ^= cp[0] | cp[1] << 8;
    }
  if (phase2)
    for (i = 0; i < ISAKMP_HDR_MESSAGE_ID_LEN; i += 2)
      {
	cp = msg + ISAKMP_HDR_MESSAGE_ID_OFF + i;
	/* Doing it this way avoids alignment problems.  */
	bucket ^= cp[0] | cp[1] << 8;
      }
  bucket &= bucket_mask;
  for (exchange = LIST_FIRST (&exchange_tab[bucket]);
       exchange && (memcmp (msg + ISAKMP_HDR_COOKIES_OFF, exchange->cookies,
			    ISAKMP_HDR_COOKIES_LEN) != 0
		    || (phase2 && memcmp (msg + ISAKMP_HDR_MESSAGE_ID_OFF,
					  exchange->message_id,
					  ISAKMP_HDR_MESSAGE_ID_LEN) != 0)
		    || (!phase2 && !zero_test (msg + ISAKMP_HDR_MESSAGE_ID_OFF,
					       ISAKMP_HDR_MESSAGE_ID_LEN)));
       exchange = LIST_NEXT (exchange, link))
    ;

  return exchange;
}

/*
 * Create a phase PHASE exchange where INITIATOR denotes our role.  DOI
 * is the domain of interpretation identifier and TYPE tells what exchange
 * type to use per either the DOI document or the ISAKMP spec proper.
 * NSA tells how many SAs we should pre-allocate, and should be zero
 * when we have the responder role.
 */
static struct exchange *
exchange_create (int phase, int initiator, int doi, int type)
{
  struct exchange *exchange;
  struct timeval expiration;
  int delta;

  /*
   * We want the exchange zeroed for exchange_free to be able to find
   * out what fields have been filled-in.
   */
  exchange = calloc (1, sizeof *exchange);
  if (!exchange)
    {
      log_error ("exchange_create: calloc (1, %d) failed", sizeof *exchange);
      return 0;
    }
  exchange->phase = phase;
  exchange->step = 0;
  exchange->initiator = initiator;
  memset (exchange->cookies, 0, ISAKMP_HDR_COOKIES_LEN);
  memset (exchange->message_id, 0, ISAKMP_HDR_MESSAGE_ID_LEN);
  exchange->doi = doi_lookup (doi);
  exchange->type = type;
  exchange->policy_id = -1;
  exchange->exch_pc = exchange_script (exchange);
  exchange->last_sent = exchange->last_received = 0;
  TAILQ_INIT (&exchange->sa_list);
  TAILQ_INIT (&exchange->aca_list);

  /* Allocate the DOI-specific structure and initialize it to zeroes.  */
  if (exchange->doi->exchange_size)
    {
      exchange->data = calloc (1, exchange->doi->exchange_size);
      if (!exchange->data)
	{
	  log_error ("exchange_create: calloc (1, %d) failed",
		     exchange->doi->exchange_size);
	  exchange_free (exchange);
	  return 0;
	}
    }

  gettimeofday (&expiration, 0);
  delta = conf_get_num ("General", "Exchange-max-time", EXCHANGE_MAX_TIME);
  expiration.tv_sec += delta;
  exchange->death = timer_add_event ("exchange_free_aux", exchange_free_aux,
				     exchange, &expiration);
  if (!exchange->death)
    {
      /* If we don't give up we might start leaking...  */
      exchange_free_aux (exchange);
      return 0;
    }

  return exchange;
}

struct exchange_finalization_node
{
  void (*first) (struct exchange *, void *, int);
  void *first_arg;
  void (*second) (struct exchange *, void *, int);
  void *second_arg;
};

/* Run the finalization functions of ARG.  */
static void
exchange_run_finalizations (struct exchange *exchange, void *arg, int fail)
{
  struct exchange_finalization_node *node = arg;

  node->first (exchange, node->first_arg, fail);
  node->second (exchange, node->second_arg, fail);
  free (node);
}

/*
 * Add a finalization function FINALIZE with argument ARG to the tail
 * of the finalization function list of EXCHANGE.
 */
static void
exchange_add_finalization (struct exchange *exchange,
			   void (*finalize) (struct exchange *, void *, int),
			   void *arg)
{
  struct exchange_finalization_node *node;

  if (!finalize)
    return;

  if (!exchange->finalize)
    {
      exchange->finalize = finalize;
      exchange->finalize_arg = arg;
      return;
    }

  node = malloc (sizeof *node);
  if (!node)
    {
      log_error ("exchange_add_finalization: malloc (%d) failed",
		 sizeof *node);
      free (arg);
      return;
    }
  node->first = exchange->finalize;
  node->first_arg = exchange->finalize_arg;
  node->second = finalize;
  node->second_arg = arg;
  exchange->finalize = exchange_run_finalizations;
  exchange->finalize_arg = node;
}

/* Establish a phase 1 exchange.  */
void
exchange_establish_p1 (struct transport *t, u_int8_t type, u_int32_t doi,
		       char *name, void *args,
		       void (*finalize) (struct exchange *, void *, int),
		       void *arg)
{
  struct exchange *exchange;
  struct message *msg;
  char *tag = 0;
  char *str;

  if (name)
    {
      /* If no exchange type given, fetch from the configuration.  */
      if (type == 0)
	{
	  /* XXX Similar code can be found in exchange_setup_p1.  Share?  */

	  /* Find out our phase 1 mode.  */
	  tag = conf_get_str (name, "Configuration");
	  if (!tag)
	    {
	      /* Use default setting */
	      tag = conf_get_str ("Phase 1", "Default");
	      if (!tag)
		{
		  log_print ("exchange_establish_p1: "
			     "no \"Default\" tag in [Phase 1] section");
		  return;
		}
#if 0
	      log_print ("exchange_establish_p1: "
			 "no configuration found for peer \"%s\"",
			 name);
#endif
	    }

	  /* Figure out the DOI.  XXX Factor out?  */
	  str = conf_get_str (tag, "DOI");
	  if (!str || strcasecmp (str, "IPSEC") == 0)
	    doi = IPSEC_DOI_IPSEC;
	  else if (strcasecmp (str, "ISAKMP") == 0)
	    doi = ISAKMP_DOI_ISAKMP;
	  else
	    {
	      log_print ("exchange_establish_p1: DOI \"%s\" unsupported", str);
	      return;
	    }

	  /* What exchange type do we want?  */
	  str = conf_get_str (tag, "EXCHANGE_TYPE");
	  if (!str)
	    {
	      log_print ("exchange_establish_p1: "
			 "no \"EXCHANGE_TYPE\" tag in [%s] section", tag);
	      return;
	    }
	  type = constant_value (isakmp_exch_cst, str);
	  if (!type)
	    {
	      log_print ("exchange_establish_p1: unknown exchange type %s",
			 str);
	      return;
	    }
	}
    }

  exchange = exchange_create (1, 1, doi, type);
  if (!exchange)
    {
      /* XXX Do something here?  */
      return;
    }

  if (name)
    {
      exchange->name = strdup (name);
      if (!exchange->name)
	{
	  log_error ("exchange_establish_p1: strdup (\"%s\") failed", name);
	  exchange_free (exchange);
	  return;
	}
    }

  exchange->policy = name ? conf_get_str (name, "Configuration") : 0;
  if ((exchange->policy == NULL) && name)
    exchange->policy = conf_get_str ("Phase 1", "Default");

  exchange->finalize = finalize;
  exchange->finalize_arg = arg;
  cookie_gen (t, exchange, exchange->cookies, ISAKMP_HDR_ICOOKIE_LEN);
  exchange_enter (exchange);
  exchange_dump ("exchange_establish_p1", exchange);

  msg = message_alloc (t, 0, ISAKMP_HDR_SZ);
  msg->exchange = exchange;

  /* Do not create SA for an information exchange.  */
  if (exchange->type != ISAKMP_EXCH_INFO)
    {
      /*
       * Don't install a transport into this SA as it will be an INADDR_ANY
       * address in the local end, which is not good at all.  Let the reply
       * packet install the transport instead.
       */
      sa_create (exchange, 0);
      msg->isakmp_sa = TAILQ_FIRST (&exchange->sa_list);
      if (!msg->isakmp_sa)
	{
	  /* XXX Do something more here?  */
	  exchange_free (exchange);
	  return;
	}
      sa_reference (msg->isakmp_sa);
    }

  msg->extra = args;

  exchange_run (msg);
}

/* Establish a phase 2 exchange.  XXX With just one SA for now.  */
void
exchange_establish_p2 (struct sa *isakmp_sa, u_int8_t type, char *name,
		       void *args,
		       void (*finalize) (struct exchange *, void *, int),
		       void *arg)
{
  struct exchange *exchange;
  struct message *msg;
  int i;
  char *tag, *str;
  u_int32_t doi = ISAKMP_DOI_ISAKMP;
  u_int32_t seq = 0;

  if (isakmp_sa)
    doi = isakmp_sa->doi->id;

  if (name)
    {
      /* Find out our phase 2 modes.  */
      tag = conf_get_str (name, "Configuration");
      if (!tag)
	{
	  log_print ("exchange_establish_p2: no configuration for peer \"%s\"",
		     name);
	  return;
	}

      seq = (u_int32_t) conf_get_num (name, "Acquire-ID", 0);

      /* Figure out the DOI.  */
      str = conf_get_str (tag, "DOI");
      if (!str || strcasecmp (str, "IPSEC") == 0)
	doi = IPSEC_DOI_IPSEC;
      else if (strcasecmp (str, "ISAKMP") == 0)
	doi = ISAKMP_DOI_ISAKMP;
      else
	{
	  log_print ("exchange_establish_p2: DOI \"%s\" unsupported", str);
	  return;
	}
      
      /* What exchange type do we want?  */
      if (!type)
	{
	  str = conf_get_str (tag, "EXCHANGE_TYPE");
	  if (!str)
	    {
	      log_print ("exchange_establish_p2: "
			 "no \"EXCHANGE_TYPE\" tag in [%s] section", tag);
	      return;
	    }
	  /* XXX IKE dependent.  */
	  type = constant_value (ike_exch_cst, str);
	  if (!type)
	    {
	      log_print ("exchange_establish_p2: unknown exchange type %s",
			 str);
	      return;
	    }
	}
    }

  exchange = exchange_create (2, 1, doi, type);
  if (!exchange)
    {
      /* XXX Do something here?  */
      return;
    }

  if (name)
    {
      exchange->name = strdup (name);
      if (!exchange->name)
	{
	  log_error ("exchange_establish_p2: strdup (\"%s\") failed", name);
	  exchange_free (exchange);
	  return;
	}
    }
  exchange->policy = name ? conf_get_str (name, "Configuration") : 0;
  exchange->finalize = finalize;
  exchange->finalize_arg = arg;
  exchange->seq = seq;
  memcpy (exchange->cookies, isakmp_sa->cookies, ISAKMP_HDR_COOKIES_LEN);
  getrandom (exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  exchange->flags |= EXCHANGE_FLAG_ENCRYPT;
  exchange_enter (exchange);
  exchange_dump ("exchange_establish_p2", exchange);

  /* 
   * Do not create SA's for informational exchanges. 
   * XXX How to handle new group mode? 
   */
  if (exchange->type != ISAKMP_EXCH_INFO)
    {
      /* XXX Number of SAs should come from the args structure.  */
      for (i = 0; i < 1; i++)
	if (sa_create (exchange, isakmp_sa->transport))
	  {
	    exchange_free (exchange);
	    return;
	  }
    }

  msg = message_alloc (isakmp_sa->transport, 0, ISAKMP_HDR_SZ);
  msg->isakmp_sa = isakmp_sa;
  sa_reference (isakmp_sa);
  
  msg->extra = args;

  /* This needs to be done late or else get_keystate won't work right.  */
  msg->exchange = exchange;

  exchange_run (msg);
}

/* Out of an incoming phase 1 message, setup an exchange.  */
struct exchange *
exchange_setup_p1 (struct message *msg, u_int32_t doi)
{
  struct transport *t = msg->transport;
  struct exchange *exchange;
  struct sockaddr *dst;
  int dst_len;
  char *name = 0, *policy = 0, *str;
  u_int32_t want_doi;
  u_int8_t type;

  /* XXX Similar code can be found in exchange_establish_p1.  Share?  */

  /*
   * Unless this is an informational exchange, look up our policy for this
   * peer.
   */
  type = GET_ISAKMP_HDR_EXCH_TYPE (msg->iov[0].iov_base);
  if (type != ISAKMP_EXCH_INFO)
    {
      /*
       * Find out our inbound phase 1 mode.
       * XXX Assumes IPv4.  It might make sense to search through several
       * policies too.
       */
      t->vtbl->get_dst (t, &dst, &dst_len);
      name = conf_get_str ("Phase 1",
			   inet_ntoa (((struct sockaddr_in *)dst)->sin_addr));
      if (name)
	{
	  /*
	   * If another phase 1 exchange is ongoing don't bother returning the
	   * call. However, we will need to continue responding if our phase 1
	   * exchange is still waiting for step 1 (i.e still half-open).
	   */
	  if (exchange_lookup_active (name, 1))
	    return 0;
	}
      else
	{
	  name = conf_get_str ("Phase 1", "Default");
	  if (!name)
	    {
	      log_print ("exchange_setup_p1: "
			 "no \"Default\" tag in [Phase 1] section");
	      return 0;
	    }
	}

      policy = conf_get_str (name, "Configuration");
      if (!policy)
	{
	  log_print ("exchange_setup_p1: no configuration for peer \"%s\"",
		     name);
	  return 0;
	}

      /* Figure out the DOI.  */
      str = conf_get_str (policy, "DOI");
      if (!str || strcasecmp (str, "IPSEC") == 0)
	want_doi = IPSEC_DOI_IPSEC;
      else if (strcasecmp (str, "ISAKMP") == 0)
	want_doi = ISAKMP_DOI_ISAKMP;
      else
	{
	  log_print ("exchange_setup_p1: DOI \"%s\" unsupported", str);
	  return 0;
	}
      if (want_doi != doi)
	{
	  /* XXX Should I tell what DOI I got?  */
	  log_print ("exchange_setup_p1: expected %s DOI", str);
	  return 0;
	}

      /* What exchange type do we want?  */
      str = conf_get_str (policy, "EXCHANGE_TYPE");
      if (!str)
	{
	  log_print ("exchange_setup_p1: "
		     "no \"EXCHANGE_TYPE\" tag in [%s] section", policy);
	  return 0;
	}
      type = constant_value (isakmp_exch_cst, str);
      if (!type)
	{
	  log_print ("exchange_setup_p1: unknown exchange type %s", str);
	  return 0;
	}
      if (type != GET_ISAKMP_HDR_EXCH_TYPE (msg->iov[0].iov_base))
	{
	  log_print ("exchange_setup_p1: expected exchange type %s got %s",
		     str,
		     constant_lookup (isakmp_exch_cst,
				      GET_ISAKMP_HDR_EXCH_TYPE (msg->iov[0]
								.iov_base)));
	  return 0;
	}
    }

  exchange = exchange_create (1, 0, doi, type);
  if (!exchange)
    return 0;

  exchange->name = name ? strdup (name) : 0;
  if (name && !exchange->name)
    {
      log_error ("exchange_setup_p1: strdup (\"%s\") failed", name);
      exchange_free (exchange);
      return 0;
    }
  exchange->policy = policy;
  cookie_gen (msg->transport, exchange,
	      exchange->cookies + ISAKMP_HDR_ICOOKIE_LEN,
	      ISAKMP_HDR_RCOOKIE_LEN);
  GET_ISAKMP_HDR_ICOOKIE (msg->iov[0].iov_base, exchange->cookies);
  exchange_enter (exchange);
  exchange_dump ("exchange_setup_p1", exchange);
  return exchange;
}

/* Out of an incoming phase 2 message, setup an exchange.  */
struct exchange *
exchange_setup_p2 (struct message *msg, u_int8_t doi)
{
  struct exchange *exchange;
  u_int8_t *buf = msg->iov[0].iov_base;

  exchange = exchange_create (2, 0, doi, GET_ISAKMP_HDR_EXCH_TYPE (buf));
  if (!exchange)
    return 0;
  GET_ISAKMP_HDR_ICOOKIE (buf, exchange->cookies);
  GET_ISAKMP_HDR_RCOOKIE (buf, exchange->cookies + ISAKMP_HDR_ICOOKIE_LEN);
  GET_ISAKMP_HDR_MESSAGE_ID (buf, exchange->message_id);
  exchange_enter (exchange);
  exchange_dump ("exchange_setup_p2", exchange);
  return exchange;
}

/* Dump interesting data about an exchange.  */
static void
exchange_dump_real (char *header, struct exchange *exchange, int class,
		    int level)
{
  char buf[LOG_SIZE];
  /* Don't risk overflowing the final log buffer.  */
  int bufsize_max = LOG_SIZE - strlen (header) - 32; 
  struct sa *sa;

  LOG_DBG ((class, level, 
	    "%s: %p %s %s policy %s phase %d doi %d exchange %d step %d",
	    header, exchange, exchange->name ? exchange->name : "<unnamed>",
	    exchange->policy ? exchange->policy : "<no policy>",
	    exchange->initiator ? "initiator" : "responder", exchange->phase,
	    exchange->doi->id, exchange->type, exchange->step));
  LOG_DBG ((class, level, 
	    "%s: icookie %08x%08x rcookie %08x%08x", header,
	    decode_32 (exchange->cookies), decode_32 (exchange->cookies + 4),
	    decode_32 (exchange->cookies + 8),
	    decode_32 (exchange->cookies + 12)));

  /* Include phase 2 SA list for this exchange */
  if (exchange->phase == 2)
    {
      sprintf (buf, "sa_list ");
      for (sa = TAILQ_FIRST (&exchange->sa_list); 
	   sa && strlen (buf) < bufsize_max; sa = TAILQ_NEXT (sa, next))
	sprintf (buf + strlen (buf), "%p ", sa);
      if (sa)
	strcat (buf, "...");
    }
  else
    buf[0] = '\0';

  LOG_DBG ((class, level, "%s: msgid %08x %s", header, 
	    decode_32 (exchange->message_id), buf));
}

static void
exchange_dump (char *header, struct exchange *exchange)
{
  exchange_dump_real (header, exchange, LOG_EXCHANGE, 10);
}

void
exchange_report (void)
{
  int i;
  struct exchange *exchange;

  for (i = 0; i <= bucket_mask; i++)
    for (exchange = LIST_FIRST (&exchange_tab[i]); exchange;
	 exchange = LIST_NEXT (exchange, link))
      exchange_dump_real ("exchange_report", exchange, LOG_REPORT, 0);
}

/*
 * Release all resources this exchange is using *except* for the "death"
 * event.  When removing an exchange from the expiration handler that event
 * will be dealt with therein instead.
 */
static void
exchange_free_aux (void *v_exch)
{
  struct exchange *exchange = v_exch;
  struct sa *sa, *next_sa;
  struct cert_handler *handler;

  LOG_DBG ((LOG_EXCHANGE, 80, "exchange_free_aux: freeing exchange %p", 
	    exchange));

  if (exchange->last_received)
    message_free (exchange->last_received);
  if (exchange->last_sent)
    message_free (exchange->last_sent);
  if (exchange->in_transit && exchange->in_transit != exchange->last_sent)
    message_free (exchange->in_transit);
  if (exchange->nonce_i)
    free (exchange->nonce_i);
  if (exchange->nonce_r)
    free (exchange->nonce_r);
  if (exchange->id_i)
    free (exchange->id_i);
  if (exchange->id_r)
    free (exchange->id_r);
  if (exchange->keystate)
    free (exchange->keystate);
  if (exchange->doi && exchange->doi->free_exchange_data)
    exchange->doi->free_exchange_data (exchange->data);
  if (exchange->data)
    free (exchange->data);
  if (exchange->name)
    free (exchange->name);
  if (exchange->recv_cert)
    {
      handler = cert_get (exchange->recv_certtype);
      if (handler)
	handler->cert_free (exchange->recv_cert);
      else if (exchange->recv_certtype == ISAKMP_CERTENC_NONE)
	free (exchange->recv_cert);
    }
  if (exchange->recv_key)
    free (exchange->recv_key);

#if defined(POLICY) || defined(KEYNOTE)
  if (exchange->policy_id != -1)
    LK (kn_close, (exchange->policy_id));
#endif

  exchange_free_aca_list (exchange);
  LIST_REMOVE (exchange, link);

  /* Tell potential finalize routine we never got there.  */
  if (exchange->finalize)
    exchange->finalize (exchange, exchange->finalize_arg, 1);

  /* Remove any SAs that has not been disassociated from us.  */
  for (sa = TAILQ_FIRST (&exchange->sa_list); sa; sa = next_sa)
    {
      next_sa = TAILQ_NEXT (sa, next);
      sa_free (sa);
    }

  free (exchange);
}

/* Release all resources this exchange is using.  */
void
exchange_free (struct exchange *exchange)
{
  if (exchange->death)
    timer_remove_event (exchange->death);
  exchange_free_aux (exchange);
}

/*
 * Upgrade the phase 1 exchange and its ISAKMP SA with the rcookie of our
 * peer (found in his recently sent message MSG).
 */
void
exchange_upgrade_p1 (struct message *msg)
{
  struct exchange *exchange = msg->exchange;

  LIST_REMOVE (exchange, link);
  GET_ISAKMP_HDR_RCOOKIE (msg->iov[0].iov_base,
			  exchange->cookies + ISAKMP_HDR_ICOOKIE_LEN);
  exchange_enter (exchange);
  sa_isakmp_upgrade (msg);
}

static int
exchange_check_old_sa (struct sa *sa, void *v_arg)
{
  struct sa *new_sa = v_arg;
  char res1[1024];

  if (sa == new_sa || !sa->name || !(sa->flags & SA_FLAG_READY) || 
      (sa->flags & SA_FLAG_REPLACED))
    return 0;

  if (sa->phase != new_sa->phase || new_sa->name == NULL ||
      strcasecmp (sa->name, new_sa->name))
    return 0;

  if (sa->initiator)
    strlcpy (res1, ipsec_decode_ids ("%s %s", sa->id_i, sa->id_i_len, sa->id_r,
				     sa->id_r_len, 0), sizeof res1);
  else
    strlcpy (res1, ipsec_decode_ids ("%s %s", sa->id_r, sa->id_r_len, sa->id_i,
				     sa->id_i_len, 0), sizeof res1);

  LOG_DBG ((LOG_EXCHANGE, 30,
	    "checking whether new SA replaces existing SA with IDs %s",
	    res1));

  if (new_sa->initiator)
    return strcasecmp (res1, ipsec_decode_ids ("%s %s", new_sa->id_i,
					       new_sa->id_i_len,
					       new_sa->id_r,
					       new_sa->id_r_len, 0)) == 0;
  else
    return strcasecmp (res1, ipsec_decode_ids ("%s %s", new_sa->id_r,
					       new_sa->id_r_len,
					       new_sa->id_i,
					       new_sa->id_i_len, 0)) == 0;
}

void
exchange_finalize (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct sa *sa, *old_sa;
  struct proto *proto;
  struct conf_list *attrs;
  struct conf_list_node *attr;
  int i;

  exchange_dump ("exchange_finalize", exchange);

  /*
   * Walk over all the SAs and noting them as ready.  If we set the
   * COMMIT bit, tell the peer each SA is connected.
   *
   * XXX The decision should really be based on if a SA was installed
   * successfully.
   */
  for (sa = TAILQ_FIRST (&exchange->sa_list); sa; sa = TAILQ_NEXT (sa, next))
    {
      /* Move over the name to the SA.  */
      sa->name = exchange->name ? strdup (exchange->name) : 0;

      if (exchange->flags & EXCHANGE_FLAG_I_COMMITTED)
	{
	  for (proto = TAILQ_FIRST (&sa->protos); proto;
	       proto = TAILQ_NEXT (proto, link))
	    for (i = 0; i < 2; i++)
	      message_send_notification (exchange->last_received,
					 msg->isakmp_sa,
					 ISAKMP_NOTIFY_STATUS_CONNECTED, proto,
					 i);
	}

      /* Locate any old SAs and mark them replaced (SA_FLAG_REPLACED).  */
      while ((old_sa = sa_find (exchange_check_old_sa, sa)) != 0)
	sa_mark_replaced (old_sa);

      /* Setup the SA flags.  */
      sa->flags |= SA_FLAG_READY;
      if (exchange->name)
	{
	  attrs = conf_get_list (exchange->name, "Flags");
	  if (attrs)
	    {
	      for (attr = TAILQ_FIRST (&attrs->fields); attr;
		   attr = TAILQ_NEXT (attr, link))
		sa->flags |= sa_flag (attr->field);
	      conf_free_list (attrs);
	    }
	  /* 'Connections' should stay alive.  */
	  if (connection_exist (exchange->name))
	    {
	      sa->flags |= SA_FLAG_STAYALIVE;

	      /* ISAKMP SA of this connection should also stay alive.  */
	      if (exchange->phase == 2 && msg->isakmp_sa)
		msg->isakmp_sa->flags |= SA_FLAG_STAYALIVE;
	    }
	}

      sa->exch_type = exchange->type;
    }

  /*
   * If this was an phase 1 SA negotiation, save the keystate in the ISAKMP SA
   * structure for future initialization of phase 2 exchanges' keystates.
   * Also save the Phase 1 ID and authentication information.
   */
  if (exchange->phase == 1 && msg->isakmp_sa)
    {
      msg->isakmp_sa->keystate = exchange->keystate;
      exchange->keystate = 0;

      msg->isakmp_sa->recv_certtype = exchange->recv_certtype;
      msg->isakmp_sa->recv_certlen = exchange->recv_certlen;
      msg->isakmp_sa->recv_key = exchange->recv_key;
      exchange->recv_key = NULL; /* Reset */
      msg->isakmp_sa->policy_id = exchange->policy_id;
      exchange->policy_id = -1; /* Reset */
      msg->isakmp_sa->id_i_len = exchange->id_i_len;
      msg->isakmp_sa->id_r_len = exchange->id_r_len;
      msg->isakmp_sa->initiator = exchange->initiator;

      switch (exchange->recv_certtype)
        {
        case ISAKMP_CERTENC_NONE:
	case ISAKMP_CERTENC_KEYNOTE: /* No need for special handling */
	    msg->isakmp_sa->recv_cert = malloc (exchange->recv_certlen);
	    if (!msg->isakmp_sa->recv_cert)
	      {
		log_error ("exchange_finalize: malloc (%d) failed",
			   exchange->recv_certlen);
		/* XXX How to cleanup?  */
		return;
	      }
	    memcpy (msg->isakmp_sa->recv_cert, exchange->recv_cert,
		    msg->isakmp_sa->recv_certlen);
	    break;

	case ISAKMP_CERTENC_X509_SIG:
#ifdef USE_X509
	    msg->isakmp_sa->recv_cert = LC (X509_dup,
					    ((X509 *) exchange->recv_cert));
	    if (!msg->isakmp_sa->recv_cert)
	      {
		log_print ("exchange_finalize: "
			   "failed copying X509 certificate to isakmp_sa");
		/* XXX How to cleanup?  */
		return;
	      }
	    break;
#endif

	    /* XXX Eventually handle these */
	case ISAKMP_CERTENC_PKCS:
	case ISAKMP_CERTENC_PGP:	
	case ISAKMP_CERTENC_DNS:
	case ISAKMP_CERTENC_X509_KE:
	case ISAKMP_CERTENC_KERBEROS:
	case ISAKMP_CERTENC_CRL:
	case ISAKMP_CERTENC_ARL:
	case ISAKMP_CERTENC_SPKI:
	case ISAKMP_CERTENC_X509_ATTR:
	}

      LOG_DBG ((LOG_EXCHANGE, 10,
		"exchange_finalize: phase 1 done: %s, %s",
		exchange->doi == NULL ? "<no doi>" :
		exchange->doi->decode_ids ("initiator id %s, responder id %s",
					   exchange->id_i, exchange->id_i_len,
					   exchange->id_r, exchange->id_r_len,
					   0),
		msg->isakmp_sa == NULL || msg->isakmp_sa->transport == NULL
		? "<no transport>"
		: msg->isakmp_sa->transport->vtbl->decode_ids (msg->isakmp_sa->transport)));
    }

  exchange->doi->finalize_exchange (msg);
  if (exchange->finalize)
    exchange->finalize (exchange, exchange->finalize_arg, 0);
  exchange->finalize = 0;

  /* copy the ID from phase 1 to exchange or phase 2 SA */
  if (msg->isakmp_sa) 
    {
      if (exchange->id_i && exchange->id_r) 
	{
	  ipsec_clone_id (&msg->isakmp_sa->id_i, &msg->isakmp_sa->id_i_len,
			  exchange->id_i, exchange->id_i_len);
	  ipsec_clone_id (&msg->isakmp_sa->id_r, &msg->isakmp_sa->id_r_len,
			  exchange->id_r, exchange->id_r_len);
	}
      else if (msg->isakmp_sa->id_i && msg->isakmp_sa->id_r)
	{
	  ipsec_clone_id (&exchange->id_i, &exchange->id_i_len,
			  msg->isakmp_sa->id_i, msg->isakmp_sa->id_i_len);
	  ipsec_clone_id (&exchange->id_r, &exchange->id_r_len,
			  msg->isakmp_sa->id_r, msg->isakmp_sa->id_r_len);
	}
    }

  /*
   * There is no reason to keep the SAs connected to us anymore, in fact
   * it can hurt us if we have short lifetimes on the SAs and we try
   * to call exchange_report, where the SA list will be walked and
   * references to freed SAs can occur.
   */
  while (TAILQ_FIRST (&exchange->sa_list))
    {
      struct sa *sa = TAILQ_FIRST (&exchange->sa_list);

      if (exchange->id_i && exchange->id_r)
	{
          ipsec_clone_id (&sa->id_i, &sa->id_i_len, exchange->id_i,
		          exchange->id_i_len);
          ipsec_clone_id (&sa->id_r, &sa->id_r_len, exchange->id_r,
		          exchange->id_r_len);
	}

      TAILQ_REMOVE (&exchange->sa_list, sa, next);
      sa_release (sa);
    }

  /* If we have nothing to retransmit we can safely remove ourselves.  */
  if (!exchange->last_sent)
    exchange_free (exchange);
}

/* Stash a nonce into the exchange data.  */
static int
exchange_nonce (struct exchange *exchange, int peer, size_t nonce_sz,
		u_int8_t *buf)
{
  int initiator = exchange->initiator ^ peer;
  u_int8_t **nonce;
  size_t *nonce_len;
  char header[32];

  nonce = initiator ? &exchange->nonce_i : &exchange->nonce_r;
  nonce_len = initiator ? &exchange->nonce_i_len : &exchange->nonce_r_len;
  *nonce_len = nonce_sz;
  *nonce = malloc (nonce_sz);
  if (!*nonce)
    {
      log_error ("exchange_nonce: malloc (%d) failed", nonce_sz);
      return -1;
    }
  memcpy (*nonce, buf, nonce_sz);
  snprintf (header, 32, "exchange_nonce: NONCE_%c", initiator ? 'i' : 'r');
  LOG_DBG_BUF ((LOG_EXCHANGE, 80, header, *nonce, nonce_sz));
  return 0;
}

/* Generate our NONCE.  */
int
exchange_gen_nonce (struct message *msg, size_t nonce_sz)
{
  struct exchange *exchange = msg->exchange;
  u_int8_t *buf;

  buf = malloc (ISAKMP_NONCE_SZ + nonce_sz);
  if (!buf)
    {
      log_error ("exchange_gen_nonce: malloc (%d) failed",
		 ISAKMP_NONCE_SZ + nonce_sz);
      return -1;
    }
  getrandom (buf + ISAKMP_NONCE_DATA_OFF, nonce_sz);
  if (message_add_payload (msg, ISAKMP_PAYLOAD_NONCE, buf,
			   ISAKMP_NONCE_SZ + nonce_sz, 1))
    {
      free (buf);
      return -1;
    }
  return exchange_nonce (exchange, 0, nonce_sz, buf + ISAKMP_NONCE_DATA_OFF);
}

/* Save the peer's NONCE.  */
int
exchange_save_nonce (struct message *msg)
{
  struct payload *noncep;
  struct exchange *exchange = msg->exchange;

  noncep = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_NONCE]);
  noncep->flags |= PL_MARK;
  return exchange_nonce (exchange, 1,
			 GET_ISAKMP_GEN_LENGTH (noncep->p)
			 - ISAKMP_NONCE_DATA_OFF,
			 noncep->p + ISAKMP_NONCE_DATA_OFF);
}

/* Save the peer's CERT REQuests.  */
int
exchange_save_certreq (struct message *msg)
{
  struct payload *cp = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_CERT_REQ]);
  struct exchange *exchange = msg->exchange;
  struct certreq_aca *tmp;

  for ( ; cp; cp = TAILQ_NEXT (cp, link))
    {
      cp->flags |= PL_MARK;
      tmp = certreq_decode (GET_ISAKMP_CERTREQ_TYPE (cp->p),
			    cp->p + ISAKMP_CERTREQ_AUTHORITY_OFF,
			    GET_ISAKMP_GEN_LENGTH (cp->p) - 
			    ISAKMP_CERTREQ_AUTHORITY_OFF);
      if (!tmp)
	continue;
      TAILQ_INSERT_TAIL (&exchange->aca_list, tmp, link);
    }

  return 0;
}

/* Free the list of pending CERTREQ */

void
exchange_free_aca_list (struct exchange *exchange)
{
  struct certreq_aca *aca;

  for (aca = TAILQ_FIRST (&exchange->aca_list); aca;
       aca = TAILQ_FIRST (&exchange->aca_list))
    {
      if (aca->data)
	{
	  if (aca->handler)
	    aca->handler->free_aca (aca->data);
	  free (aca->data);
	}
      TAILQ_REMOVE (&exchange->aca_list, aca, link);
      free (aca);
    }
}

/* Obtain certificates from acceptable certification authority.  */
int
exchange_add_certs (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct certreq_aca *aca;
  u_int8_t *cert;
  u_int32_t certlen;
  u_int8_t *id;
  size_t id_len;

  id = exchange->initiator ? exchange->id_r : exchange->id_i;
  id_len = exchange->initiator ? exchange->id_r_len : exchange->id_i_len;

  for (aca = TAILQ_FIRST (&exchange->aca_list); aca; 
       aca = TAILQ_NEXT (aca, link))
    {
      /* XXX? If we can not satisfy a CERTREQ we drop the message */
      if (!aca->handler->cert_obtain (id, id_len, aca->data, &cert, &certlen))
	{
	  log_print ("exchange_add_certs: could not obtain cert for a type %d "
		     "cert request", aca->id);
	  return -1;
	}
      cert = realloc (cert, ISAKMP_CERT_SZ + certlen);
      if (!cert)
	{
	  log_error ("exchange_add_certs: realloc (%p, %d) failed", cert,
		     ISAKMP_CERT_SZ + certlen);
	  return -1;
	}
      memmove (cert + ISAKMP_CERT_DATA_OFF, cert, certlen);
      SET_ISAKMP_CERT_ENCODING (cert, aca->id);
      if (message_add_payload (msg, ISAKMP_PAYLOAD_CERT, cert,
			       ISAKMP_CERT_SZ + certlen, 1))
	{
	  free (cert);
	  return -1;
	}
    }

  /* We dont need the CERT REQs any more, they are anwsered */
  exchange_free_aca_list (exchange);

  return 0;
}

static void
exchange_establish_finalize (struct exchange *exchange, void *arg, int fail)
{
  char *name = arg;

  LOG_DBG ((LOG_EXCHANGE, 20,
	    "exchange_establish_finalize: "
	    "finalizing exchange %p with arg %p (%s) & fail = %d",
	    exchange, arg, name ? name : "<unnamed>", fail));

  if (!fail)
    exchange_establish (name, 0, 0);
  free (name);
}

/*
 * Establish an exchange named NAME, and record the FINALIZE function
 * taking ARG as an argument to be run after the exchange is ready.
 */
void
exchange_establish (char *name,
		    void (*finalize) (struct exchange *, void *, int),
		    void *arg)
{
  int phase;
  char *trpt;
  struct transport *transport;
  char *peer;
  struct sa *isakmp_sa;
  struct exchange *exchange;
  phase = conf_get_num (name, "Phase", 0);

  /*
   * First of all, never try to establish anything if another exchange of the
   * same kind is running.
   */
  exchange = exchange_lookup_by_name (name, phase);
  if (exchange)
    {
      LOG_DBG ((LOG_EXCHANGE, 40,
		"exchange_establish: %s exchange already exists as %p", name,
		exchange));
      exchange_add_finalization (exchange, finalize, arg);
      return;
    }

  switch (phase)
    {
    case 1:
      trpt = conf_get_str (name, "Transport");
      if (!trpt)
	{
	  /* Phase 1 transport defaults to "udp".  */
	  trpt = ISAKMP_DEFAULT_TRANSPORT;
	}

      transport = transport_create (trpt, name);
      if (!transport)
	{
	  log_print ("exchange_establish: "
		     "transport \"%s\" for peer \"%s\" could not be created",
		     trpt, name);
	  return;
	}

      exchange_establish_p1 (transport, 0, 0, name, 0, finalize, arg);
      break;

    case 2:
      peer = conf_get_str (name, "ISAKMP-peer");
      if (!peer)
	{
	  log_print ("exchange_establish: No ISAKMP-peer given for \"%s\"",
		     name);
	  return;
	}

      isakmp_sa = sa_lookup_by_name (peer, 1);
      if (!isakmp_sa)
	{
	  name = strdup (name);
	  if (!name)
	    {
	      log_error ("exchange_establish: strdup(\"%s\") failed", name);
	      return;
	    }

	  if (conf_get_num (peer, "Phase", 0) != 1)
	    {
	      log_print ("exchange_establish: "
			 "[%s]:ISAKMP-peer's (%s) phase is not 1", name, peer);
	      return;
	    }

	  exchange_establish (peer, exchange_establish_finalize, name);
	}
      else
	exchange_establish_p2 (isakmp_sa, 0, name, 0, finalize, arg);
      break;

    default:
      log_print ("exchange_establish: "
		 "peer \"%s\" does not have a correct phase (%d)",
		 name, phase);
      break;
    }
}
