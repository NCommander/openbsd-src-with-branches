/*	$OpenBSD: files.c,v 1.6 2000/06/30 16:00:23 millert Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)files.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: files.c,v 1.6 2000/06/30 16:00:23 millert Exp $";
#endif
#endif /* not lint */

#include "sort.h"
#include "fsort.h"

#include <string.h>

/*
 * this is the subroutine for file management for fsort().
 * It keeps the buffers for all temporary files.
 */
int
getnext(binno, infl0, nfiles, pos, end, dummy)
	int binno;
	union f_handle infl0;
	int nfiles;
	register RECHEADER *pos;
	register u_char *end;
	struct field *dummy;
{
	register int i;
	register u_char *hp;
	static int nleft = 0;
	static int cnt = 0, flag = -1;
	static u_char maxb = 0;
	static FILE *fp;

	if (nleft == 0) {
		if (binno < 0)	/* reset files. */ {
			for (i = 0; i < nfiles; i++) {
				rewind(fstack[infl0.top + i].fp);
				fstack[infl0.top + i].max_o = 0;
			}
			flag = -1;
			nleft = cnt = 0;
			return (-1);
		}
		maxb = fstack[infl0.top].maxb;
		for (; nleft == 0; cnt++) {
			if (cnt >= nfiles) {
				cnt = 0;
				return (EOF);
			}
			fp = fstack[infl0.top + cnt].fp;
			hp = (u_char *) &nleft;
			for (i = sizeof(TRECHEADER); i; --i)
				*hp++ = getc(fp);
			if (binno < maxb)
				fstack[infl0.top+cnt].max_o
					+= sizeof(nleft) + nleft;
			else if (binno == maxb) {
				if (binno != fstack[infl0.top].lastb) {
					fseek(fp, fstack[infl0.top+
						cnt].max_o, SEEK_SET);
					fread(&nleft, sizeof(nleft), 1, fp);
				}
				if (nleft == 0)
					fclose(fp);
			} else if (binno == maxb + 1) {		/* skip a bin */
				fseek(fp, nleft, SEEK_CUR);
				fread(&nleft, sizeof(nleft), 1, fp);
				flag = cnt;
			}
		}
	}
	if ((u_char *) pos > end - sizeof(TRECHEADER))
		return (BUFFEND);
	hp = (u_char *) pos;
	for (i = sizeof(TRECHEADER); i ; --i)
		*hp++ = (u_char) getc(fp);
	if (end - pos->data < pos->length) {
		for (i = sizeof(TRECHEADER); i ;  i--)
			ungetc(*--hp, fp);
		return (BUFFEND);
	}
	fread(pos->data, pos->length, 1, fp);
	nleft -= pos->length + sizeof(TRECHEADER);
	if (nleft == 0 && binno == fstack[infl0.top].maxb)
		fclose(fp);
	return (0);
}

/*
 * this is called when there is no special key. It's only called
 * in the first fsort pass.
 */
int
makeline(flno, filelist, nfiles, buffer, bufend, dummy2)
	int flno;
	union f_handle filelist;
	int nfiles;
	RECHEADER *buffer;
	u_char *bufend;
	struct field *dummy2;
{
	static char *opos;
	register char *end, *pos;
	static int fileno = 0, overflow = 0;
	static FILE *fp = 0;
	register int c;

	pos = (char *) buffer->data;
	end = min((char *) bufend, pos + MAXLLEN);
	if (overflow) {
		memmove(pos, opos, bufend - (u_char *) opos);
		pos += ((char *) bufend - opos);
		overflow = 0;
	}
	for (;;) {
		if (flno >= 0 && (fp = fstack[flno].fp) == NULL)
			return (EOF);
		else if (fp == 0) {
			if (fileno  >= nfiles)
				return (EOF);
			if (!(fp = fopen(filelist.names[fileno], "r")))
				err(2, "%s", filelist.names[fileno]);
			fileno++;
		}
		while ((pos < end) && ((c = getc(fp)) != EOF)) {
			if ((*pos++ = c) == REC_D) {
				buffer->offset = 0;
				buffer->length = pos - (char *) buffer->data;
				return (0);
			}
		}
		if (pos >= end && end == (char *) bufend) {
			if ((char *) buffer->data < end) {
				overflow = 1;
				opos = (char *) buffer->data;
			}
			return (BUFFEND);
		} else if (c == EOF) {
			if (buffer->data != (u_char *) pos) {
				warnx("last character not record delimiter");
				*pos++ = REC_D;
				buffer->offset = 0;
				buffer->length = pos - (char *) buffer->data;
				return (0);
			}
			FCLOSE(fp);
			fp = 0;
			if (flno >= 0)
				fstack[flno].fp = 0;
		} else {
			buffer->data[100] = '\000';
			warnx("line too long: ignoring %s...", buffer->data);
		}
	}
}

/*
 * This generates keys. It's only called in the first fsort pass
 */
int
makekey(flno, filelist, nfiles, buffer, bufend, ftbl)
	int flno, nfiles;
	union f_handle filelist;
	RECHEADER *buffer;
	u_char *bufend;
	struct field *ftbl;
{
	static int (*get)();
	static int fileno = 0;
	static FILE *dbdesc = 0;
	static DBT dbkey[1], line[1];
	static int overflow = 0;
	int c;

	if (overflow) {
		overflow = 0;
		enterkey(buffer, line, bufend - (u_char *) buffer, ftbl);
		return (0);
	}
	for (;;) {
		if (flno >= 0) {
			get = seq;
			if (!(dbdesc = fstack[flno].fp))
				return (EOF);
		} else if (!dbdesc) {
			if (fileno  >= nfiles)
				return (EOF);
			dbdesc = fopen(filelist.names[fileno], "r");
			if (!dbdesc)
				err(2, "%s", filelist.names[fileno]);
			fileno++;
			get = seq;
		}
		if (!(c = get(dbdesc, line, dbkey))) {
			if ((signed)line->size > bufend - buffer->data)
				overflow = 1;
			else
				overflow = enterkey(buffer, line,
				    bufend - (u_char *) buffer, ftbl);
			if (overflow)
				return (BUFFEND);
			else
				return (0);
		}
		if (c == EOF) {
			FCLOSE(dbdesc);
			dbdesc = 0;
			if (flno >= 0)
				fstack[flno].fp = 0;
		} else {
			((char *) line->data)[60] = '\000';
			warnx("line too long: ignoring %.100s...",
			    (char *)line->data);
		}
	}
}

/*
 * get a key/line pair from fp
 */
int
seq(fp, line, key)
	FILE *fp;
	DBT *line;
	DBT *key;
{
	static char *buf, flag = 1;
	register char *end, *pos;
	register int c;

	if (flag) {
		flag = 0;
		buf = (char *) linebuf;
		end = buf + MAXLLEN;
		line->data = buf;
	}
	pos = buf;
	while ((c = getc(fp)) != EOF) {
		if ((*pos++ = c) == REC_D) {
			line->size = pos - buf;
			return (0);
		}
		if (pos == end) {
			line->size = MAXLLEN;
			*--pos = REC_D;
			while ((c = getc(fp)) != EOF) {
				if (c == REC_D)
					return (BUFFEND);
			}
		}
	}
	if (pos != buf) {
		warnx("last character not record delimiter");
		*pos++ = REC_D;
		line->size = pos - buf;
		return (0);
	} else
		return (EOF);
}

/*
 * write a key/line pair to a temporary file
 */
void
putrec(rec, fp)
	register RECHEADER *rec;
	register FILE *fp;
{
	EWRITE(rec, 1, rec->length + sizeof(TRECHEADER), fp);
}

/*
 * write a line to output
 */
void
putline(rec, fp)
	register RECHEADER *rec;
	register FILE *fp;
{
	EWRITE(rec->data+rec->offset, 1, rec->length - rec->offset, fp);
}

/*
 * get a record from a temporary file. (Used by merge sort.)
 */
int
geteasy(flno, filelist, nfiles, rec, end, dummy2)
	int flno, nfiles;
	union f_handle filelist;
	register RECHEADER *rec;
	register u_char *end;
	struct field *dummy2;
{
	int i;
	FILE *fp;

	fp = fstack[flno].fp;
	if ((u_char *) rec > end - sizeof(TRECHEADER))
		return (BUFFEND);
	if (!fread(rec, 1, sizeof(TRECHEADER), fp)) {
		fclose(fp);
		fstack[flno].fp = 0;
		return (EOF);
	}
	if (end - rec->data < rec->length) {
		for (i = sizeof(TRECHEADER) - 1; i >= 0;  i--)
			ungetc(*((char *) rec + i), fp);
		return (BUFFEND);
	}
	fread(rec->data, rec->length, 1, fp);
	return (0);
}
