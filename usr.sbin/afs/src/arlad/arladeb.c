/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      H�gskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <err.h>
#include <parse_units.h>
#include <roken.h>
#include "volcache.h"
#include "ko.h"

#include "arladeb.h"

RCSID("$Id: arladeb.c,v 1.22 2000/08/24 21:44:32 lha Exp $");

Log_method* arla_log_method = NULL;
Log_unit* arla_log_unit = NULL;

#define all (ADEBERROR | ADEBWARN | ADEBDISCONN | ADEBFBUF |		\
	     ADEBMSG | ADEBKERNEL | ADEBCLEANER | ADEBCALLBACK |	\
	     ADEBCM | ADEBVOLCACHE | ADEBFCACHE | ADEBINIT |		\
	     ADEBCONN | ADEBMISC | ADEBVLOG)

#define DEFAULT_LOG (ADEBWARN | ADEBERROR)

struct units arla_deb_units[] = {
    { "all",		all},
    { "almost-all",	all & ~ADEBCLEANER},
    { "errors",		ADEBERROR },
    { "warnings",	ADEBWARN },
    { "disconn",	ADEBDISCONN },
    { "fbuf",		ADEBFBUF },
    { "messages",	ADEBMSG },
    { "kernel",		ADEBKERNEL },
    { "cleaner",	ADEBCLEANER },
    { "callbacks",	ADEBCALLBACK },
    { "cache-manager",	ADEBCM },
    { "volume-cache",	ADEBVOLCACHE },
    { "file-cache",	ADEBFCACHE },
    { "initialization",	ADEBINIT },
    { "connection",	ADEBCONN },
    { "miscellaneous",	ADEBMISC },
    { "venuslog",	ADEBVLOG },
    { "default",	DEFAULT_LOG },
    { "none",		0 },
    { NULL }
};

void
arla_log(unsigned level, char *fmt, ...)
{
    va_list args;
    
    assert (arla_log_method);
    
    va_start(args, fmt);
    log_vlog(arla_log_unit, level, fmt, args);
    va_end(args);
}

void
arla_loginit(char *log)
{
    assert (log);
    
    arla_log_method = log_open("arla", log);
    if (arla_log_method == NULL)
	errx (1, "arla_loginit: log_opened failed with log `%s'", log);
    arla_log_unit = log_unit_init (arla_log_method, "arla", arla_deb_units,
				   DEFAULT_LOG);
    if (arla_log_unit == NULL)
	errx (1, "arla_loginit: log_unit_init failed");
}

int
arla_log_set_level (const char *s)
{
    log_set_mask_str (arla_log_method, NULL, s);
    return 0;
}

void
arla_log_set_level_num (unsigned level)
{
    log_set_mask (arla_log_unit, level);
}

void
arla_log_get_level (char *s, size_t len)
{
    log_mask2str (arla_log_method, NULL, s, len);
}

unsigned
arla_log_get_level_num (void)
{
    return log_get_mask (arla_log_unit);
}

void
arla_log_print_levels (FILE *f)
{
    print_flags_table (arla_deb_units, f);
}

/*
 *
 */

void
arla_err (int eval, unsigned level, int error, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_verr (eval, level, error, fmt, args);
    va_end (args);
}

void
arla_verr (int eval, unsigned level, int error, const char *fmt, va_list args)
{
    char *s;

    vasprintf (&s, fmt, args);
    if (s == NULL) {
	log_log (arla_log_unit, level,
		 "Sorry, no memory to print `%s'...", fmt);
	exit (eval);
    }
    log_log (arla_log_unit, level, "%s: %s", s, koerr_gettext (error));
    free (s);
    exit (eval);
}

void
arla_errx (int eval, unsigned level, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_verrx (eval, level, fmt, args);
    va_end (args);
}

void
arla_verrx (int eval, unsigned level, const char *fmt, va_list args)
{
    log_vlog (arla_log_unit, level, fmt, args);
    exit (eval);
}

void
arla_warn (unsigned level, int error, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_vwarn (level, error, fmt, args);
    va_end (args);
}

void
arla_vwarn (unsigned level, int error, const char *fmt, va_list args)
{
    char *s;

    vasprintf (&s, fmt, args);
    if (s == NULL) {
	log_log (arla_log_unit, level,
		 "Sorry, no memory to print `%s'...", fmt);
	return;
    }
    log_log (arla_log_unit, level, "%s: %s", s, koerr_gettext (error));
    free (s);
}

void
arla_warnx (unsigned level, const char *fmt, ...)
{
    va_list args;

    va_start (args, fmt);
    arla_vwarnx (level, fmt, args);
    va_end (args);
}

void
arla_vwarnx (unsigned level, const char *fmt, va_list args)
{
    log_vlog (arla_log_unit, level, fmt, args);
}
