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
 * handle_spi_update:
 * receive a SPI_UPDATE packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: handle_spi_update.c,v 1.2 1997/06/12 17:09:20 provos Exp provos $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "cookie.h"
#include "buffer.h"
#include "packet.h"
#include "encrypt.h"
#include "validity.h"
#include "secrets.h"
#include "schedule.h"
#include "scheme.h"
#include "errlog.h"
#include "spi.h"
#ifdef IPSEC
#include "kernel.h"
#endif

int
handle_spi_update(u_char *packet, int size, char *address, 
			char *local_address)
{
	struct spi_update *header;
	struct stateob *st;
	struct spiob *spi;
	time_t lifetime;
	u_int8_t *p, *attributes;
	u_int16_t i, asize, attribsize, tmp;
	u_int8_t signature[22];  /* XXX - constant */

	if (size < SPI_UPDATE_MIN)
	     return -1;	/* packet too small  */

	header = (struct spi_update *) packet;

	st = state_find_cookies(address, header->icookie, header->rcookie);
	if (st == NULL) {
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, BAD_COOKIE);
	     send_packet();
	     return 0;
	}

	if (st->phase != SPI_UPDATE && st->phase != SPI_NEEDED)
	     return 0;     /* We don't want this packet */

	/* Decrypt message */
	tmp = size - SPI_UPDATE_MIN;
	if (packet_decrypt(st, SPI_UPDATE_VERIFICATION(header), &tmp) == -1) {
	     log_error(0, "packet_decrypt() in handle_spi_update()");
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, VERIFICATION_FAILURE);
	     send_packet();
	     return -1;
	}

	/* Verify message */
	if (!(i = get_validity_verification_size(st))) {
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, VERIFICATION_FAILURE);
	     send_packet();

	     return -1;
	}
	
	asize = SPI_UPDATE_MIN + i;

	p = SPI_UPDATE_VERIFICATION(header);

	attributes = p + i;
	asize += packet[size-1];           /* Padding size */
	attribsize = 0;
	while(asize + attribsize < size)
	     attribsize += attributes[attribsize+1] + 2;

	asize += attribsize;

	if (asize != size) {
	     log_error(0, "wrong packet size in handle_spi_update()");
	     return -1;
	}

	if (i > sizeof(signature)) {
	     log_error(0, "verification too long in handle_spi_update()");
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, VERIFICATION_FAILURE);
	     send_packet();
	     return -1;
	}

	bcopy(p, signature, i);
	bzero(p, i);

	if (!verify_validity_verification(st, signature, packet, size)) {
	     log_error(0, "verification failed in handle_spi_update()");
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, VERIFICATION_FAILURE);
	     send_packet();
	     return 0;
	}

	lifetime = (header->lifetime[0]<<16) + (header->lifetime[1]<<8) +
	     header->lifetime[2];

	if (lifetime == 0) {
	     /* Delete specified security association */
	     if ((spi = spi_find(st->address, header->SPI)) == NULL) {
		  log_error(0, "spi_find() in handle_spi_update()");
		  return -1;
	     }
#ifdef IPSEC
	     kernel_unlink_spi(spi);
#endif
	     spi_unlink(spi);
	     spi_value_reset(spi);
	     return 0;
	} 

	if ((spi = spi_new(st->address, header->SPI)) == NULL) {
	     log_error(0, "spi_new() in handle_spi_update()"); 
	     return -1; 
	}
	if ((spi->local_address = strdup(local_address)) == NULL) {
	     log_error(1, "strdup() in handle_spi_update()");
	     return -1;
	}
	if((spi->attributes = calloc(attribsize, sizeof(u_int8_t))) == NULL) {
	     log_error(1, "calloc() in handle_spi_update()");
	     return -1;
	}
	bcopy(attributes, spi->attributes, attribsize);
	spi->attribsize = attribsize;
	bcopy(st->icookie, spi->icookie, COOKIE_SIZE);
	spi->lifetime = time(NULL) + lifetime;

	make_session_keys(st, spi);

	spi_insert(spi);
#ifdef IPSEC
	kernel_insert_spi(spi);
#endif
	return 0;
}
