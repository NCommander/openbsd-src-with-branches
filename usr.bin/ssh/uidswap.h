/*	$OpenBSD: uidswap.h,v 1.8 2001/06/26 06:33:06 itojun Exp $	*/

/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 */

#ifndef UIDSWAP_H
#define UIDSWAP_H

void	 temporarily_use_uid(struct passwd *);
void	 restore_uid(void);
void	 permanently_set_uid(struct passwd *);

#endif				/* UIDSWAP_H */
