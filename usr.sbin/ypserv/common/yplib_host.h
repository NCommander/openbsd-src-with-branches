/*	$NetBSD: yplib_host.h,v 1.6 1994/10/26 00:57:11 cgd Exp $	*/

/*
 * Copyright (c) 1992, 1993 Theo de Raadt <deraadt@fsa.ca>
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
 *	This product includes software developed by Theo de Raadt.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _YPLIB_HOST_H_
#define _YPLIB_HOST_H_

int	yp_match_host 	__P((CLIENT *client, char *indomain, char *inmap,
			    const char *inkey, int inkeylen, char **outval,
			    int *outvallen));
int	yp_first_host	__P((CLIENT *client, char *indomain, char *inmap,
			    char **outkey, int *outkeylen, char **outval,
			    int *outvallen));
int	yp_next_host	__P((CLIENT *client, char *indomain, char *inmap,
			    char *inkey, int inkeylen, char **outkey,
			    int *outkeylen, char **outval, int *outvallen));
int	yp_master_host	__P((CLIENT *client,
			    char *indomain, char *inmap, char **outname));
int	yp_order_host	__P((CLIENT *client,
			    char *indomain, char *inmap, int *outorder));
int	yp_all_host	__P((CLIENT *client, char *indomain, char *inmap,
			    struct ypall_callback *incallback));
int	yp_maplist_host	__P((CLIENT *client, char *indomain,
			    struct ypmaplist **outmaplist));
CLIENT *yp_bind_local	__P((u_long program, u_long version));
CLIENT *yp_bind_host	__P((char *server, u_long program, u_long version,
			    u_short port, int usetcp));

#endif /* _YPLIB_HOST_H_ */

