/*	$OpenBSD: policy.c,v 1.28 2001/04/09 12:34:38 ho Exp $	*/
/*	$EOM: policy.c,v 1.49 2000/10/24 13:33:39 niklas Exp $ */

/*
 * Copyright (c) 1999, 2000, 2001 Angelos D. Keromytis.  All rights reserved.
 * Copyright (c) 1999, 2000, 2001 Niklas Hallqvist.  All rights reserved.
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
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <regex.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <keynote.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <openssl/ssl.h>

#include "sysdep.h"

#include "app.h"
#include "conf.h"
#include "connection.h"
#include "cookie.h"
#include "doi.h"
#include "dyn.h"
#include "exchange.h"
#include "init.h"
#include "ipsec.h"
#include "isakmp_doi.h"
#include "math_group.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "udp.h"
#include "log.h"
#include "message.h"
#include "ui.h"
#include "util.h"
#include "policy.h"
#include "x509.h"

#if defined (HAVE_DLOPEN) && !defined (USE_KEYNOTE) && 0

void *libkeynote = 0;

/*
 * These prototypes matches OpenBSD keynote.h 1.6.  If you use
 * a different version than that, you are on your own.
 */
int *lk_keynote_errno;
int (*lk_kn_add_action) (int, char *, char *, int);
int (*lk_kn_add_assertion) (int, char *, int, int);
int (*lk_kn_add_authorizer) (int, char *);
int (*lk_kn_close) (int);
int (*lk_kn_do_query) (int, char **, int);
char *(*lk_kn_encode_key) (struct keynote_deckey *, int, int, int);
int (*lk_kn_init) (void);
char **(*lk_kn_read_asserts) (char *, int, int *);
int (*lk_kn_remove_authorizer) (int, char *);
int (*lk_kn_get_authorizer) (int, int, int *);
void (*lk_kn_free_key) (struct keynote_deckey *);
struct keynote_keylist *(*lk_kn_get_licensees) (int, int);
#define SYMENTRY(x) { SYM, SYM (x), (void **)&lk_ ## x }

static struct dynload_script libkeynote_script[] = {
  { LOAD, "libc.so", &libkeynote },
  { LOAD, "libcrypto.so", &libkeynote },
  { LOAD, "libm.so", &libkeynote },
  { LOAD, "libkeynote.so", &libkeynote },
  SYMENTRY (keynote_errno),
  SYMENTRY (kn_add_action),
  SYMENTRY (kn_add_assertion),
  SYMENTRY (kn_add_authorizer),
  SYMENTRY (kn_close),
  SYMENTRY (kn_do_query),
  SYMENTRY (kn_encode_key),
  SYMENTRY (kn_init),
  SYMENTRY (kn_read_asserts),
  SYMENTRY (kn_remove_authorizer),
  SYMENTRY (kn_get_licensees),
  SYMENTRY (kn_get_authorizer),
  { EOS }
};
#endif

int keynote_sessid = -1;
char **keynote_policy_asserts = NULL;
int keynote_policy_asserts_num = 0;
char **x509_policy_asserts = NULL;
int x509_policy_asserts_num = 0;
int x509_policy_asserts_num_alloc = 0;
struct exchange *policy_exchange = 0;
struct sa *policy_sa = 0;
struct sa *policy_isakmp_sa = 0;

static const char hextab[] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};

/*
 * Adaptation of Vixie's inet_ntop4 ()
 */
static const char *
my_inet_ntop4 (const in_addr_t *src, char *dst, size_t size, int normalize)
{
  static const char fmt[] = "%03u.%03u.%03u.%03u";
  char tmp[sizeof "255.255.255.255"];
  in_addr_t src2;

  if (normalize)
    src2 = ntohl (*src);
  else
    src2 = *src;

  if (sprintf (tmp, fmt, ((u_int8_t *) &src2)[0], ((u_int8_t *) &src2)[1],
	       ((u_int8_t *) &src2)[2], ((u_int8_t *) &src2)[3]) > size)
    {
      errno = ENOSPC;
      return 0;
    }
  strcpy (dst, tmp);
  return dst;
}

char *
policy_callback (char *name)
{
  struct proto *proto;

  u_int8_t *attr, *value, *id, *idlocal, *idremote;
  size_t id_sz, idlocalsz, idremotesz;
  struct sockaddr_in *sin;
  struct ipsec_exch *ie;
  struct ipsec_sa *is;
  int fmt, i, lifetype = 0;
  in_addr_t net, subnet;
  u_int16_t len, type;
  time_t tt;
  static char mytimeofday[15];

  /* We use all these as a cache.  */
  static char *esp_present, *ah_present, *comp_present;
  static char *ah_hash_alg, *ah_auth_alg, *esp_auth_alg, *esp_enc_alg;
  static char *comp_alg, ah_life_kbytes[32], ah_life_seconds[32];
  static char esp_life_kbytes[32], esp_life_seconds[32], comp_life_kbytes[32];
  static char comp_life_seconds[32], *ah_encapsulation, *esp_encapsulation;
  static char *comp_encapsulation, ah_key_length[32], esp_key_length[32];
  static char ah_key_rounds[32], esp_key_rounds[32], comp_dict_size[32];
  static char comp_private_alg[32], *remote_filter_type, *local_filter_type;
  static char remote_filter_addr_upper[64], remote_filter_addr_lower[64];
  static char local_filter_addr_upper[64], local_filter_addr_lower[64];
  static char ah_group_desc[32], esp_group_desc[32], comp_group_desc[32];
  static char remote_ike_address[64], local_ike_address[64];
  static char *remote_id_type, remote_id_addr_upper[64], *phase_1;
  static char remote_id_addr_lower[64], *remote_id_proto, remote_id_port[32];
  static char remote_filter_port[32], local_filter_port[32];
  static char *remote_filter_proto, *local_filter_proto, *pfs, *initiator;
  static char remote_filter_proto_num[3], local_filter_proto_num[3];
  static char remote_id_proto_num[3];
  static char phase1_group[32];

  /* Allocated.  */
  static char *remote_filter = 0, *local_filter = 0, *remote_id = 0;

  static int dirty = 1;

  /* We only need to set dirty at initialization time really.  */
  if (strcmp (name, KEYNOTE_CALLBACK_CLEANUP) == 0
      || strcmp (name, KEYNOTE_CALLBACK_INITIALIZE) == 0)
    {
      esp_present = ah_present = comp_present = pfs = "no";
      ah_hash_alg = ah_auth_alg = phase_1 = "";
      esp_auth_alg = esp_enc_alg = comp_alg = ah_encapsulation = "";
      esp_encapsulation = comp_encapsulation = remote_filter_type = "";
      local_filter_type = remote_id_type = initiator = "";
      remote_filter_proto = local_filter_proto = remote_id_proto = "";

      if (remote_filter != 0)
        {
	  free (remote_filter);
	  remote_filter = 0;
	}

      if (local_filter != 0)
        {
	  free (local_filter);
	  local_filter = 0;
	}

      if (remote_id != 0)
        {
	  free (remote_id);
	  remote_id = 0;
	}

      memset (remote_ike_address, 0, sizeof remote_ike_address);
      memset (local_ike_address, 0, sizeof local_ike_address);
      memset (ah_life_kbytes, 0, sizeof ah_life_kbytes);
      memset (ah_life_seconds, 0, sizeof ah_life_seconds);
      memset (esp_life_kbytes, 0, sizeof esp_life_kbytes);
      memset (esp_life_seconds, 0, sizeof esp_life_seconds);
      memset (comp_life_kbytes, 0, sizeof comp_life_kbytes);
      memset (comp_life_seconds, 0, sizeof comp_life_seconds);
      memset (ah_key_length, 0, sizeof ah_key_length);
      memset (ah_key_rounds, 0, sizeof ah_key_rounds);
      memset (esp_key_length, 0, sizeof esp_key_length);
      memset (esp_key_rounds, 0, sizeof esp_key_rounds);
      memset (comp_dict_size, 0, sizeof comp_dict_size);
      memset (comp_private_alg, 0, sizeof comp_private_alg);
      memset (remote_filter_addr_upper, 0, sizeof remote_filter_addr_upper);
      memset (remote_filter_addr_lower, 0, sizeof remote_filter_addr_lower);
      memset (local_filter_addr_upper, 0, sizeof local_filter_addr_upper);
      memset (local_filter_addr_lower, 0, sizeof local_filter_addr_lower);
      memset (remote_id_addr_upper, 0, sizeof remote_id_addr_upper);
      memset (remote_id_addr_lower, 0, sizeof remote_id_addr_lower);
      memset (ah_group_desc, 0, sizeof ah_group_desc);
      memset (esp_group_desc, 0, sizeof esp_group_desc);
      memset (remote_id_port, 0, sizeof remote_id_port);
      memset (remote_filter_port, 0, sizeof remote_filter_port);
      memset (local_filter_port, 0, sizeof local_filter_port);
      memset (phase1_group, 0, sizeof phase1_group);

      dirty = 1;
      return "";
    }

  /*
   * If dirty is set, this is the first request for an attribute, so
   * populate our value cache.
   */
  if (dirty)
    {
      ie = policy_exchange->data;

      if (ie->pfs)
	pfs = "yes";

      is = policy_isakmp_sa->data;
      sprintf (phase1_group, "%u", is->group_desc);

      for (proto = TAILQ_FIRST (&policy_sa->protos); proto;
	   proto = TAILQ_NEXT (proto, link))
	{
	  switch (proto->proto)
	    {
	    case IPSEC_PROTO_IPSEC_AH:
	      ah_present = "yes";
	      switch (proto->id)
		{
		case IPSEC_AH_MD5:
		  ah_hash_alg = "md5";
		  break;

		case IPSEC_AH_SHA:
		  ah_hash_alg = "sha";
		  break;

		case IPSEC_AH_RIPEMD:
		  ah_hash_alg = "ripemd";
		  break;

		case IPSEC_AH_DES:
		  ah_hash_alg = "des";
		  break;
		}

	      break;

	    case IPSEC_PROTO_IPSEC_ESP:
	      esp_present = "yes";
	      switch (proto->id)
		{
		case IPSEC_ESP_DES_IV64:
		  esp_enc_alg = "des-iv64";
		  break;

		case IPSEC_ESP_DES:
		  esp_enc_alg = "des";
		  break;

		case IPSEC_ESP_3DES:
		  esp_enc_alg = "3des";
		  break;

		case IPSEC_ESP_AES:
		  esp_enc_alg = "aes";
		  break;

		case IPSEC_ESP_RC5:
		  esp_enc_alg = "rc5";
		  break;

		case IPSEC_ESP_IDEA:
		  esp_enc_alg = "idea";
		  break;

		case IPSEC_ESP_CAST:
		  esp_enc_alg = "cast";
		  break;

		case IPSEC_ESP_BLOWFISH:
		  esp_enc_alg = "blowfish";
		  break;

		case IPSEC_ESP_3IDEA:
		  esp_enc_alg = "3idea";
		  break;

		case IPSEC_ESP_DES_IV32:
		  esp_enc_alg = "des-iv32";
		  break;

		case IPSEC_ESP_RC4:
		  esp_enc_alg = "rc4";
		  break;

		case IPSEC_ESP_NULL:
		  esp_enc_alg = "null";
		  break;
		}

	      break;

	    case IPSEC_PROTO_IPCOMP:
	      comp_present = "yes";
	      switch (proto->id)
		{
		case IPSEC_IPCOMP_OUI:
		  comp_alg = "oui";
		  break;

		case IPSEC_IPCOMP_DEFLATE:
		  comp_alg = "deflate";
		  break;

		case IPSEC_IPCOMP_LZS:
		  comp_alg = "lzs";
		  break;

		case IPSEC_IPCOMP_V42BIS:
		  comp_alg = "v42bis";
		  break;
		}

	      break;
	    }

	  for (attr = proto->chosen->p + ISAKMP_TRANSFORM_SA_ATTRS_OFF;
	       attr
		 < proto->chosen->p + GET_ISAKMP_GEN_LENGTH (proto->chosen->p);
	       attr = value + len)
	    {
	      if (attr + ISAKMP_ATTR_VALUE_OFF
		  > (proto->chosen->p
		     + GET_ISAKMP_GEN_LENGTH (proto->chosen->p)))
		return "";

	      type = GET_ISAKMP_ATTR_TYPE (attr);
	      fmt = ISAKMP_ATTR_FORMAT (type);
	      type = ISAKMP_ATTR_TYPE (type);
	      value = attr + (fmt ? ISAKMP_ATTR_LENGTH_VALUE_OFF :
			      ISAKMP_ATTR_VALUE_OFF);
	      len = (fmt ? ISAKMP_ATTR_LENGTH_VALUE_LEN :
		     GET_ISAKMP_ATTR_LENGTH_VALUE (attr));

	      if (value + len > proto->chosen->p +
		  GET_ISAKMP_GEN_LENGTH (proto->chosen->p))
		return "";

	      switch (type)
		{
		case IPSEC_ATTR_SA_LIFE_TYPE:
		  lifetype = decode_16 (value);
		  break;

		case IPSEC_ATTR_SA_LIFE_DURATION:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      if (lifetype == IPSEC_DURATION_SECONDS)
			{
			  if (len == 2)
			    sprintf (ah_life_seconds, "%u",
				     decode_16 (value));
			  else
			    sprintf (ah_life_seconds, "%u",
				     decode_32 (value));
			}
		      else
			{
			  if (len == 2)
			    sprintf (ah_life_kbytes, "%u",
				     decode_16 (value));
			  else
			    sprintf (ah_life_kbytes, "%u",
				     decode_32 (value));
			}

		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      if (lifetype == IPSEC_DURATION_SECONDS)
			{
			  if (len == 2)
			    sprintf (esp_life_seconds, "%u",
				     decode_16 (value));
			  else
			    sprintf (esp_life_seconds, "%u",
				     decode_32 (value));
			}
		      else
			{
			  if (len == 2)
			    sprintf (esp_life_kbytes, "%u",
				     decode_16 (value));
			  else
			    sprintf (esp_life_kbytes, "%u",
				     decode_32 (value));
			}

		      break;

		    case IPSEC_PROTO_IPCOMP:
		      if (lifetype == IPSEC_DURATION_SECONDS)
			{
			  if (len == 2)
			    sprintf (comp_life_seconds, "%u",
				     decode_16 (value));
			  else
			    sprintf (comp_life_seconds, "%u",
				     decode_32 (value));
			}
		      else
			{
			  if (len == 2)
			    sprintf (comp_life_kbytes, "%u",
				     decode_16 (value));
			  else
			    sprintf (comp_life_kbytes, "%u",
				     decode_32 (value));
			}

		      break;
		    }
		  break;

		case IPSEC_ATTR_GROUP_DESCRIPTION:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      sprintf (ah_group_desc, "%u", decode_16 (value));
		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      sprintf (esp_group_desc, "%u",
			       decode_16 (value));
		      break;

		    case IPSEC_PROTO_IPCOMP:
		      sprintf (comp_group_desc, "%u",
			       decode_16 (value));
		      break;
		    }
		  break;

		case IPSEC_ATTR_ENCAPSULATION_MODE:
		  if (decode_16 (value) == IPSEC_ENCAP_TUNNEL)
		    switch (proto->proto)
		      {
		      case IPSEC_PROTO_IPSEC_AH:
			ah_encapsulation = "tunnel";
			break;

		      case IPSEC_PROTO_IPSEC_ESP:
			esp_encapsulation = "tunnel";
			break;

		      case IPSEC_PROTO_IPCOMP:
			comp_encapsulation = "tunnel";
			break;
		      }
		  else
		    switch (proto->proto)
		      {
		      case IPSEC_PROTO_IPSEC_AH:
			ah_encapsulation = "transport";
			break;

		      case IPSEC_PROTO_IPSEC_ESP:
			esp_encapsulation = "transport";
			break;

		      case IPSEC_PROTO_IPCOMP:
			comp_encapsulation = "transport";
			break;
		      }
		  break;

		case IPSEC_ATTR_AUTHENTICATION_ALGORITHM:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      switch (decode_16 (value))
			{
			case IPSEC_AUTH_HMAC_MD5:
			  ah_auth_alg = "hmac-md5";
			  break;

			case IPSEC_AUTH_HMAC_SHA:
			  ah_auth_alg = "hmac-sha";
			  break;

			case IPSEC_AUTH_HMAC_RIPEMD:
			  ah_auth_alg = "hmac-ripemd";
			  break;

			case IPSEC_AUTH_DES_MAC:
			  ah_auth_alg = "des-mac";
			  break;

			case IPSEC_AUTH_KPDK:
			  ah_auth_alg = "kpdk";
			  break;
			}
		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      switch (decode_16 (value))
			{
			case IPSEC_AUTH_HMAC_MD5:
			  esp_auth_alg = "hmac-md5";
			  break;

			case IPSEC_AUTH_HMAC_SHA:
			  esp_auth_alg = "hmac-sha";
			  break;

			case IPSEC_AUTH_HMAC_RIPEMD:
			  esp_auth_alg = "hmac-ripemd";
			  break;

			case IPSEC_AUTH_DES_MAC:
			  esp_auth_alg = "des-mac";
			  break;

			case IPSEC_AUTH_KPDK:
			  esp_auth_alg = "kpdk";
			  break;
			}
		      break;
		    }
		  break;

		case IPSEC_ATTR_KEY_LENGTH:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      sprintf (ah_key_length, "%u", decode_16 (value));
		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      sprintf (esp_key_length, "%u",
			       decode_16 (value));
		      break;
		    }
		  break;

		case IPSEC_ATTR_KEY_ROUNDS:
		  switch (proto->proto)
		    {
		    case IPSEC_PROTO_IPSEC_AH:
		      sprintf (ah_key_rounds, "%u", decode_16 (value));
		      break;

		    case IPSEC_PROTO_IPSEC_ESP:
		      sprintf (esp_key_rounds, "%u",
			       decode_16 (value));
		      break;
		    }
		  break;

		case IPSEC_ATTR_COMPRESS_DICTIONARY_SIZE:
		  sprintf (comp_dict_size, "%u", decode_16 (value));
		  break;

		case IPSEC_ATTR_COMPRESS_PRIVATE_ALGORITHM:
		  sprintf (comp_private_alg, "%u", decode_16 (value));
		  break;
		}
	    }
	}

      /* XXX IPv4-specific.  */
      policy_sa->transport->vtbl->get_src (policy_sa->transport,
					   (struct sockaddr **)&sin, &fmt);
      my_inet_ntop4 (&(sin->sin_addr.s_addr), local_ike_address,
		     sizeof local_ike_address - 1, 0);

      policy_sa->transport->vtbl->get_dst (policy_sa->transport,
					   (struct sockaddr **)&sin, &fmt);
      my_inet_ntop4 (&(sin->sin_addr.s_addr), remote_ike_address,
		     sizeof remote_ike_address - 1, 0);

      switch (policy_isakmp_sa->exch_type)
	{
	case ISAKMP_EXCH_AGGRESSIVE:
	  phase_1 = "aggressive";
	  break;

	case ISAKMP_EXCH_ID_PROT:
	  phase_1 = "main";
	  break;
	}

      if (policy_isakmp_sa->initiator)
        {
	  id = policy_isakmp_sa->id_r;
	  id_sz = policy_isakmp_sa->id_r_len;
	}
      else
        {
	  id = policy_isakmp_sa->id_i;
	  id_sz = policy_isakmp_sa->id_i_len;
	}

      switch (id[0])
        {
	case IPSEC_ID_IPV4_ADDR:
	  remote_id_type = "IPv4 address";

	  net = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ);
	  my_inet_ntop4 (&net, remote_id_addr_upper,
			 sizeof remote_id_addr_upper - 1, 1);
	  my_inet_ntop4 (&net, remote_id_addr_lower,
			 sizeof remote_id_addr_lower - 1, 1);
	  remote_id = strdup (remote_id_addr_upper);
	  if (!remote_id)
  	    {
	      log_error ("policy_callback: strdup (\"%s\") failed",
			 remote_id_addr_upper);
	      goto bad;
	    }
	  break;

	case IPSEC_ID_IPV4_RANGE:
	  remote_id_type = "IPv4 range";

	  net = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ);
	  my_inet_ntop4 (&net, remote_id_addr_lower,
			 sizeof remote_id_addr_lower - 1, 1);
	  net = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ + 4);
	  my_inet_ntop4 (&net, remote_id_addr_upper,
			 sizeof remote_id_addr_upper - 1, 1);
	  remote_id = calloc (strlen (remote_id_addr_upper)
			      + strlen (remote_id_addr_lower) + 2,
			      sizeof (char));
	  if (!remote_id)
	    {
	      log_error ("policy_callback: calloc (%d, %d) failed",
			 strlen (remote_id_addr_upper)
			 + strlen (remote_id_addr_lower) + 2,
			 sizeof (char));
	      goto bad;
	    }

	  strcpy (remote_id, remote_id_addr_lower);
	  remote_id[strlen (remote_id_addr_lower)] = '-';
	  strcpy (remote_id + strlen (remote_id_addr_lower) + 1,
		  remote_id_addr_upper);
	  break;

	case IPSEC_ID_IPV4_ADDR_SUBNET:
	  remote_id_type = "IPv4 subnet";

	  net = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ);
	  subnet = decode_32 (id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ + 4);
	  net &= subnet;
	  my_inet_ntop4 (&net, remote_id_addr_lower,
			 sizeof remote_id_addr_lower - 1, 1);
	  net |= ~subnet;
	  my_inet_ntop4 (&net, remote_id_addr_upper,
			 sizeof remote_id_addr_upper - 1, 1);
	  remote_id = calloc (strlen (remote_id_addr_upper)
			      + strlen (remote_id_addr_lower) + 2,
			      sizeof (char));
	  if (!remote_id)
	    {
	      log_error ("policy_callback: calloc (%d, %d) failed",
			 strlen (remote_id_addr_upper)
			 + strlen (remote_id_addr_lower) + 2,
			 sizeof (char));
	      goto bad;
	    }

	  strcpy (remote_id, remote_id_addr_lower);
	  remote_id[strlen (remote_id_addr_lower)] = '-';
	  strcpy (remote_id + strlen (remote_id_addr_lower) + 1,
		  remote_id_addr_upper);
	  break;

	case IPSEC_ID_IPV6_ADDR:
	  /* XXX Not yet implemented.  */
	  remote_id_type = "IPv6 address";
	  break;

	case IPSEC_ID_IPV6_RANGE:
	  /* XXX Not yet implemented.  */
	  remote_id_type = "IPv6 range";
	  break;

	case IPSEC_ID_IPV6_ADDR_SUBNET:
	  /* XXX Not yet implemented.  */
	  remote_id_type = "IPv6 address";
	  break;

	case IPSEC_ID_FQDN:
	  remote_id_type = "FQDN";
	  remote_id = calloc (id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ + 1,
			      sizeof (char));
	  if (!remote_id)
	    {
	      log_error ("policy_callback: calloc (%d, %d) failed",
			 id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ + 1,
			 sizeof (char));
	      goto bad;
	    }
	  memcpy (remote_id, id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ,
		  id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ);
	  break;

	case IPSEC_ID_USER_FQDN:
	  remote_id_type = "User FQDN";
	  remote_id = calloc (id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ + 1,
			      sizeof (char));
	  if (!remote_id)
	    {
	      log_error ("policy_callback: calloc (%d, %d) failed",
			 id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ + 1,
			 sizeof (char));
	      goto bad;
	    }
	  memcpy (remote_id, id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ,
		  id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ);
	  break;

	case IPSEC_ID_DER_ASN1_DN: /* XXX -- not sure what's in this.  */
	  remote_id_type = "ASN1 DN";
	  break;

	case IPSEC_ID_DER_ASN1_GN: /* XXX -- not sure what's in this.  */
	  remote_id_type = "ASN1 GN";
	  break;

	case IPSEC_ID_KEY_ID:
	  remote_id_type = "Key ID";
	  remote_id
	    = calloc (2 * (id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ) + 1,
		      sizeof (char));
	  if (!remote_id)
	    {
	      log_error ("policy_callback: calloc (%d, %d) failed",
			 2 * (id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ) + 1,
			 sizeof (char));
	      goto bad;
	    }
          for (i = 0; i < id_sz - ISAKMP_ID_DATA_OFF + ISAKMP_GEN_SZ; i++)
	    {
	      remote_id[2 * i]
		= hextab[*(id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ) >> 4];
	      remote_id[2 * i + 1]
		= hextab[*(id + ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ) & 0xF];
	    }
	  break;

	default:
	  log_print ("policy_callback: unknown remote ID type %u", id[0]);
	  goto bad;
	}

      switch (id[1])
        {
	case IPPROTO_TCP:
	  remote_id_proto = "tcp";
	  break;

	case IPPROTO_UDP:
	  remote_id_proto = "udp";
	  break;

#ifdef IPPROTO_ETHERIP
	case IPPROTO_ETHERIP:
	  remote_id_proto = "etherip";
	  break;
#endif

 	default:
	  sprintf (remote_id_proto_num, "%2d", id[1]);
	  remote_id_proto = remote_id_proto_num;
	  break;
	}

      snprintf (remote_id_port, sizeof remote_id_port - 1, "%u",
		decode_16 (id + 2));

      if (policy_exchange->initiator)
        {
	  initiator = "yes";
	  idlocal = ie->id_ci;
	  idremote = ie->id_cr;
	  idlocalsz = ie->id_ci_sz;
	  idremotesz = ie->id_cr_sz;
        }
      else
        {
	  initiator = "no";
	  idlocal = ie->id_cr;
	  idremote = ie->id_ci;
	  idlocalsz = ie->id_cr_sz;
	  idremotesz = ie->id_ci_sz;
	}

      /* Initialize the ID variables.  */
      if (idremote)
        {
	  switch (GET_ISAKMP_ID_TYPE (idremote))
	    {
	    case IPSEC_ID_IPV4_ADDR:
	      remote_filter_type = "IPv4 address";

	      net = decode_32 (idremote + ISAKMP_ID_DATA_OFF);
	      my_inet_ntop4 (&net, remote_filter_addr_upper,
			     sizeof remote_filter_addr_upper - 1, 1);
	      my_inet_ntop4 (&net, remote_filter_addr_lower,
			     sizeof remote_filter_addr_lower - 1, 1);
	      remote_filter = strdup (remote_filter_addr_upper);
	      if (!remote_filter)
	        {
		  log_error ("policy_callback: strdup (\"%s\") failed",
			     remote_filter_addr_upper);
		  goto bad;
		}
	      break;

	    case IPSEC_ID_IPV4_RANGE:
	      remote_filter_type = "IPv4 range";

	      net = decode_32 (idremote + ISAKMP_ID_DATA_OFF);
	      my_inet_ntop4 (&net, remote_filter_addr_lower,
			     sizeof remote_filter_addr_lower - 1, 1);
	      net = decode_32 (idremote + ISAKMP_ID_DATA_OFF + 4);
	      my_inet_ntop4 (&net, remote_filter_addr_upper,
			     sizeof remote_filter_addr_upper - 1, 1);
	      remote_filter = calloc (strlen (remote_filter_addr_upper)
				      + strlen (remote_filter_addr_lower) + 2,
				      sizeof (char));
	      if (!remote_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     strlen (remote_filter_addr_upper)
			     + strlen (remote_filter_addr_lower) + 2,
			     sizeof (char));
		  goto bad;
		}
	      strcpy (remote_filter, remote_filter_addr_lower);
	      remote_filter[strlen (remote_filter_addr_lower)] = '-';
	      strcpy (remote_filter + strlen (remote_filter_addr_lower) + 1,
		      remote_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV4_ADDR_SUBNET:
	      remote_filter_type = "IPv4 subnet";

	      net = decode_32 (idremote + ISAKMP_ID_DATA_OFF);
	      subnet = decode_32 (idremote + ISAKMP_ID_DATA_OFF + 4);
	      net &= subnet;
	      my_inet_ntop4 (&net, remote_filter_addr_lower,
			     sizeof remote_filter_addr_lower - 1, 1);
	      net |= ~subnet;
	      my_inet_ntop4 (&net, remote_filter_addr_upper,
			     sizeof remote_filter_addr_upper - 1, 1);
	      remote_filter = calloc (strlen (remote_filter_addr_upper)
				      + strlen (remote_filter_addr_lower) + 2,
				      sizeof (char));
	      if (!remote_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     strlen (remote_filter_addr_upper)
			     + strlen (remote_filter_addr_lower) + 2,
			     sizeof (char));
		  goto bad;
		}
	      strcpy (remote_filter, remote_filter_addr_lower);
	      remote_filter[strlen (remote_filter_addr_lower)] = '-';
	      strcpy (remote_filter + strlen (remote_filter_addr_lower) + 1,
		      remote_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV6_ADDR:
	      /* XXX Not yet implemented.  */
	      remote_filter_type = "IPv6 address";
	      break;

	    case IPSEC_ID_IPV6_RANGE:
	      /* XXX Not yet implemented.  */
	      remote_filter_type = "IPv6 range";
	      break;

	    case IPSEC_ID_IPV6_ADDR_SUBNET:
	      /* XXX Not yet implemented.  */
	      remote_filter_type = "IPv6 address";
	      break;

	    case IPSEC_ID_FQDN:
	      remote_filter_type = "FQDN";
	      remote_filter = calloc (idremotesz - ISAKMP_ID_DATA_OFF + 1,
				      sizeof (char));
	      if (!remote_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     idremotesz - ISAKMP_ID_DATA_OFF + 1,
			     sizeof (char));
		  goto bad;
		}
	      memcpy (remote_filter, idremote + ISAKMP_ID_DATA_OFF,
		      idremotesz);
	      break;

	    case IPSEC_ID_USER_FQDN:
	      remote_filter_type = "User FQDN";
	      remote_filter = calloc (idremotesz - ISAKMP_ID_DATA_OFF + 1,
				      sizeof (char));
	      if (!remote_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     idremotesz - ISAKMP_ID_DATA_OFF + 1,
			     sizeof (char));
		  goto bad;
		}
	      memcpy (remote_filter, idremote + ISAKMP_ID_DATA_OFF,
		      idremotesz);
	      break;

	    case IPSEC_ID_DER_ASN1_DN: /* XXX -- not sure what's in this.  */
	      remote_filter_type = "ASN1 DN";
	      break;

	    case IPSEC_ID_DER_ASN1_GN: /* XXX -- not sure what's in this.  */
	      remote_filter_type = "ASN1 GN";
	      break;

	    case IPSEC_ID_KEY_ID:
	      remote_filter_type = "Key ID";
	      remote_filter
		= calloc (2 * (idremotesz - ISAKMP_ID_DATA_OFF) + 1,
			  sizeof (char));
	      if (!remote_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     2 * (idremotesz - ISAKMP_ID_DATA_OFF) + 1,
			     sizeof (char));
		  goto bad;
	        }
              for (i = 0; i < idremotesz - ISAKMP_ID_DATA_OFF; i++)
	        {
		  remote_filter[2 * i]
		    = hextab[*(idremote + ISAKMP_ID_DATA_OFF) >> 4];
		  remote_filter[2 * i + 1]
		    = hextab[*(idremote + ISAKMP_ID_DATA_OFF) & 0xF];
	        }
	      break;

	    default:
	      log_print ("policy_callback: unknown Remote ID type %u",
			 GET_ISAKMP_ID_TYPE (idremote));
	      goto bad;
	    }

	  switch (idremote[ISAKMP_GEN_SZ + 1])
	    {
	    case IPPROTO_TCP:
	      remote_filter_proto = "tcp";
	      break;

	    case IPPROTO_UDP:
	      remote_filter_proto = "udp";
	      break;

#ifdef IPPROTO_ETHERIP
	    case IPPROTO_ETHERIP:
	      remote_filter_proto = "etherip";
	      break;
#endif

 	    default:
	      sprintf (remote_filter_proto_num, "%2d",
		       idremote[ISAKMP_GEN_SZ + 1]);
	      remote_filter_proto = remote_filter_proto_num;
	      break;
	    }

	  snprintf (remote_filter_port, sizeof remote_filter_port - 1,
		    "%u", decode_16 (idremote + ISAKMP_GEN_SZ + 2));
	}
      else
        {
	  policy_sa->transport->vtbl->get_dst (policy_sa->transport,
					       (struct sockaddr **) &sin,
					       &fmt);
	  remote_filter_type = "IPv4 address";

	  my_inet_ntop4 (&(sin->sin_addr.s_addr), remote_filter_addr_upper,
			 sizeof remote_filter_addr_upper - 1, 0);
	  my_inet_ntop4 (&(sin->sin_addr.s_addr), remote_filter_addr_lower,
			 sizeof remote_filter_addr_lower - 1, 0);
	  remote_filter = strdup (remote_filter_addr_upper);
	  if (!remote_filter)
	    {
	      log_error ("policy_callback: strdup (\"%s\") failed",
			 remote_filter_addr_upper);
	      goto bad;
	    }
	}

      if (idlocal)
        {
	  switch (GET_ISAKMP_ID_TYPE (idlocal))
	    {
	    case IPSEC_ID_IPV4_ADDR:
	      local_filter_type = "IPv4 address";

	      net = decode_32 (idlocal + ISAKMP_ID_DATA_OFF);
	      my_inet_ntop4 (&net, local_filter_addr_upper,
			     sizeof local_filter_addr_upper - 1, 1);
	      my_inet_ntop4 (&net, local_filter_addr_lower,
			     sizeof local_filter_addr_upper - 1, 1);
	      local_filter = strdup (local_filter_addr_upper);
	      if (!local_filter)
	        {
		  log_error ("policy_callback: strdup (\"%s\") failed",
			     local_filter_addr_upper);
		  goto bad;
		}
	      break;

	    case IPSEC_ID_IPV4_RANGE:
	      local_filter_type = "IPv4 range";

	      net = decode_32 (idlocal + ISAKMP_ID_DATA_OFF);
	      my_inet_ntop4 (&net, local_filter_addr_lower,
			     sizeof local_filter_addr_lower - 1, 1);
	      net = decode_32 (idlocal + ISAKMP_ID_DATA_OFF + 4);
	      my_inet_ntop4 (&net, local_filter_addr_upper,
			     sizeof local_filter_addr_upper - 1, 1);
	      local_filter = calloc (strlen (local_filter_addr_upper)
				     + strlen (local_filter_addr_lower) + 2,
				     sizeof (char));
	      if (!local_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     strlen (local_filter_addr_upper)
			     + strlen (local_filter_addr_lower) + 2,
			     sizeof (char));
		  goto bad;
		}
	      strcpy (local_filter, local_filter_addr_lower);
	      local_filter[strlen (local_filter_addr_lower)] = '-';
	      strcpy (local_filter + strlen (local_filter_addr_lower) + 1,
		      local_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV4_ADDR_SUBNET:
	      local_filter_type = "IPv4 subnet";

	      net = decode_32 (idlocal + ISAKMP_ID_DATA_OFF);
	      subnet = decode_32 (idlocal + ISAKMP_ID_DATA_OFF + 4);
	      net &= subnet;
	      my_inet_ntop4 (&net, local_filter_addr_lower,
			     sizeof local_filter_addr_lower - 1, 1);
	      net |= ~subnet;
	      my_inet_ntop4 (&net, local_filter_addr_upper,
			     sizeof local_filter_addr_upper - 1, 1);
	      local_filter = calloc (strlen (local_filter_addr_upper)
				     + strlen (local_filter_addr_lower) + 2,
				     sizeof (char));
	      if (!local_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     strlen (local_filter_addr_upper)
			     + strlen (local_filter_addr_lower) + 2,
			     sizeof (char));
		  goto bad;
		}
	      strcpy (local_filter, local_filter_addr_lower);
	      local_filter[strlen (local_filter_addr_lower)] = '-';
	      strcpy (local_filter + strlen (local_filter_addr_lower) + 1,
		      local_filter_addr_upper);
	      break;

	    case IPSEC_ID_IPV6_ADDR:
	      /* XXX Not yet implemented.  */
	      local_filter_type = "IPv6 address";
	      break;

	    case IPSEC_ID_IPV6_RANGE:
	      /* XXX Not yet implemented.  */
	      local_filter_type = "IPv6 range";
	      break;

	    case IPSEC_ID_IPV6_ADDR_SUBNET:
	      /* XXX Not yet implemented.  */
	      local_filter_type = "IPv6 address";
	      break;

	    case IPSEC_ID_FQDN:
	      local_filter_type = "FQDN";
	      local_filter = calloc (idlocalsz - ISAKMP_ID_DATA_OFF + 1,
				     sizeof (char));
	      if (!local_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     idlocalsz - ISAKMP_ID_DATA_OFF + 1,
			     sizeof (char));
		  goto bad;
		}
	      memcpy (local_filter, idlocal + ISAKMP_ID_DATA_OFF,
		      idlocalsz);
	      break;

	    case IPSEC_ID_USER_FQDN:
	      local_filter_type = "User FQDN";
	      local_filter = calloc (idlocalsz - ISAKMP_ID_DATA_OFF + 1,
				     sizeof (char));
	      if (!local_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     idlocalsz - ISAKMP_ID_DATA_OFF + 1,
			     sizeof (char));
		  goto bad;
		}
	      memcpy (local_filter, idlocal + ISAKMP_ID_DATA_OFF,
		      idlocalsz);
	      break;

	    case IPSEC_ID_DER_ASN1_DN: /* XXX -- not sure what's in this.  */
	      local_filter_type = "ASN1 DN";
	      break;

	    case IPSEC_ID_DER_ASN1_GN: /* XXX -- not sure what's in this.  */
	      local_filter_type = "ASN1 GN";
	      break;

	    case IPSEC_ID_KEY_ID:
	      local_filter_type = "Key ID";
	      local_filter = calloc (2 * (idlocalsz - ISAKMP_ID_DATA_OFF) + 1,
				     sizeof (char));
	      if (!local_filter)
	        {
		  log_error ("policy_callback: calloc (%d, %d) failed",
			     2 * (idlocalsz - ISAKMP_ID_DATA_OFF) + 1,
			     sizeof (char));
		  goto bad;
	        }
              for (i = 0; i < idremotesz - ISAKMP_ID_DATA_OFF; i++)
	        {
		  local_filter[2 * i]
		    = hextab[*(idlocal + ISAKMP_ID_DATA_OFF) >> 4];
		  local_filter[2 * i + 1]
		    = hextab[*(idlocal + ISAKMP_ID_DATA_OFF) & 0xF];
	        }
	      break;

	    default:
	      log_print ("policy_callback: unknown Local ID type %u",
			 GET_ISAKMP_ID_TYPE (idlocal));
	      goto bad;
	    }

	  switch (idlocal[ISAKMP_GEN_SZ + 1])
	    {
	    case IPPROTO_TCP:
	      local_filter_proto = "tcp";
	      break;

	    case IPPROTO_UDP:
	      local_filter_proto = "udp";
	      break;

#ifdef IPPROTO_ETHERIP
	    case IPPROTO_ETHERIP:
	      local_filter_proto = "etherip";
	      break;
#endif

 	    default:
	      sprintf (local_filter_proto_num, "%2d",
		       idlocal[ISAKMP_GEN_SZ + 1]);
	      local_filter_proto = local_filter_proto_num;
	      break;
	    }

	  snprintf (local_filter_port, sizeof local_filter_port - 1,
		    "%u", decode_16 (idlocal + ISAKMP_GEN_SZ + 2));
	}
      else
        {
	  policy_sa->transport->vtbl->get_src (policy_sa->transport,
					       (struct sockaddr **)&sin,
					       &fmt);

	  local_filter_type = "IPv4 address";

	  my_inet_ntop4 (&(sin->sin_addr.s_addr), local_filter_addr_upper,
			 sizeof local_filter_addr_upper - 1, 0);
	  my_inet_ntop4 (&(sin->sin_addr.s_addr), local_filter_addr_lower,
			 sizeof local_filter_addr_lower - 1, 0);
	  local_filter = strdup (local_filter_addr_upper);
	  if (!local_filter)
	    {
	      log_error ("policy_callback: strdup (\"%s\") failed",
			 local_filter_addr_upper);
	      goto bad;
	    }
        }

      LOG_DBG ((LOG_POLICY, 80, "Policy context (action attributes):"));
      LOG_DBG ((LOG_POLICY, 80, "esp_present == %s", esp_present));
      LOG_DBG ((LOG_POLICY, 80, "ah_present == %s", ah_present));
      LOG_DBG ((LOG_POLICY, 80, "comp_present == %s", comp_present));
      LOG_DBG ((LOG_POLICY, 80, "ah_hash_alg == %s", ah_hash_alg));
      LOG_DBG ((LOG_POLICY, 80, "esp_enc_alg == %s", esp_enc_alg));
      LOG_DBG ((LOG_POLICY, 80, "comp_alg == %s", comp_alg));
      LOG_DBG ((LOG_POLICY, 80, "ah_auth_alg == %s", ah_auth_alg));
      LOG_DBG ((LOG_POLICY, 80, "esp_auth_alg == %s", esp_auth_alg));
      LOG_DBG ((LOG_POLICY, 80, "ah_life_seconds == %s", ah_life_seconds));
      LOG_DBG ((LOG_POLICY, 80, "ah_life_kbytes == %s", ah_life_kbytes));
      LOG_DBG ((LOG_POLICY, 80, "esp_life_seconds == %s", esp_life_seconds));
      LOG_DBG ((LOG_POLICY, 80, "esp_life_kbytes == %s", esp_life_kbytes));
      LOG_DBG ((LOG_POLICY, 80, "comp_life_seconds == %s", comp_life_seconds));
      LOG_DBG ((LOG_POLICY, 80, "comp_life_kbytes == %s", comp_life_kbytes));
      LOG_DBG ((LOG_POLICY, 80, "ah_encapsulation == %s", ah_encapsulation));
      LOG_DBG ((LOG_POLICY, 80, "esp_encapsulation == %s", esp_encapsulation));
      LOG_DBG ((LOG_POLICY, 80, "comp_encapsulation == %s",
		comp_encapsulation));
      LOG_DBG ((LOG_POLICY, 80, "comp_dict_size == %s", comp_dict_size));
      LOG_DBG ((LOG_POLICY, 80, "comp_private_alg == %s", comp_private_alg));
      LOG_DBG ((LOG_POLICY, 80, "ah_key_length == %s", ah_key_length));
      LOG_DBG ((LOG_POLICY, 80, "ah_key_rounds == %s", ah_key_rounds));
      LOG_DBG ((LOG_POLICY, 80, "esp_key_length == %s", esp_key_length));
      LOG_DBG ((LOG_POLICY, 80, "esp_key_rounds == %s", esp_key_rounds));
      LOG_DBG ((LOG_POLICY, 80, "ah_group_desc == %s", ah_group_desc));
      LOG_DBG ((LOG_POLICY, 80, "esp_group_desc == %s", esp_group_desc));
      LOG_DBG ((LOG_POLICY, 80, "comp_group_desc == %s", comp_group_desc));
      LOG_DBG ((LOG_POLICY, 80, "remote_filter_type == %s",
		remote_filter_type));
      LOG_DBG ((LOG_POLICY, 80, "remote_filter_addr_upper == %s",
		remote_filter_addr_upper));
      LOG_DBG ((LOG_POLICY, 80, "remote_filter_addr_lower == %s",
		remote_filter_addr_lower));
      LOG_DBG ((LOG_POLICY, 80, "remote_filter == %s",
		(remote_filter ? remote_filter : "")));
      LOG_DBG ((LOG_POLICY, 80, "remote_filter_port == %s",
		remote_filter_port));
      LOG_DBG ((LOG_POLICY, 80, "remote_filter_proto == %s",
		remote_filter_proto));
      LOG_DBG ((LOG_POLICY, 80, "local_filter_type == %s", local_filter_type));
      LOG_DBG ((LOG_POLICY, 80, "local_filter_addr_upper == %s",
		local_filter_addr_upper));
      LOG_DBG ((LOG_POLICY, 80, "local_filter_addr_lower == %s",
		local_filter_addr_lower));
      LOG_DBG ((LOG_POLICY, 80, "local_filter == %s",
		(local_filter ? local_filter : "")));
      LOG_DBG ((LOG_POLICY, 80, "local_filter_port == %s", local_filter_port));
      LOG_DBG ((LOG_POLICY, 80, "local_filter_proto == %s",
		local_filter_proto));
      LOG_DBG ((LOG_POLICY, 80, "remote_id_type == %s", remote_id_type));
      LOG_DBG ((LOG_POLICY, 80, "remote_id_addr_upper == %s",
		remote_id_addr_upper));
      LOG_DBG ((LOG_POLICY, 80, "remote_id_addr_lower == %s",
		remote_id_addr_lower));
      LOG_DBG ((LOG_POLICY, 80, "remote_id == %s",
		(remote_id ? remote_id : "")));
      LOG_DBG ((LOG_POLICY, 80, "remote_id_port == %s", remote_id_port));
      LOG_DBG ((LOG_POLICY, 80, "remote_id_proto == %s", remote_id_proto));
      LOG_DBG ((LOG_POLICY, 80, "remote_negotiation_address == %s",
		remote_ike_address));
      LOG_DBG ((LOG_POLICY, 80, "local_negotiation_address == %s",
		local_ike_address));
      LOG_DBG ((LOG_POLICY, 80, "pfs == %s", pfs));
      LOG_DBG ((LOG_POLICY, 80, "initiator == %s", initiator));
      LOG_DBG ((LOG_POLICY, 80, "phase1_group_desc == %s", phase1_group));

      /* Unset dirty now.  */
      dirty = 0;
    }

  if (strcmp (name, "phase_1") == 0)
    return phase_1;

  if (strcmp (name, "GMTTimeOfDay") == 0)
    {
      tt = time ((time_t) NULL);
      strftime (mytimeofday, 14, "%G%m%d%H%M%S", gmtime (&tt));
      return mytimeofday;
    }

  if (strcmp (name, "LocalTimeOfDay") == 0)
    {
      tt = time ((time_t) NULL);
      strftime (mytimeofday, 14, "%G%m%d%H%M%S", localtime (&tt));
      return mytimeofday;
    }

  if (strcmp (name, "initiator") == 0)
    return initiator;

  if (strcmp (name, "pfs") == 0)
    return pfs;

  if (strcmp (name, "app_domain") == 0)
    return "IPsec policy";

  if (strcmp (name, "doi") == 0)
    return "ipsec";

  if (strcmp (name, "esp_present") == 0)
    return esp_present;

  if (strcmp (name, "ah_present") == 0)
    return ah_present;

  if (strcmp (name, "comp_present") == 0)
    return comp_present;

  if (strcmp (name, "ah_hash_alg") == 0)
    return ah_hash_alg;

  if (strcmp (name, "ah_auth_alg") == 0)
    return ah_auth_alg;

  if (strcmp (name, "esp_auth_alg") == 0)
    return esp_auth_alg;

  if (strcmp (name, "esp_enc_alg") == 0)
    return esp_enc_alg;

  if (strcmp (name, "comp_alg") == 0)
    return comp_alg;

  if (strcmp (name, "ah_life_kbytes") == 0)
    return ah_life_kbytes;

  if (strcmp (name, "ah_life_seconds") == 0)
    return ah_life_seconds;

  if (strcmp (name, "esp_life_kbytes") == 0)
    return esp_life_kbytes;

  if (strcmp (name, "esp_life_seconds") == 0)
    return esp_life_seconds;

  if (strcmp (name, "comp_life_kbytes") == 0)
    return comp_life_kbytes;

  if (strcmp (name, "comp_life_seconds") == 0)
    return comp_life_seconds;

  if (strcmp (name, "ah_encapsulation") == 0)
    return ah_encapsulation;

  if (strcmp (name, "esp_encapsulation") == 0)
    return esp_encapsulation;

  if (strcmp (name, "comp_encapsulation") == 0)
    return comp_encapsulation;

  if (strcmp (name, "ah_key_length") == 0)
    return ah_key_length;

  if (strcmp (name, "ah_key_rounds") == 0)
    return ah_key_rounds;

  if (strcmp (name, "esp_key_length") == 0)
    return esp_key_length;

  if (strcmp (name, "esp_key_rounds") == 0)
    return esp_key_rounds;

  if (strcmp (name, "comp_dict_size") == 0)
    return comp_dict_size;

  if (strcmp (name, "comp_private_alg") == 0)
    return comp_private_alg;

  if (strcmp (name, "remote_filter_type") == 0)
    return remote_filter_type;

  if (strcmp (name, "remote_filter") == 0)
    return (remote_filter ? remote_filter : "");

  if (strcmp (name, "remote_filter_addr_upper") == 0)
    return remote_filter_addr_upper;

  if (strcmp (name, "remote_filter_addr_lower") == 0)
    return remote_filter_addr_lower;

  if (strcmp (name, "remote_filter_port") == 0)
    return remote_filter_port;

  if (strcmp (name, "remote_filter_proto") == 0)
    return remote_filter_proto;

  if (strcmp (name, "local_filter_type") == 0)
    return local_filter_type;

  if (strcmp (name, "local_filter") == 0)
    return (local_filter ? local_filter : "");

  if (strcmp (name, "local_filter_addr_upper") == 0)
    return local_filter_addr_upper;

  if (strcmp (name, "local_filter_addr_lower") == 0)
    return local_filter_addr_lower;

  if (strcmp (name, "local_filter_port") == 0)
    return local_filter_port;

  if (strcmp (name, "local_filter_proto") == 0)
    return local_filter_proto;

  if (strcmp (name, "remote_ike_address") == 0)
    return remote_ike_address;

  if (strcmp (name, "remote_negotiation_address") == 0)
    return remote_ike_address;

  if (strcmp (name, "local_ike_address") == 0)
    return local_ike_address;

  if (strcmp (name, "local_negotiation_address") == 0)
    return local_ike_address;

  if (strcmp (name, "remote_id_type") == 0)
    return remote_id_type;

  if (strcmp (name, "remote_id") == 0)
    return (remote_id ? remote_id : "");

  if (strcmp (name, "remote_id_addr_upper") == 0)
    return remote_id_addr_upper;

  if (strcmp (name, "remote_id_addr_lower") == 0)
    return remote_id_addr_lower;

  if (strcmp (name, "remote_id_port") == 0)
    return remote_id_port;

  if (strcmp (name, "remote_id_proto") == 0)
    return remote_id_proto;

  if (strcmp (name, "phase1_group_desc") == 0)
    return phase1_group;

  return "";

 bad:
  policy_callback (KEYNOTE_CALLBACK_INITIALIZE);
  return "";
}

void
policy_init (void)
{
  char *ptr, *policy_file;
  char **asserts;
  off_t sz;
  int fd, len, i;

  LOG_DBG ((LOG_POLICY, 30, "policy_init: initializing"));

#if defined (HAVE_DLOPEN) && !defined (USE_KEYNOTE)
  if (!dyn_load (libkeynote_script))
    return;
#endif

  /* If there exists a session already, release all its resources.  */
  if (keynote_sessid != -1)
    LK (kn_close, (keynote_sessid));

  /* Initialize a session.  */
  keynote_sessid = LK (kn_init, ());
  if (keynote_sessid == -1)
    log_fatal ("policy_init: kn_init () failed");

  /* Get policy file from configuration.  */
  policy_file = conf_get_str ("General", "Policy-file");
  if (!policy_file)
    policy_file = POLICY_FILE_DEFAULT;

  /* Check file modes and collect file size */
  if (check_file_secrecy (policy_file, &sz))
    log_fatal ("policy_init: cannot read %s", policy_file);

  /* Open policy file.  */
  fd = open (policy_file, O_RDONLY);
  if (fd == -1)
    log_fatal ("policy_init: open (\"%s\", O_RDONLY) failed", policy_file);

  /* Allocate memory to keep policies.  */
  ptr = calloc (sz + 1, sizeof (char));
  if (!ptr)
    log_fatal ("policy_init: calloc (%d, %d) failed", sz + 1,
	       sizeof (char));

  /* Just in case there are short reads...  */
  for (len = 0; len < sz; len += i)
    {
      i = read (fd, ptr + len, sz - len);
      if (i == -1)
	log_fatal ("policy_init: read (%d, %p, %d) failed", fd, ptr + len,
		   sz - len);
    }

  /* We're done with this.  */
  close (fd);

  /* Parse buffer, break up into individual policies.  */
  asserts = LK (kn_read_asserts, (ptr, sz, &i));

  /* Begone!  */
  free (ptr);

  /* Add each individual policy in the session.  */
  for (fd = 0; fd < i; fd++)
    {
      if (LK (kn_add_assertion, (keynote_sessid, asserts[fd],
				 strlen (asserts[fd]), ASSERT_FLAG_LOCAL))
	  == -1)
        log_print ("policy_init: "
		   "kn_add_assertion (%d, %p, %d, ASSERT_FLAG_LOCAL) failed",
                   keynote_sessid, asserts[fd], strlen (asserts[fd]));
    }

  /* Cleanup */
  if (keynote_policy_asserts)
    {
      for (fd = 0; fd < keynote_policy_asserts_num; fd++)
        if (keynote_policy_asserts && keynote_policy_asserts[fd])
          free (keynote_policy_asserts[fd]);

      free (keynote_policy_asserts);
    }

  keynote_policy_asserts = asserts;
  keynote_policy_asserts_num = i;
}

/* Nothing needed for initialization */
int
keynote_cert_init (void)
{
  return 1;
}

/* Just copy and return.  */
void *
keynote_cert_get (u_int8_t *data, u_int32_t len)
{
  char *foo = calloc (len + 1, sizeof (char));

  if (foo == NULL)
    return NULL;

  memcpy (foo, data, len);
  return foo;
}

/*
 * We just verify the signature on the credentials.
 * On signature failure, just drop the whole payload.
 */
int
keynote_cert_validate (void *scert)
{
  char **foo;
  int num, i;

  if (scert == NULL)
    return 0;

  foo = LK (kn_read_asserts, ((char *) scert, strlen ((char *) scert),
			      &num));
  if (foo == NULL)
    return 0;

  for (i = 0; i < num; i++)
    {
      if (LK (kn_verify_assertion, (scert, strlen ((char *) scert)))
	  != SIGRESULT_TRUE)
        {
	  for (; i < num; i++)
	    free (foo[i]);
	  free (foo);
	  return 0;
	}

      free (foo[i]);
    }

  free (foo);
  return 1;
}

/* Add received credentials.  */
int
keynote_cert_insert (int sid, void *scert)
{
  char **foo;
  int num;

  if (scert == NULL)
    return 0;

  foo = LK (kn_read_asserts, ((char *) scert, strlen ((char *) scert),
			      &num));
  if (foo == NULL)
    return 0;

  while (num--)
    LK (kn_add_assertion, (sid, foo[num], strlen (foo[num]), 0));

  return 1;
}

/* Just regular memory free.  */
void
keynote_cert_free (void *cert)
{
  free (cert);
}

/* Verify that the key given to us is valid.  */
int
keynote_certreq_validate (u_int8_t *data, u_int32_t len)
{
  struct keynote_deckey dc;
  int err = 1;
  char *dat;

  dat = calloc (len + 1, sizeof (char));
  if (!dat)
    {
      log_error ("keynote_certreq_validate: calloc (%d, %d) failed", len + 1,
		 sizeof (char));
	return 0;
    }

  memcpy (dat, data, len);

  if (LK (kn_decode_key, (&dc, dat, KEYNOTE_PUBLIC_KEY)) != 0)
    err = 0;
  else
    LK (kn_free_key, (&dc));

  free (dat);

  return err;
}

/* Beats me what we should be doing with this.  */
void *
keynote_certreq_decode (u_int8_t *data, u_int32_t len)
{
  /* XXX */
  return NULL;
}

void
keynote_free_aca (void *blob)
{
  /* XXX */
}

int
keynote_cert_obtain (u_int8_t *id, size_t id_len, void *data, u_int8_t **cert,
		     u_int32_t *certlen)
{
  char *dirname, *file;
  struct stat sb;
  int idtype, fd, len;

  if (!id)
    {
      log_print ("keynote_cert_obtain: ID is missing");
      return 0;
    }

  /* Get type of ID.  */
  idtype = id[0];
  id += ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ;
  id_len -= ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ;

  dirname = conf_get_str ("KeyNote", "Credential-directory");
  if (!dirname)
    {
      LOG_DBG ((LOG_POLICY, 30,
		"keynote_cert_obtain: no Credential-directory"));
      return 0;
    }

  len = strlen (dirname) + strlen (CREDENTIAL_FILE) + 3;

  switch (idtype)
    {
    case IPSEC_ID_IPV4_ADDR:
      {
	struct in_addr in;

	file = calloc (len + 15, sizeof (char));
	if (file == NULL)
	  {
	    log_error ("keynote_cert_obtain: failed to allocate %d bytes",
		       len + 15);
	    return 0;
	  }

	memcpy (&in, id, sizeof in);
	sprintf (file, "%s/%s/%s", dirname, inet_ntoa (in), CREDENTIAL_FILE);
	break;
      }

    case IPSEC_ID_FQDN:
    case IPSEC_ID_USER_FQDN:
      {
        file = calloc (len + id_len, sizeof (char));
	if (file == NULL)
	  {
	    log_error ("keynote_cert_obtain: failed to allocate %d bytes",
		       len + id_len);
	    return 0;
	  }

	sprintf (file, "%s/", dirname);
	memcpy (file + strlen (file), id, id_len);
	sprintf (file + strlen (dirname) + 1 + id_len, "/%s", CREDENTIAL_FILE);
	break;
      }

    default:
      return 0;
    }

  if (stat (file, &sb) < 0)
    {
      LOG_DBG ((LOG_POLICY, 30, "keynote_cert_obtain: failed to stat \"%s\"",
		file));
      free (file);
      return 0;
    }

  *cert = calloc (sb.st_size, sizeof (char));
  if (*cert == NULL)
    {
      log_error ("keynote_cert_obtain: failed to allocate %d bytes",
		 sb.st_size);
      free (file);
      return 0;
    }

  fd = open (file, O_RDONLY, 0);
  if (fd < 0)
    {
      LOG_DBG ((LOG_POLICY, 30, "keynote_cert_obtain: failed to open \"%s\"",
		file));
      free (file);
      return 0;
    }

  if (read (fd, *cert, sb.st_size) != sb.st_size)
    {
      LOG_DBG ((LOG_POLICY, 30, "keynote_cert_obtain: failed to read %d "
		"bytes from \"%s\"", sb.st_size, file));
      free (file);
      close (fd);
      return 0;
    }

  close (fd);
  free (file);
  *certlen = sb.st_size;
  return 1;
}

/* This should never be called.  */
int
keynote_cert_get_subjects (void *scert, int *n, u_int8_t ***id,
			   u_int32_t **id_len)
{
  return 0;
}

/* Get the authorizer key.  */
int
keynote_cert_get_key (void *scert, void *keyp)
{
  struct keynote_keylist *kl;
  int sid, num;
  char **foo;

  foo = LK (kn_read_asserts, ((char *)scert, strlen ((char *)scert), &num));
  if (foo == NULL || num == 0)
    return 0;

  sid = LK (kn_add_assertion, (keynote_sessid, foo[num - 1],
			       strlen (scert), 0));
  while (num--)
    free (foo[num]);
  free (foo);

  if (sid == -1)
    return 0;

  *(RSA **)keyp = NULL;

  kl = LK (kn_get_licensees, (keynote_sessid, sid));
  while (kl)
    {
      if (kl->key_alg == KEYNOTE_ALGORITHM_RSA)
	{
	  *(RSA **)keyp = LC (RSAPublicKey_dup, (kl->key_key));
	  break;
	}

      kl = kl->key_next;
    }

  LK (kn_remove_assertion, (keynote_sessid, sid));
  return *(RSA **)keyp == NULL ? 0 : 1;
}
