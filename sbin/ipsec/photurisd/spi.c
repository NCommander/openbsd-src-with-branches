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
 * spi.c:
 * SPI handling functions
 */

#ifndef lint
static char rcsid[] = "$Id: spi.c,v 1.3 1997/06/12 17:09:20 provos Exp provos $";
#endif

#define _SPI_C_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include "config.h"
#include "photuris.h"
#include "state.h"
#include "attributes.h"
#include "buffer.h"
#include "spi.h"
#include "schedule.h"
#include "errlog.h"
#ifdef IPSEC
#include "kernel.h"
#endif


static struct spiob *spiob = NULL;

int
isinattrib(u_int8_t *attributes, u_int16_t attribsize, u_int8_t attribute)
{
     while(attribsize>0) {
	  if(*attributes==attribute)
	       return 1;
	  if(attribsize - (*(attributes+1)+2) > attribsize) 
	       return 0;

	  attribsize -= *(attributes+1)+2;
	  attributes += *(attributes+1)+2;
     }
     return 0;
}

time_t
getspilifetime(struct stateob *st)
{
     /* XXX - destination depend lifetimes */
     return spi_lifetime;
}

int
make_spi(struct stateob *st, char *local_address,
	 u_int8_t *SPI, time_t *lifetime,
	 u_int8_t **attributes, u_int16_t *attribsize)
{
     u_int32_t tmp = 0;
     u_int16_t i;

     if(*attributes == NULL) {           /* We are in need of attributes */
	  u_int16_t count = 0;
	  u_int8_t *wanted, *offered, *p;
	  u_int16_t wantedsize, offeredsize;
	  u_int16_t mode = 0;            /* We only take when in ah|esp mode */
	  int first = 0;                 /* Obmit AH|ESP header if not needed*/
	  struct attribute_list *ob; 
      
	  if ((ob = attrib_find(NULL)) == NULL) { 
	       log_error(0, "attrib_find() for default in make_spi() in "
			 "exchange to %s", st->address); 
	       return -1; 
	  } 

	  /* Take from Owner */
	  wanted = ob->attributes;
	  wantedsize = ob->attribsize;

	  /* Take from User */
	  offered = st->uSPIoattrib;
	  offeredsize = st->uSPIoattribsize;
	  
	  /* This should never happen */
	  if(wantedsize>BUFFER_SIZE)
	       return -1;

	  p = buffer;
	  while(wantedsize>0) {
	       /* Scan the offered attributes */
	       if (*wanted == AT_AH_ATTRIB && 
		   (st->flags & IPSEC_OPT_AUTH)) {
		    first = 1;
		    mode = AT_AH_ATTRIB;
	       } else if (*wanted == AT_ESP_ATTRIB &&
			  (st->flags & IPSEC_OPT_ENC)) {
		    mode = AT_ESP_ATTRIB;
		    first = 1;
	       }
	       
	       /* 
		* Take attributes only from AH or ESP sections.
		* Obmit AH or ESP header when there are no entries
		* in that section.
		* XXX - put && first && in if to take only one attrib
		* in each section.
		*/

	       if (mode && first &&
		   *wanted != AT_AH_ATTRIB && *wanted != AT_ESP_ATTRIB &&
		   isinattrib(offered, offeredsize, *wanted)) {
		    
		    /* Put prober header in there */
		    if (first) {
			 p[0] = mode;
			 p[1] = 0;
			 first = 0;
			 count += 2;
			 p += 2;
		    }
		    /* We are using our own attributes, safe to proceed */
		    bcopy(wanted, p, *(wanted+1) + 2);
		    count += *(wanted+1) + 2;
		    p += *(wanted+1) + 2;
	       }
	       if(wantedsize - *(wanted+1) - 2 > wantedsize)
		    break;
	       wantedsize -= *(wanted+1) + 2;
	       wanted += *(wanted+1) + 2;
	  }
	  if((*attributes=calloc(count,sizeof(u_int8_t))) == NULL) {
	       log_error(1, "Out of memory for SPI attributes (%d)", count);
	       return -1;
	  }
	  *attribsize = count;
	  bcopy(buffer, *attributes, count);
     }
	
     /* Just grab a random number, this should be uniq */
     for(i=0; i<SPI_SIZE; i++) {
	  if(i%4 == 0)
#ifdef IPSEC
	       tmp = kernel_reserve_spi(local_address);
#else
	       tmp = arc4random();
#endif
	  SPI[i] = tmp & 0xFF;
	  tmp = tmp >> 8;
     }
	  
     *lifetime = getspilifetime(st) + (arc4random() & 0x1F);

     return 0;
}


int
spi_insert(struct spiob *ob)
{
     struct spiob *tmp;

     ob->next = NULL;

     if(spiob == NULL) {
	  spiob = ob;
	  return 1;
     }
     
     tmp=spiob;
     while(tmp->next!=NULL)
	  tmp = tmp->next;

     tmp->next = ob;
     return 1;
}

int
spi_unlink(struct spiob *ob)
{
     struct spiob *tmp;
     if(spiob == ob) {
	  spiob = ob->next;
	  free(ob);
	  return 1;
     }

     for(tmp=spiob; tmp!=NULL; tmp=tmp->next) {
	  if(tmp->next==ob) {
	       tmp->next=ob->next;
	       free(ob);
	       return 1;
	  }
     }
     return 0;
}

struct spiob *
spi_new(char *address, u_int8_t *spi)
{
     struct spiob *p;
     if (spi_find(address, spi) != NULL)
	  return NULL;
     if ((p = calloc(1, sizeof(struct spiob))) == NULL)
	  return NULL;

     if ((p->address = strdup(address)) == NULL) {
	  free(p);
	  return NULL;
     }
     bcopy(spi, p->SPI, SPI_SIZE);
     
     return p;
}

int
spi_value_reset(struct spiob *ob)
{ 
     if (ob->address != NULL)
	  free(ob->address);
     if (ob->local_address != NULL)
	  free(ob->local_address);
     if (ob->attributes != NULL)
	  free(ob->attributes);
     if (ob->sessionkey != NULL)
	  free(ob->sessionkey);

     return 1;
}


struct spiob * 
spi_find_attrib(char *address, u_int8_t *attrib, u_int16_t attribsize) 
{ 
     struct spiob *tmp = spiob; 
     u_int16_t i;

     while(tmp!=NULL) { 
          if(!strcmp(address, tmp->address)) {
	       for(i=0;i<attribsize; i += attrib[i+1]+2) {
		    if (attrib[i] == AT_AH_ATTRIB || attrib[i] == AT_ESP_ATTRIB)
			 continue;
		    if (!isinattrib(tmp->attributes, tmp->attribsize, attrib[i]))
			 break;
	       }
	       if (i == attribsize)
		    return tmp;
	  }
          tmp = tmp->next; 
     } 
     return NULL; 
} 

/* 
 * find the spi ob with matching address
 * Alas this is tweaked, for owner = 1 compare with local_address
 * and for owner = 0 compare with address.
 */

struct spiob *
spi_find(char *address, u_int8_t *spi)
{
     struct spiob *tmp = spiob;
     while(tmp!=NULL) {
          if ((address == NULL || (tmp->owner ? 
	      !strcmp(address, tmp->local_address) :
	      !strcmp(address, tmp->address))) &&
	     !bcmp(spi, tmp->SPI, SPI_SIZE))
	       return tmp;
	  tmp = tmp->next;
     }
     return NULL;
}

struct spiob *
spi_root(void)
{
     return spiob;
}

void
spi_cleanup()
{
     struct spiob *p;
     struct spiob *tmp = spiob;
     while(tmp!=NULL) {
	  p = tmp;
	  tmp = tmp->next;
	  spi_value_reset(p);
	  free(p);
     }
     spiob = NULL;
}

void
spi_expire(void)
{
     struct spiob *tmp = spiob, *p;
     time_t tm;

     tm = time(NULL);
     while (tmp != NULL) {
	  if (tmp->lifetime == -1 || 
	      tmp->lifetime + (tmp->owner ? CLEANUP_TIMEOUT : 0) > tm) {
	       tmp = tmp->next;
	       continue;
	  }
#ifdef DEBUG
	  {
	       int i = BUFFER_SIZE;
	       bin2hex(buffer, &i, tmp->SPI, 4);
	       printf("Expiring %s spi %s to %s\n", tmp->owner ? "Owner" : "User",
		      buffer, tmp->address);
	  }
#endif
#ifdef IPSEC
	  kernel_unlink_spi(tmp);
#endif
	  p = tmp;
	  tmp = tmp->next;
	  spi_value_reset(p);
	  spi_unlink(p);
     }
}
