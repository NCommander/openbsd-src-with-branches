/*
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: os.h,v 1.12 1997/11/22 03:37:42 brian Exp $
 *
 *	TODO:
 */

extern char *IfDevName;

extern int OsSetIpaddress(struct in_addr, struct in_addr, struct in_addr);
extern int  OsInterfaceDown(int);
extern int  OpenTunnel(int *);
extern void OsLinkup(void);
extern int  OsLinkIsUp(void);
extern void OsLinkdown(void);
