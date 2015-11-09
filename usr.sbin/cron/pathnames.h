/*	$OpenBSD: pathnames.h,v 1.18 2015/11/04 12:53:05 millert Exp $	*/

/* Copyright 1993,1994 by Paul Vixie
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

#ifndef _PATHNAMES_H_
#define _PATHNAMES_H_

#include <paths.h>

			/* CRONDIR is where cron(8) and crontab(1) both chdir
			 * to; CRON_SPOOL, CRON_ALLOW, CRON_DENY, and LOG_FILE
			 * are all relative to this directory.
			 */
#define CRONDIR		"/var/cron"

			/* SPOOLDIR is where the crontabs live.
			 * This directory will have its modtime updated
			 * whenever crontab(1) changes a crontab; this is
			 * the signal for cron(8) to look at each individual
			 * crontab file and reload those whose modtimes are
			 * newer than they were last time around (or which
			 * didn't exist last time around...)
			 */
#define CRON_SPOOL	"tabs"

			/* AT_SPOOL is where the at jobs live (relative to
			 * CRONDIR). This directory will have its modtime
			 * updated whenever at(1) changes a crontab; this is
			 * the signal for cron(8) to look for changes in the
			 * jobs directory (new, changed or jobs).
			 */
#define AT_SPOOL	"atjobs"

			/* CRONSOCK is the name of the socket used by at and
			 * crontab to poke cron to re-read the at and cron
			 * spool files while cron is asleep.
			 * It lives in the spool directory.
			 */
#define	CRONSOCK	".sock"

			/* cron allow/deny file.  At least cron.deny must
			 * exist for ordinary users to run crontab.
			 */
#define	CRON_ALLOW	"cron.allow"
#define	CRON_DENY	"cron.deny"

			/* at allow/deny file.  At least at.deny must
			 * exist for ordinary users to run at.
			 */
#define	AT_ALLOW	"at.allow"
#define	AT_DENY		"at.deny"

			/* 4.3BSD-style crontab */
#define SYSCRONTAB	"/etc/crontab"

			/* what editor to use if no EDITOR or VISUAL
			 * environment variable specified.
			 */
#define EDITOR		_PATH_VI

#endif /* _PATHNAMES_H_ */
