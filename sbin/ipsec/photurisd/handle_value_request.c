/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
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
 *      This product includes software developed by Niels Provos.
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
 * handle_value_request:
 * receive a VALUE_REQUEST packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: handle_value_request.c,v 1.1.1.1 1997/07/18 22:48:50 provos Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "cookie.h"
#include "config.h"
#include "buffer.h"
#include "scheme.h"
#include "packet.h"
#include "exchange.h"
#include "secrets.h"
#include "server.h"
#include "errlog.h"

int
handle_value_request(u_char *packet, int size,
		     char *address, u_short port,
		     u_int8_t *schemes, u_int16_t ssize)

{
	struct value_request *header;
	struct stateob *st;
	u_int8_t *p, *modp, *refp, *genp = NULL;
	u_int16_t i, sstart, vsize, asize, modsize, modflag;
	u_int8_t scheme_ref[2];
	u_int8_t rcookie[COOKIE_SIZE];

	if (size < VALUE_REQUEST_MIN)
	     return -1;	/* packet too small  */

	header = (struct value_request *) packet;

	st = state_find_cookies(address, header->icookie, header->rcookie);
	if (st == NULL) {
	     struct stateob tempst;
	     bzero((char *)&tempst, sizeof(tempst)); /* Set up temp. state */ 
	     tempst.initiator = 0;                   /* We are the Responder */ 
	     bcopy(header->icookie, tempst.icookie, COOKIE_SIZE); 
	     strncpy(tempst.address, address, 15); 
	     tempst.port = global_port; 
	     tempst.counter = header->counter;
	     
	     cookie_generate(&tempst, rcookie, COOKIE_SIZE); 

	     /* Check for invalid cookie */
	     if (bcmp(rcookie, header->rcookie, COOKIE_SIZE)) {
		  packet_size = PACKET_BUFFER_SIZE;
		  photuris_error_message(&tempst, packet_buffer, &packet_size,
					 header->icookie, header->rcookie,
					 header->counter, BAD_COOKIE);
		  send_packet();
		  return 0;
	     }

	     /* Check exchange value - XXX doesn't check long form */
	     p = VALUE_REQUEST_VALUE(header);
	     vsize = varpre2octets(p);
	     asize = VALUE_REQUEST_MIN + vsize;
	     if (asize >= size)
		  return -1;   /* Exchange value too big */

	     /* Check schemes - selected length is in exchange value*/
	     sstart = 0;
	     modflag = 0;
	     refp = modp = NULL;
	     *(u_int16_t *)scheme_ref = htons(scheme_get_ref(header->scheme));
	     while(sstart < ssize) {
		  p = scheme_get_mod(schemes+sstart);
		  modsize = varpre2octets(p);
		  if (!bcmp(header->scheme, schemes + sstart, 2)) {
		       modflag = 1;
		       if (modsize == vsize) {
			    genp = scheme_get_gen(schemes+sstart);
			    modp = p;
			    break;  /* On right scheme + right size */
		       } else if (modsize <= 2 && refp != NULL) {
			    modp =  refp;
			    break;
		       }
		  } else if (!bcmp(scheme_ref, schemes + sstart,2 ) && modsize == vsize) {
		       genp = scheme_get_gen(schemes+sstart);
		       if (modflag) {
			    modp = p;
			    break;
		       }
		       refp = p;
		  }
		  
		  sstart += scheme_get_len(schemes+sstart);
	     }
	     if (sstart >= ssize)
		  return -1;   /* Did not find a scheme - XXX log */

	     p = VALUE_REQUEST_VALUE(header) + vsize;
	     
	     /* Check attributes */
	     i = 0;
	     while(asize + i < size)
		  i += p[i+1] + 2;

	     /* This 'i' is used below as well as p */
	     if (asize + i != size)
		  return -1;  /* attributes dont match udp length */

	     if ((st = state_new()) == NULL)
		  return -1;

	     /* Default options */
	     st->flags = IPSEC_OPT_ENC|IPSEC_OPT_AUTH;

	     /* Fill the state object */
	     st->uSPIoattrib = calloc(i, sizeof(u_int8_t));
             if (st->uSPIoattrib == NULL) {
                  state_value_reset(st);
		  return -1;
	     }
             bcopy(p, st->uSPIoattrib, i);  
             st->uSPIoattribsize = i;  

	     /* Save scheme, which will be used by both parties */
	     vsize = 2 + varpre2octets(modp);

	     /* XXX - VPN - only support two octets */
	     if (genp != NULL)
		  vsize += 2 + varpre2octets(genp);

	     st->scheme = calloc(vsize, sizeof(u_int8_t));
	     if (st->scheme == NULL) {
                  state_value_reset(st); 
                  return -1; 
             } 
             bcopy(header->scheme, st->scheme, 2);
	     if (genp != NULL) {
		  st->scheme[2] = (vsize-4) >> 8;
		  st->scheme[3] = (vsize-4) & 0xFF;
		  bcopy(genp, st->scheme+2+2, varpre2octets(genp));
	     }
	     bcopy(modp, st->scheme + 2 + (genp == NULL ? 0 : 2 + varpre2octets(genp)),
		   varpre2octets(modp));;
		   
             st->schemesize = vsize;

#ifdef DEBUG
	     {
		  int i = BUFFER_SIZE;
		  bin2hex(buffer, &i, VALUE_REQUEST_VALUE(header), varpre2octets(VALUE_REQUEST_VALUE(header)));
		  printf("Got exchange value 0x%s\n", buffer);
	     }
#endif

	     /* Set exchange value */
	     st->texchangesize = varpre2octets(VALUE_REQUEST_VALUE(header));
	     st->texchange = calloc(st->texchangesize, sizeof(u_int8_t));
	     if (st->texchange == NULL) {
		  log_error(1, "calloc() in handle_value_request()");
		  return -1;
	     }
	     bcopy(VALUE_REQUEST_VALUE(header), st->texchange, st->texchangesize);

             strncpy(st->address, address, 15);  
             st->port = port;  
	     st->counter = header->counter;
             bcopy(header->icookie, st->icookie, COOKIE_SIZE);  
             bcopy(header->rcookie, st->rcookie, COOKIE_SIZE);  

	     if ((st->roschemes = calloc(ssize, sizeof(u_int8_t))) == NULL) {
		  state_value_reset(st);
		  return -1;
	     }
	     bcopy(schemes, st->roschemes, ssize);
	     st->roschemesize = ssize;

	     if (pick_attrib(st, &(st->oSPIoattrib), 
			     &(st->oSPIoattribsize)) == -1) {
		  state_value_reset(st);
		  return -1;
	     }

	     st->lifetime = exchange_timeout + time(NULL);

	     /* Now put the filled state object in the chain */
	     state_insert(st);
	}
	     
	packet_size = PACKET_BUFFER_SIZE;
	if (photuris_value_response(st, packet_buffer, &packet_size) == -1)
	     return -1;

	send_packet();

        /* Compute the shared secret now */
        compute_shared_secret(st, &(st->shared), &(st->sharedsize));
#ifdef DEBUG   
	{
	     int i = BUFFER_SIZE;
	     bin2hex(buffer, &i, st->shared, st->sharedsize);
	     printf("Shared secret is: 0x%s\n", buffer);   
	}
#endif   

	st->retries = 0;
	st->phase = VALUE_RESPONSE;
	return 0;
}
