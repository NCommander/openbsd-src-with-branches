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
 * identity.c:
 * handling identity choices and creation of the before mentioned.
 */

#ifndef lint
static char rcsid[] = "$Id: identity.c,v 1.1.1.1 1997/07/18 22:48:49 provos Exp $";
#endif

#define _IDENTITY_C_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <md5.h>
#include <gmp.h>
#include <sha1.h>
#include "config.h"
#include "photuris.h"
#include "state.h"
#include "attributes.h"
#include "modulus.h"
#include "exchange.h"
#include "identity.h"
#include "buffer.h"
#include "scheme.h"
#include "errlog.h"

#ifdef NEED_STRSEP
#include "strsep.h"
#endif

static struct identity *idob = NULL;

int
init_identities(char *name, struct identity *root)
{
     FILE *fp;
     char *p, *p2, *file = secret_file;
     struct identity *tmp, **ob;
     struct passwd *pwd;
     struct stat sb;
     int type;

     if (name != NULL) {
	  ob = (struct identity **)&root->object;
	  file = name;
     } else
	  ob = &idob;

     if (lstat(file, &sb) == -1) {
	  log_error(1, "lstat() on %s in init_identities()", file);
	  return -1;
     }
     if (((sb.st_mode & S_IFMT) & ~S_IFREG)) {
	  log_error(0, "no regular file %s in init_identities()", file);
	  return -1;
     }
     fp = fopen(file, "r");
     if (fp == (FILE *) NULL)
     {
	  log_error(1, "no hash secrets file %s", file);
	  return -1;
     }

#ifdef DEBUG
     if (name == NULL)
	  printf("[Reading identities + secrets]\n");
#endif

     while(fgets(buffer, BUFFER_SIZE,fp)) {
	  p=buffer;
	  while(isspace(*p))  /* Get rid of leading spaces */
	       p++;
	  if(*p == '#')       /* Ignore comments */
	       continue;
	  if(!strlen(p))
	       continue;

	  if (!strncmp(p, IDENT_LOCAL, strlen(IDENT_LOCAL))) {
	       type = ID_LOCAL;
	       p += strlen(IDENT_LOCAL);
	  } else if (!strncmp(p, IDENT_LOCALPAIR, strlen(IDENT_LOCALPAIR))) {
	       type = ID_LOCALPAIR;
	       p += strlen(IDENT_LOCALPAIR);
	  } else if (!strncmp(p, IDENT_REMOTE, strlen(IDENT_REMOTE))) {
	       type = ID_REMOTE;
	       p += strlen(IDENT_REMOTE);
	  } else if (!strncmp(p, IDENT_LOOKUP, strlen(IDENT_LOOKUP))) {
	       type = ID_LOOKUP;
	       p += strlen(IDENT_LOOKUP);
	  } else {
	       log_error(0, "Unkown tag %s in %s", p, file);
	       continue;
	  }
		
	  if ((tmp = identity_new()) == NULL) {
	       log_error(0, "identity_new() in init_identities()");
	       continue;
	  }

	  p2 = p;
	  if (!isspace(*p2))
	       continue;
	  
	  /* Tokens are braced with "token" */
	  if((p=strsep(&p2, "\"\'")) == NULL || 
	     (p=strsep(&p2, "\"\'")) == NULL)
	       continue;

	  tmp->type = type;
	  tmp->tag = strdup(p);
	  tmp->root = root;

	  switch(type) {
	  case ID_LOCAL:
	  case ID_REMOTE:
	       if (type == ID_REMOTE) {
		    /* Search for duplicates */
		    if (identity_find(idob, tmp->tag, ID_REMOTE) != NULL) {
			 log_error(0, "Duplicate id \"%s\" found in %s",
				   tmp->tag, name != NULL ? name : "root");
			 identity_value_reset(tmp);
			 continue;
		    }
	       }
	       /* Tokens are braced with "token" */
	       if((p=strsep(&p2, "\"\'")) == NULL || 
		  (p=strsep(&p2, "\"\'")) == NULL) {
		    identity_value_reset(tmp);
		    continue;
	       }
	       tmp->object = strdup(p);
	       break;
	  case ID_LOCALPAIR:
	       /* Tokens are braced with "token" */
	       if((p=strsep(&p2, "\"\'")) == NULL || 
		  (p=strsep(&p2, "\"\'")) == NULL) {
		    identity_value_reset(tmp);
		    continue;
	       }
	       tmp->pairid = strdup(p);
	       /* Tokens are braced with "token" */
	       if((p=strsep(&p2, "\"\'")) == NULL || 
		  (p=strsep(&p2, "\"\'")) == NULL) {
		    identity_value_reset(tmp);
		    continue;
	       }
	       tmp->object = strdup(p);
	       break;
	  case ID_LOOKUP:
	       if (name != NULL) {
		    log_error(0, "lookup in user file %s in init_identities()",
			      name);
		    continue;
	       }
	       while(isspace(*p2)) p2++;

	       while(isspace(p2[strlen(p2)-1]))
		    p2[strlen(p2)-1] = 0;

	       if ((pwd = getpwnam(p2)) == NULL) {
		    log_error(1, "getpwnam() in init_identities()");
		    identity_value_reset(tmp);
		    continue;
	       } else {
		    char *dir = calloc(strlen(PHOTURIS_USER_SECRET)+
				       strlen(pwd->pw_dir) + 2,
				       sizeof(char));

		    /* This is the user name */
		    tmp->pairid = strdup(p2);

		    if (dir == NULL) {
			 log_error(1, "calloc() in init_identities()");
			 identity_value_reset(tmp);
			 continue;
		    }
		    sprintf(dir,"%s/%s", pwd->pw_dir, PHOTURIS_USER_SECRET);
		    if (init_identities(dir, (struct identity *)tmp) == -1) {
			 free(dir);
			 identity_value_reset(tmp);
			 continue;
		    }

		    free(dir);
	       }
	       break;
	  }
	  identity_insert(ob, tmp);
     }
     fclose(fp);

     return 0;
}

/* 
 * Get shared symmetric keys and identity, put the values in
 * the state object. If a SPI User ident is given, we look up
 * the matching remote secret.
 */

int
get_secrets(struct stateob *st, int mode)
{
     u_int8_t local_ident[MAX_IDENT];
     u_int8_t local_secret[MAX_IDENT_SECRET];
     u_int8_t remote_secret[MAX_IDENT_SECRET]; 

     struct identity *id, *root = idob;

     local_ident[0] = '\0';
     local_secret[0] = '\0';
     remote_secret[0] = '\0';

     /* 
      * Remote secret first, if we find the remote secret in 
      * a user secret file, we restrict our local searches
      * to that tree.
      */

     if(st->uSPIident != NULL && st->uSPIsecret == NULL && 
	(mode & ID_REMOTE)) {
	  int skip;

	  if (st->uSPIident[0] == 255 && st->uSPIident[1] == 255) 
	       skip = 8; 
	  else if (st->uSPIident[0] == 255) 
	       skip = 4; 
	  else 
	       skip = 2; 

	  id = identity_find(root, st->uSPIident+skip, ID_REMOTE);
	  if (id != NULL) {
               strncpy(remote_secret, id->object, MAX_IDENT_SECRET-1); 
               remote_secret[MAX_IDENT_SECRET-1] = '\0';  
 
	       if (id->root)
		    root = (struct identity *)id->root->object;
	  }
     }
     
     if (st->user != NULL && 
	 (id = identity_find(idob, st->user, ID_LOOKUP)) != NULL) {
	  /* User keying */
	  id = identity_find((struct identity *)id->object, NULL, ID_LOCAL);
     } else
	  id = NULL;

     if (id == NULL) {
	  /* Host keying */
	  id = identity_find(root, NULL, ID_LOCAL);
     }

      if (id != NULL && (mode & (ID_LOCAL|ID_LOCALPAIR))) {
	  /* Namespace: root->tag + user->tag */
	  if (id->root) {
	       strncpy(local_ident, id->root->tag, MAX_IDENT-1);
	       local_ident[MAX_IDENT-1] = '\0';
	  }
	  strncpy(local_ident+strlen(local_ident), id->tag, 
		  MAX_IDENT-1-strlen(local_ident));
	  local_ident[MAX_IDENT_SECRET-1] = '\0'; 

	  strncpy(local_secret, id->object, MAX_IDENT_SECRET-1);
	  local_secret[MAX_IDENT_SECRET-1] = '\0'; 
     }
     if (st->uSPIident != NULL && st->oSPIident == NULL && 
	 (mode & (ID_LOCAL|ID_LOCALPAIR))) {
	  int skip;
	  if (st->uSPIident[0] == 255 && st->uSPIident[1] == 255)
	       skip = 8;
	  else if (st->uSPIident[0] == 255)
	       skip = 4;
	  else
	       skip = 2;

	  id = identity_find(root, st->uSPIident+skip, ID_LOCALPAIR);
	  if (id != NULL) {
	       local_ident[0] = '\0';
	       /* Namespace: root->tag + user->tag */
	       if (id->root) {
		    strncpy(local_ident, id->root->tag, MAX_IDENT-1);
		    local_ident[MAX_IDENT-1] = '\0';
	       }
	       strncpy(local_ident+strlen(local_ident), id->pairid, 
		       MAX_IDENT-1-strlen(local_ident));
               local_ident[MAX_IDENT-1] = '\0'; 
 
               strncpy(local_secret, id->object, MAX_IDENT_SECRET-1); 
               local_secret[MAX_IDENT_SECRET-1] = '\0';  
	  }
     }
	  
     if((strlen(remote_secret) == 0 && (mode & ID_REMOTE)) ||
	(strlen(local_ident) == 0 && (mode & (ID_LOCAL|ID_LOCALPAIR))) ) {
	  log_error(0, "Can't find identities or secrets in get_secrets()");
	  return -1;
     }

     if(st->oSPIident == NULL && (mode & (ID_LOCAL|ID_LOCALPAIR))) {
	  st->oSPIident = calloc(2+strlen(local_ident)+1,sizeof(u_int8_t));
	  if(st->oSPIident == NULL)
	       return -1; 
	  strcpy(st->oSPIident+2,local_ident);
	  st->oSPIident[0] = ((strlen(local_ident)+1) >> 5) & 0xFF;
	  st->oSPIident[1] = ((strlen(local_ident)+1) << 3) & 0xFF;

	  st->oSPIsecret = calloc(strlen(local_secret)+1,sizeof(u_int8_t));
	  if(st->oSPIsecret == NULL)
	       return -1; 
	  strcpy(st->oSPIsecret,local_secret);
	  st->oSPIsecretsize = strlen(local_secret)+1;
     }
     if(st->uSPIident != NULL && st->uSPIsecret == NULL && 
	(mode & ID_REMOTE)) {
          st->uSPIsecret = calloc(strlen(remote_secret)+1,sizeof(u_int8_t)); 
          if(st->uSPIsecret == NULL) 
               return -1;  
          strcpy(st->uSPIsecret,remote_secret); 
	  st->uSPIsecretsize = strlen(remote_secret)+1;
     } 
     return 0;
}

int
choose_identity(struct stateob *st, u_int8_t *packet, u_int16_t *size,
		 u_int8_t *attributes, u_int16_t attribsize)
{
     u_int16_t rsize, asize, tmp;
     int mode = 0;
     rsize = *size;

     /* XXX - we only have one identity choice at the moment. */
     tmp = 0;
     while(attribsize>0 && !tmp) {
	  switch(*attributes) {
	  case AT_MD5_DP:
	       tmp = 1;
	       break;
	  default:
	       if(attribsize -(*(attributes+1)+2) > attribsize) {
		    attribsize=0;
		    break;
	       }
	       attribsize -= *(attributes+1)+2;
	       attributes += *(attributes+1)+2;
	       break;
	  }	
     }

     if(attribsize == 0) {
	  log_error(0, "No identity choice found in offered attributes "
		    "in choose_identity");
	  return -1;
     }
     
     if(rsize < *(attributes+1)+2)
	  return -1;

     asize = *(attributes+1)+2;
     rsize -= asize;
     bcopy(attributes, packet, asize);

     /* Now put identity in state object */
     if (st->oSPIidentchoice == NULL) {
	  if ((st->oSPIidentchoice = calloc(asize, sizeof(u_int8_t))) == NULL)
	       return -1;
	  bcopy(attributes, st->oSPIidentchoice, asize);
	  st->oSPIidentchoicesize = asize;
     }

     packet += asize;

     /* Chooses identity and secrets for Owner and User */
     if (st->uSPIsecret == NULL && st->uSPIident != NULL)
	  mode |= ID_REMOTE;
     if (st->oSPIsecret == NULL)
	  mode |= ID_LOCAL;
     if(get_secrets(st, mode) == -1)
	  return -1;

     /* oSPIident is varpre already */
     tmp = 2+strlen(st->oSPIident+2)+1;
     if(rsize < tmp)
	  return -1;
     
     bcopy(st->oSPIident, packet, tmp);

     *size = asize + tmp;

     return 0;
}


u_int16_t
get_identity_verification_size(struct stateob *st, u_int8_t *choice)
{
     switch(*choice) {
     case AT_MD5_DP:
	  return (128/8)+2;
     default:
	  log_error(0, "Unknown identity choice: %d\n", *choice);
	  return 0;
     }
}

int
create_identity_verification(struct stateob *st, u_int8_t *buffer, 
			     u_int8_t *packet, u_int16_t size)
{
     int hash_size;
     switch(*(st->oSPIidentchoice)) {
     case AT_MD5_DP:
	  hash_size = MD5idsign(st, buffer+2, packet, size);
	  break;
     default: 
          log_error(0, "Unknown identity choice: %d\n", 
		    *(st->oSPIidentchoice)); 
          return 0; 
     } 
     if(hash_size) {
	  /* Create varpre number from digest */
	  buffer[0] = hash_size >> 5 & 0xFF;
	  buffer[1] = hash_size << 3 & 0xFF;

	  if(st->oSPIidentver != NULL)
	       free(st->oSPIidentver);

	  st->oSPIidentver = calloc(hash_size+2,sizeof(u_int8_t));
	  if(st->oSPIidentver == NULL) {
	       log_error(1, "Not enough memory in create_identity_verification()", 0);
	       return 0;
	  }

	  bcopy(buffer, st->oSPIidentver, hash_size+2);
	  st->oSPIidentversize = hash_size+2;
     }
     return hash_size+2;
}

int 
verify_identity_verification(struct stateob *st, u_int8_t *buffer,  
			     u_int8_t *packet, u_int16_t size) 
{ 
     switch(*(st->uSPIidentchoice)) { 
     case AT_MD5_DP: 
	  if (varpre2octets(buffer) != 18)
	       return 0;
          return MD5idverify(st, buffer+2, packet, size); 
     default:  
          log_error(0, "Unknown identity choice %d in verify_identity_verification()",
		    *(st->uSPIidentchoice));
          return 0;  
     }  
} 


int
MD5idsign(struct stateob *st, u_int8_t *signature,  
                    u_int8_t *packet, u_int16_t psize) 
{
     MD5_CTX ctx;
     struct moduli_cache *mod;
     struct identity_message *p;

     MD5Init(&ctx);

     MD5Update(&ctx, st->shared, st->sharedsize); 

     MD5Update(&ctx, st->icookie, COOKIE_SIZE);
     MD5Update(&ctx, st->rcookie, COOKIE_SIZE);
     MD5Update(&ctx, st->roschemes, st->roschemesize);

     /* Our exchange value */
     mod = mod_find_modgen(st->modulus, st->generator);
     MD5Update(&ctx, mod->exchangevalue, mod->exchangesize); 
     MD5Update(&ctx, st->oSPIoattrib, st->oSPIoattribsize);
     MD5Update(&ctx, st->oSPIident, strlen(st->oSPIident));
     MD5Update(&ctx, st->oSPIsecret, st->oSPIsecretsize);

     /* Their exchange value */
     MD5Update(&ctx, st->texchange, st->texchangesize);
     MD5Update(&ctx, st->uSPIoattrib, st->uSPIoattribsize);

     if(st->uSPIident != NULL) {
	  MD5Update(&ctx, st->uSPIident, strlen(st->uSPIident));
	  MD5Update(&ctx, st->uSPIsecret, st->uSPIsecretsize);
     }

     /* Hash type, lifetime + spi fields */
     p = (struct identity_message *)packet;
     MD5Update(&ctx, (char *)&(p->type), IDENTITY_MESSAGE_MIN - 2*COOKIE_SIZE);

     /* Hash attribute choice, padding */
     packet += IDENTITY_MESSAGE_MIN;
     psize -= IDENTITY_MESSAGE_MIN + packet[1] + 2;
     packet += packet[1] + 2;
     psize -= varpre2octets(packet) + 2 + MD5_SIZE;
     packet += varpre2octets(packet) + 2 + MD5_SIZE;

     MD5Update(&ctx, packet, psize);

     /* Data fill */
     MD5Final(NULL, &ctx);

     /* And finally the trailing key */
     MD5Update(&ctx, st->shared, st->sharedsize);

     MD5Final(signature, &ctx);

     return MD5_SIZE;
}

int
MD5idverify(struct stateob *st, u_int8_t *signature,   
	  u_int8_t *packet, u_int16_t psize)
{
     MD5_CTX ctx; 
     u_int8_t digest[16];
     struct moduli_cache *mod; 
     struct identity_message *p;
 
     p = (struct identity_message *)packet;

     MD5Init(&ctx); 
 
     /* Our shared secret */
     MD5Update(&ctx, st->shared, st->sharedsize); 
 
     MD5Update(&ctx, st->icookie, COOKIE_SIZE); 
     MD5Update(&ctx, st->rcookie, COOKIE_SIZE); 
     MD5Update(&ctx, st->roschemes, st->roschemesize); 
 
     /* Their exchange value */ 
     MD5Update(&ctx, st->texchange, st->texchangesize); 
     MD5Update(&ctx, st->uSPIoattrib, st->uSPIoattribsize); 
     MD5Update(&ctx, st->uSPIident, strlen(st->uSPIident)); 
     MD5Update(&ctx, st->uSPIsecret, st->uSPIsecretsize); 
 
     /* Our exchange value */ 
     mod = mod_find_modgen(st->modulus, st->generator); 
     MD5Update(&ctx, mod->exchangevalue, mod->exchangesize);  
     MD5Update(&ctx, st->oSPIoattrib, st->oSPIoattribsize); 

     /* Determine if the sender knew our secret already */
     if(p->type != IDENTITY_REQUEST) {
	  MD5Update(&ctx, st->oSPIident, strlen(st->oSPIident)); 
	  MD5Update(&ctx, st->oSPIsecret, st->oSPIsecretsize); 
     }
 
     /* Hash type, lifetime + spi fields */
     MD5Update(&ctx, (char *)&(p->type), IDENTITY_MESSAGE_MIN - 2*COOKIE_SIZE);

     packet += IDENTITY_MESSAGE_MIN;
     psize -= IDENTITY_MESSAGE_MIN + packet[1] + 2;
     packet += packet[1] + 2;
     psize -= varpre2octets(packet) + 2 + MD5_SIZE;
     packet += varpre2octets(packet) + 2 + MD5_SIZE;
     MD5Update(&ctx, packet, psize);

     /* Data fill */
     MD5Final(NULL, &ctx); 

     /* And finally the trailing key */
     MD5Update(&ctx, st->shared, st->sharedsize);

     MD5Final(digest, &ctx); 

     return !bcmp(digest, signature, MD5_SIZE);
}

int
identity_insert(struct identity **idob, struct identity *ob)
{
     struct identity *tmp;

     ob->next = NULL;

     if(*idob == NULL) {
	  *idob = ob;
	  return 1;
     }
     
     tmp=*idob;
     while(tmp->next!=NULL)
	  tmp = tmp->next;

     tmp->next = ob;
     return 1;
}

int
identity_unlink(struct identity **idob, struct identity *ob)
{
     struct identity *tmp;
     if(*idob == ob) {
	  *idob = ob->next;
	  free(ob);
	  return 1;
     }

     for(tmp=*idob; tmp!=NULL; tmp=tmp->next) {
	  if(tmp->next==ob) {
	       tmp->next=ob->next;
	       free(ob);
	       return 1;
	  }
     }
     return 0;
}

struct identity *
identity_new(void)
{
     struct identity *p;

     if((p = calloc(1, sizeof(struct identity)))==NULL)
	  return NULL;

     return p;
}

int
identity_value_reset(struct identity *ob)
{ 
     if (ob->tag != NULL)
	  free(ob->tag);
     if (ob->pairid != NULL)
	  free(ob->pairid);
     if (ob->object != NULL)
	  free(ob->object);

     return 1;
}

/* 
 * find the state ob with matching address
 */

struct identity *
identity_root(void)
{
     return idob;
}

/* On ID_LOOKUP match pairid, on ID_LOCAL only match type */

struct identity *
identity_find(struct identity *idob, char *id, int type)
{
     struct identity *tmp = idob, *p;
     while(tmp!=NULL) {
          if(((type == ID_LOCAL && id == NULL) ||
	     (type != ID_LOOKUP && !strcmp(id, tmp->tag)) ||
	     (type == ID_LOOKUP && tmp->pairid != NULL && !strcmp(id, tmp->pairid))) &&
	     type == tmp->type)
		    return tmp;
	  if (tmp->type == ID_LOOKUP && tmp->object != NULL) {
	       p = identity_find((struct identity *)tmp->object, id, type);
	       if (p != NULL)
		    return p;
	  }
	  tmp = tmp->next;
     }
     return NULL;
}

void
identity_cleanup(struct identity **root)
{
     struct identity *p;
     struct identity *tmp;

     if (root == NULL)
	  tmp = idob;
     else
	  tmp = *root;

     while(tmp!=NULL) {
	  if (tmp->type == ID_LOOKUP)
	       identity_cleanup((struct identity **)&tmp->object);
	  p = tmp;
	  tmp = tmp->next;
	  identity_value_reset(p);
	  free(p);
     }

     if (root != NULL)
	  *root = NULL;
     else
	  idob = NULL;
}
