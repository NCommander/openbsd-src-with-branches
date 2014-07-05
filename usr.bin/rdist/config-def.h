/*	$OpenBSD: config-def.h,v 1.8 2014/07/05 06:18:58 guenther Exp $	*/

/*
 * Copyright (c) 1993 Michael A. Cooper
 * Copyright (c) 1993 Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $From: config-def.h,v 1.1.1.1 1995/05/20 23:35:17 christos Exp $
 * @(#)configdef.h
 */

#ifndef __configdef_h__
#define __configdef_h__

/*
 * Configuration definetions
 */

/*
 * Types of directory routines
 */
#define DIR_DIRECT		1
#define DIR_DIRENT		2

/*
 * Types of filesystem info routines
 */
#define FSI_GETFSSTAT		1
#define FSI_GETMNT		2
#define FSI_MNTCTL		3
#define FSI_GETMNTENT		4

/*
 * Types of set filetime functions
 */
#define SETFTIME_UTIMES		1		/* Have utimes() */
#define SETFTIME_UTIME		2		/* Have utime() */

/*
 * Types of statfs() calls
 */
#define STATFS_BSD		1
#define STATFS_SYSV		2
#define STATFS_OSF1		3

#endif	/* __configdef_h__ */
