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
 * attributes.h:
 * attributes for a security association
 */

#ifndef _ATTRIBUTES_H_
#define _ATTRIBUTES_H_

#undef EXTERN
#ifdef _ATTRIBUTES_C_
#define EXTERN
#else
#define EXTERN extern
#endif

#define AT_PAD        0
#define AT_AH_ATTRIB  1
#define AT_ESP_ATTRIB 2
#define AT_MD5_DP     3
#define AT_SHA1_DP    4
#define AT_MD5_KDP    5
#define AT_SHA1_KDP   6
#define AT_DES_CBC    8
#define AT_ORG        255

#define MD5_KEYLEN    384
#define DES_KEYLEN    64

/* XXX - Only for the moment */
#define DH_G_2_MD5          2
#define DH_G_3_MD5          3
#define DH_G_2_DES_MD5      4    
#define DH_G_5_MD5          5
#define DH_G_3_DES_MD5      6
#define DH_G_VAR_MD5        7
#define DH_G_2_3DES_SHA1    8
#define DH_G_5_DES_MD5      10
#define DH_G_3_3DES_SHA1    12
#define DH_G_VAR_DES_MD5    14
#define DH_G_5_3DES_SHA1    20
#define DH_G_VAR_3DES_SHA1  28

struct attribute_list {
     struct attribute_list *next;
     char *address;
     in_addr_t netmask;
     u_int8_t *attributes;
     u_int16_t attribsize;
};

EXTERN int isinattrib(u_int8_t *attributes, u_int16_t attribsize, 
		      u_int8_t attribute);
EXTERN int isattribsubset(u_int8_t *set, u_int16_t setsize, 
			  u_int8_t *subset, u_int16_t subsetsize);
EXTERN struct attribute_list *attrib_new(void);
EXTERN int attrib_insert(struct attribute_list *);
EXTERN int attrib_unlink(struct attribute_list *);
EXTERN int attrib_value_reset(struct attribute_list *);
EXTERN struct attribute_list *attrib_find(char *);
EXTERN void attrib_cleanup(void);

#endif /* ATTRIBUTES_H */
