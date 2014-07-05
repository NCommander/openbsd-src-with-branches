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
 * $OpenBSD: os-openbsd.h,v 1.15 2014/07/05 06:18:58 guenther Exp $
 */

/*
 * OpenBSD os-*.h file
 */

/*
 * NOTE: OpenBSD uses 64-bit file size semantics, and so you
 * must be careful when using varargs-type functions
 * like the *printf family when printing elements which
 * might be 64-bits (such as stat->st_size from stat.h).
 */

/*
 * Set process args to messages that show up when running ps(1)
 *
 * OpenBSD has setproctitle() in libc so we don't want to use rdist's.
 */
#define HAVE_SETPROCTITLE

/*
 * Define the type of directory routines your system has.
 */
#define DIR_TYPE	DIR_DIRENT

/*
 * Determine what routines we have to get filesystem info.
 */
#define FSI_TYPE	FSI_GETFSSTAT

/*
 * Select the type of statfs() system call (if any).
 */
#define STATFS_TYPE	STATFS_44BSD

/*
 * Use f_fstypename in struct statfs.
 */
#define HAVE_FSTYPENAME	1

/*
 * UID argument type for chown()
 */
typedef uid_t CHOWN_UID_T;

/*
 * GID argument type for chown()
 */
typedef gid_t CHOWN_GID_T;

/*
 * Our types, usually these are uid_t and gid_t.
 */
typedef uid_t UID_T;	/* Must be signed */
typedef gid_t GID_T;	/* Must be signed */

/*
 * Generic pointer, used by memcpy, malloc, etc.  Usually char or void.
 */
typedef void POINTER;

/*
 * Type of set file time function available
 */
#define SETFTIME_TYPE	SETFTIME_UTIMES

/*
 * Things we have
 */
#define HAVE_FCHOWN			/* Have fchown() */
#define HAVE_LCHOWN			/* Have lchown() */
#define HAVE_FCHMOD			/* Have fchmod() */
#define HAVE_FTRUNCATE			/* Have ftruncate() */
#define HAVE_SAVED_IDS			/* Have POSIX style saved [ug]id's */
#define HAVE_VIS			/* Have vis() */
#define HAVE_VSNPRINTF			/* Have vsnprintf() */
#define POSIX_SIGNALS			/* Have POSIX signals */
#define HAVE_PATHS_H			/* Have <paths.h> */

/*
 * Path to old-style rdist command
 */
#define _PATH_OLDRDIST	"/usr/bin/oldrdist"

/*
 * Path to remote shell command
 */
#define _PATH_REMSH	"/usr/bin/ssh"

/*
 * Use the system <paths.h>
 */
#define PATHS_H		<paths.h>
