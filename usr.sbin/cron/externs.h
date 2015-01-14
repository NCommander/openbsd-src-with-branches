/*	$OpenBSD: externs.h,v 1.13 2015/01/14 17:27:13 millert Exp $	*/

/* Copyright 1993,1994 by Paul Vixie
 * All rights reserved
 */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* reorder these #include's at your peril */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <bitstring.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <locale.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <utime.h>

#if defined(SYSLOG)
# include <syslog.h>
#endif

#if defined(LOGIN_CAP)
# include <login_cap.h>
#endif /*LOGIN_CAP*/

#if defined(BSD_AUTH)
# include <bsd_auth.h>
#endif /*BSD_AUTH*/

#ifndef TZNAME_ALREADY_DEFINED
extern char *tzname[2];
#endif
#define TZONE(tm) tzname[(tm).tm_isdst]

#ifndef WCOREDUMP
# define WCOREDUMP(st)          (((st) & 0200) != 0)
#endif
