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
 * photuris_error_message:
 * create a ERROR_MESSAGE packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: photuris_error_message.c,v 1.2 1997/09/02 17:26:45 provos Exp $";
#endif

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "cookie.h"

int
photuris_error_message(struct stateob *st, u_char *buffer, int *size,
		       char *icookie, char *rcookie, u_int8_t counter,
		       u_int8_t error_type)
{
	struct error_message *header;

	if (*size < ERROR_MESSAGE_PACKET_SIZE + 
	    (error_type == RESOURCE_LIMIT ? 1 : 0))
	  return -1;	/* buffer not large enough */

	header = (struct error_message *) buffer;
	*size = ERROR_MESSAGE_PACKET_SIZE + 
	     (error_type == RESOURCE_LIMIT ? 1 : 0);
	
	bcopy(icookie, header->icookie, COOKIE_SIZE);
	bcopy(rcookie, header->rcookie, COOKIE_SIZE);

	header->type = error_type;
	
	if (error_type == RESOURCE_LIMIT) {
	     int i;
	     buffer[ERROR_MESSAGE_PACKET_SIZE] = counter;
	     
	     for(i = 0; i<COOKIE_SIZE; i++)
		  if (rcookie[i] != 0)
		       break;

	     if (i != COOKIE_SIZE || counter != 0)
		  return 0;

	     if (st != NULL) {
		  bcopy(st->rcookie, header->rcookie, COOKIE_SIZE);
		  buffer[ERROR_MESSAGE_PACKET_SIZE] = st->counter;
	     }
	}

	return 0;
}
