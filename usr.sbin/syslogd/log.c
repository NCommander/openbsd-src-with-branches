/*	$OpenBSD$	*/

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2017 Alexander Bluhm <bluhm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include "log.h"
#include "syslogd.h"

static int		 debug;
static int		 verbose;
static int		 facility;
static const char	*log_procname;

void
log_init(int n_debug, int fac)
{
	extern char	*__progname;

	debug = n_debug;
	verbose = n_debug;
	facility = fac;
	log_procinit(__progname);

	tzset();
}

void
log_procinit(const char *procname)
{
	if (procname != NULL)
		log_procname = procname;
}

void
log_setdebug(int d)
{
	debug = d;
}

int
log_getdebug(void)
{
	return (debug);
}

void
log_setverbose(int v)
{
	verbose = v;
}

int
log_getverbose(void)
{
	return (verbose);
}

void
logit(int pri, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vlog(pri, fmt, ap);
	va_end(ap);
}

void
vlog(int pri, const char *fmt, va_list ap)
{
	char	 ebuf[ERRBUFSIZE];
	size_t	 l;
	int	 saved_errno = errno;

	if (debug) {
		l = snprintf(ebuf, sizeof(ebuf), "%s: ", log_procname);
		if (l < sizeof(ebuf))
			vsnprintf(ebuf+l, sizeof(ebuf)-l, fmt, ap);
		fprintf(stderr, "%s\n", ebuf);
		fflush(stderr);
	} else
		vlogmsg(pri, log_procname, fmt, ap);

	errno = saved_errno;
}

void
log_warn(const char *emsg, ...)
{
	char	 ebuf[ERRBUFSIZE];
	size_t	 l;
	va_list	 ap;
	int	 saved_errno = errno;

	/* best effort to even work in out of memory situations */
	if (emsg == NULL)
		logit(LOG_ERR, "%s", strerror(saved_errno));
	else {
		va_start(ap, emsg);
		l = vsnprintf(ebuf, sizeof(ebuf), emsg, ap);
		if (l < sizeof(ebuf))
			snprintf(ebuf+l, sizeof(ebuf)-l, ": %s",
			    strerror(saved_errno));
		logit(LOG_ERR, "%s", ebuf);
		va_end(ap);
	}
	errno = saved_errno;
}

void
log_warnx(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_ERR, emsg, ap);
	va_end(ap);
}

void
log_info(int pri, const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(pri, emsg, ap);
	va_end(ap);
}

void
log_debug(const char *emsg, ...)
{
	char	 ebuf[ERRBUFSIZE];
	va_list	 ap;
	int	 saved_errno;

	if (verbose) {
		saved_errno = errno;
		va_start(ap, emsg);
		vsnprintf(ebuf, sizeof(ebuf), emsg, ap);
		fprintf(stderr, "%s\n", ebuf);
		fflush(stderr);
		va_end(ap);
		errno = saved_errno;
	}
}

static void
vfatalc(int error, const char *emsg, va_list ap)
{
	char		 ebuf[ERRBUFSIZE];
	const char	*sep;

	if (emsg != NULL) {
		(void)vsnprintf(ebuf, sizeof(ebuf), emsg, ap);
		sep = ": ";
	} else {
		ebuf[0] = '\0';
		sep = "";
	}
	if (error)
		logit(LOG_CRIT, "fatal in %s: %s%s%s",
		    log_procname, ebuf, sep, strerror(error));
	else
		logit(LOG_CRIT, "fatal in %s%s%s", log_procname, sep, ebuf);
}

void
fatal(const char *emsg, ...)
{
	va_list	ap;

	va_start(ap, emsg);
	vfatalc(errno, emsg, ap);
	va_end(ap);
	die(0);
}

void
fatalx(const char *emsg, ...)
{
	va_list	ap;

	va_start(ap, emsg);
	vfatalc(0, emsg, ap);
	va_end(ap);
	die(0);
}
