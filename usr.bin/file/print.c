/*	$OpenBSD: print.c,v 1.8 2003/03/03 22:24:08 ian Exp $	*/

/*
 * print.c - debugging printout routines
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <err.h>
#include "file.h"

#ifndef lint
static char *moduleid = "$OpenBSD: print.c,v 1.8 2003/03/03 22:24:08 ian Exp $";
#endif  /* lint */

#define SZOF(a)	(sizeof(a) / sizeof(a[0]))

void
mdump(m)
struct magic *m;
{
	static char *typ[] = {   "invalid", "byte", "short", "invalid",
				 "long", "string", "date", "beshort",
				 "belong", "bedate", "leshort", "lelong",
				 "ledate" };
	(void) fputc('[', stderr);
	(void) fprintf(stderr, ">>>>>>>> %d" + 8 - (m->cont_level & 7),
		       m->offset);

	if (m->flag & INDIR)
		(void) fprintf(stderr, "(%s,%d),",
			       (m->in.type >= 0 && m->in.type < SZOF(typ)) ? 
					typ[(unsigned char) m->in.type] :
					"*bad*",
			       m->in.offset);

	(void) fprintf(stderr, " %s%s", (m->flag & UNSIGNED) ? "u" : "",
		       (m->type >= 0 && m->type < SZOF(typ)) ? 
				typ[(unsigned char) m->type] : 
				"*bad*");
	if (m->mask != ~0)
		(void) fprintf(stderr, " & %.8x", m->mask);

	(void) fprintf(stderr, ",%c", m->reln);

	if (m->reln != 'x') {
	    switch (m->type) {
	    case BYTE:
	    case SHORT:
	    case LONG:
	    case LESHORT:
	    case LELONG:
	    case BESHORT:
	    case BELONG:
		    (void) fprintf(stderr, "%d", m->value.l);
		    break;
	    case STRING:
		    showstr(stderr, m->value.s, -1);
		    break;
	    case DATE:
	    case LEDATE:
	    case BEDATE:
		    {
			    char *rt, *pp = ctime((time_t*) &m->value.l);
			    if ((rt = strchr(pp, '\n')) != NULL)
				    *rt = '\0';
			    (void) fprintf(stderr, "%s,", pp);
			    if (rt)
				    *rt = '\n';
		    }
		    break;
	    default:
		    (void) fputs("*bad*", stderr);
		    break;
	    }
	}
	(void) fprintf(stderr, ",\"%s\"]\n", m->desc);
}

/*
 * This "error" is here so we don't have to change all occurrences of
 * error() to err(1,...) when importing new versions from Christos.
 */
void error(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	verr(1, fmt, va);
}

/*
 * ckfputs - futs, but with error checking
 * ckfprintf - fprintf, but with error checking
 */
void
ckfputs(str, fil) 	
    const char *str;
    FILE *fil;
{
	if (fputs(str,fil) == EOF)
		err(1, "write failed");
}

/*VARARGS*/
void
ckfprintf(FILE *f, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	(void) vfprintf(f, fmt, va);
	if (ferror(f))
		err(1, "write failed");
	va_end(va);
}
