/*	$OpenBSD$	*/

/*
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) Header: extract.h,v 1.12 96/06/20 18:48:42 leres Exp (LBL)
 */

#ifdef LBL_ALIGN
#define EXTRACT_SHORT(p)\
	((u_short)\
		((u_short)*((u_char *)(p)+0)<<8|\
		 (u_short)*((u_char *)(p)+1)<<0))
#define EXTRACT_LONG(p)\
		((u_int32_t)*((u_char *)(p)+0)<<24|\
		 (u_int32_t)*((u_char *)(p)+1)<<16|\
		 (u_int32_t)*((u_char *)(p)+2)<<8|\
		 (u_int32_t)*((u_char *)(p)+3)<<0)
#else
#define EXTRACT_SHORT(p)	((u_short)ntohs(*(u_short *)(p)))
#define EXTRACT_LONG(p)		(ntohl(*(u_int32_t *)(p)))
#endif
