/* $OpenBSD$ */
/*
 * Copyright (c) 2001 Marc Espie.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
 * PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stddef.h>
#include <stdio.h>
#include <err.h>
#include "mdef.h"
#include "stdd.h"
#include "extern.h"

FILE *traceout = stderr;

int traced_macros = 0;

#define TRACE_ARGS 	1
#define TRACE_EXPANSION 2
#define TRACE_QUOTE	4
#define TRACE_FILENAME	8
#define TRACE_LINENO	16
#define TRACE_CONT	32
#define TRACE_ID	64
#define TRACE_NEWFILE	128	/* not implemented yet */
#define TRACE_INPUT	256	/* not implemented yet */
#define TRACE_ALL	512


static unsigned int letter_to_flag __P((int));
static void print_header __P((struct input_file *));

static unsigned int flags = TRACE_QUOTE | TRACE_EXPANSION;

static struct t {
	struct t *next;
	char 	 *name;
} *l;

void
mark_traced(name)
	const char *name;
{
	struct t *n;

	traced_macros = 1;
	n = xalloc(sizeof(struct t));
	n->name = xstrdup(name);
	n->next = l;
	l = n;
}


int 
is_traced(name)
	const char *name;
{
	struct t *n;

	if (flags & TRACE_ALL)
		return 1;
	for (n = l; n != NULL; n = n->next)
		if (STREQ(n->name, name))
			return 1;
	return 0;
}

void
trace_file(name)
	const char *name;
{

	if (traceout != stderr)
		fclose(traceout);
	traceout = fopen(name, "w");
	if (!traceout)
		err(1, "can't open %s", name);
}

static unsigned int
letter_to_flag(c)
	int c;
{
	switch(c) {
	case 'a':
		return TRACE_ARGS;
	case 'e':
		return TRACE_EXPANSION;
	case 'q':
		return TRACE_QUOTE;
	case 'c':
		return TRACE_CONT;
	case 'x':
		return TRACE_ID;
	case 'f':
		return TRACE_FILENAME;
	case 'l':
		return TRACE_LINENO;
	case 'p':
		return TRACE_NEWFILE;
	case 'i':
		return TRACE_INPUT;
	case 't':
		return TRACE_ALL;
	case 'V':
		return ~0;
	default:
		return 0;
	}
}

void
set_trace_flags(s)
	const char *s;
{
	char mode = 0;
	unsigned int f = 0;

	traced_macros = 1;

	if (*s == '+' || *s == '-')
		mode = *s++;
	while (*s)
		f |= letter_to_flag(*s++);
	switch(mode) {
	case 0:
		flags = f;
		break;
	case '+':
		flags |= f;
		break;
	case '-':
		flags &= ~f;
		break;
	}
}

static void
print_header(inp)
	struct input_file *inp;
{
	fprintf(traceout, "m4trace:");
	if (flags & TRACE_FILENAME)
		fprintf(traceout, "%s:", inp->name);
	if (flags & TRACE_LINENO)
		fprintf(traceout, "%lu:", inp->lineno);
	fprintf(traceout, " -1- ");
	if (flags & TRACE_ID)
		fprintf(traceout, "id %lu: ", expansion_id);
}

ssize_t 
trace(argv, argc, inp)
	const char **argv;
	int argc;
	struct input_file *inp;
{
	print_header(inp);
	if (flags & TRACE_CONT) {
		fprintf(traceout, "%s ...\n", argv[1]);
		print_header(inp);
	}
	fprintf(traceout, "%s", argv[1]);
	if ((flags & TRACE_ARGS) && argc > 2) {
		char delim = LPAREN;
		int i;

		for (i = 2; i < argc; i++) {
			fprintf(traceout, "%c%s%s%s", delim, 
			    (flags & TRACE_QUOTE) ? lquote : "", 
			    argv[i], 
			    (flags & TRACE_QUOTE) ? rquote : "");
			delim = COMMA;
		}
		fprintf(traceout, "%c", RPAREN);
	}
	if (flags & TRACE_CONT) {
		fprintf(traceout, " -> ???\n");
		print_header(inp);
		fprintf(traceout, argc > 2 ? "%s(...)" : "%s", argv[1]);
	}
	if (flags & TRACE_EXPANSION)
		return buffer_mark();
	else {
		fprintf(traceout, "\n");
		return -1;
	}
}

void 
finish_trace(mark)
size_t mark;
{
	fprintf(traceout, " -> ");
	if (flags & TRACE_QUOTE)
		fprintf(traceout, "%s", lquote);
	dump_buffer(traceout, mark);
	if (flags & TRACE_QUOTE)
		fprintf(traceout, "%s", rquote);
	fprintf(traceout, "\n");
}
