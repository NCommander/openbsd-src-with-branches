/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
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
/* $Id: kernel.h,v 1.7 2000/12/12 01:53:42 provos Exp $ */
/*
 * kernel.h: 
 * security paramter index creation.
 */
 
#ifndef _KERNEL_H_
#define _KERNEL_H_

#undef EXTERN
#ifdef _KERNEL_C_
#define EXTERN

#define ESP_OLD		0x01
#define ESP_NEW		0x02
#define AH_OLD		0x04
#define AH_NEW		0x08

#define XF_ENC		0x10
#define XF_AUTH		0x20

#define XF_SUP		0x80		/* Mark the transforms as supported */

typedef struct {
     int photuris_id;
     int kernel_id, flags;
} transform;

/* 
 * Translation from Photuris Attributes to Kernel Transforms.
 * For the actual ids see: draft-simpson-photuris-*.txt and
 * draft-simpson-photuris-schemes-*.txt
 */

transform xf[] = {
     {  5, SADB_X_AALG_MD5, XF_AUTH|AH_OLD},
     {  6, SADB_X_AALG_SHA1, XF_AUTH|AH_OLD},
     {  5, SADB_AALG_MD5HMAC, XF_AUTH|AH_NEW|ESP_NEW},
     {  6, SADB_AALG_SHA1HMAC, XF_AUTH|AH_NEW|ESP_NEW},
     {  7, SADB_AALG_RIPEMD160HMAC, XF_AUTH|AH_NEW|ESP_NEW},
     {  8, SADB_EALG_DESCBC, XF_ENC|ESP_OLD},
     { 18, SADB_EALG_3DESCBC, XF_ENC|ESP_NEW},
     { 16, SADB_X_EALG_BLF, XF_ENC|ESP_NEW},
     { 17, SADB_X_EALG_CAST, XF_ENC|ESP_NEW},
     { 19, SADB_X_EALG_SKIPJACK, XF_ENC|ESP_NEW},
};

transform *kernel_get_transform(int id);
void kernel_transform_seen(int id, int type);

int kernel_register(int sd);

int kernel_xf_set(int sd, char *buf, int blen, struct iovec *io, int cnt,
		  int len);
int kernel_xf_read(int sd, char *buf, int blen, int seq);

int kernel_ah(attrib_t *ob, struct spiob *SPI, u_int8_t *secrets, int hmac);
int kernel_esp(attrib_t *ob, attrib_t *ob2, struct spiob *SPI, 
	       u_int8_t *secrets);

int kernel_group_spi(char *address, u_int8_t *spi);
int kernel_bind_spis(struct spiob *spi1, struct spiob *spi2);

int kernel_delete_spi(char *address, u_int32_t spi, int proto);

struct sadb_msg;
int kernel_request_sa(struct sadb_msg *);
int kernel_handle_expire(struct sadb_msg *);
#else
#define EXTERN extern
#endif

EXTERN int kernel_known_transform(int id);
EXTERN int kernel_valid(attrib_t *enc, attrib_t *auth);
EXTERN int kernel_valid_auth(attrib_t *auth, u_int8_t *flag, u_int16_t size);

EXTERN u_int32_t kernel_reserve_spi(char *srcaddress, char *dstaddress, 
				    int options);
EXTERN u_int32_t kernel_reserve_single_spi(char *srcaddress, char *dstaddress,
					   u_int32_t spi, int proto);

EXTERN int kernel_insert_spi(struct stateob *st, struct spiob *SPI);
EXTERN int kernel_unlink_spi(struct spiob *ospi);
EXTERN int init_kernel(void);
EXTERN int kernel_get_socket(void);
EXTERN void kernel_set_socket_policy(int sd);
EXTERN void kernel_handle_notify(int sd);
EXTERN void kernel_notify_result(struct stateob *, struct spiob *, int);

#endif /* _KERNEL_H */
