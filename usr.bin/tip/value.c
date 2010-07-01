/*	$OpenBSD: value.c,v 1.24 2010/07/01 20:24:19 chl Exp $	*/
/*	$NetBSD: value.c,v 1.6 1997/02/11 09:24:09 mrg Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include "tip.h"

#define MIDDLE	35

static value_t *vlookup(char *);
static void vassign(value_t *, char *);
static void vtoken(char *);
static void vprint(value_t *);
static char *vinterp(char *, int);

static size_t col = 0;

/*
 * Variable manipulation
 */
void
vinit(void)
{
	char file[FILENAME_MAX], *cp;
	int written;
	FILE *fp;

	/* Read environment variables. */
	if (cp = getenv("HOME"))
		vsetstr(HOME, cp);
	if (cp = getenv("SHELL"))
		vsetstr(SHELL, cp);

	/* Read the .tiprc file in the HOME directory. */
	written = snprintf(file, sizeof(file), "%s/.tiprc", vgetstr(HOME));
	if (written < 0 || written >= sizeof(file)) {
		(void)fprintf(stderr, "Home directory path too long: %s\n",
		    vgetstr(HOME));
	} else {
		if ((fp = fopen(file, "r")) != NULL) {
			char *tp;

			while (fgets(file, sizeof(file), fp) != NULL) {
				if (vflag)
					printf("set %s", file);
				if ((tp = strrchr(file, '\n')))
					*tp = '\0';
				vlex(file);
			}
			fclose(fp);
		}
	}
}

/*VARARGS1*/
static void
vassign(value_t *p, char *v)
{
	if (p->v_flags & V_READONLY) {
		printf("access denied\r\n");
		return;
	}

	switch (p->v_flags & V_TYPEMASK) {
	case V_STRING:
		if (p->v_value && strcmp(p->v_value, v) == 0)
			return;
		if (!(p->v_flags & V_INIT))
			free(p->v_value);
		if ((p->v_value = strdup(v)) == NULL) {
			printf("out of core\r\n");
			return;
		}
		p->v_flags &= ~V_INIT;
		break;
	case V_NUMBER:
		if (p->v_value == v)
			return;
		p->v_value = v;
		break;
	case V_BOOL:
		if ((long)p->v_value == (*v != '!'))
			return;
		p->v_value = (char *)(long)(*v != '!');
		break;
	case V_CHAR:
		if ((char)(long)p->v_value == *v)
			return;
		p->v_value = (char *)(long)*v;
	}
	p->v_flags |= V_CHANGED;
}

void
vlex(char *s)
{
	value_t *p;
	char *cp;

	if (strcmp(s, "all") == 0) {
		for (p = vtable; p->v_name; p++)
			vprint(p);
	} else {
		do {
			if ((cp = vinterp(s, ' ')))
				cp++;
			vtoken(s);
			s = cp;
		} while (s);
	}
	if (col > 0) {
		printf("\r\n");
		col = 0;
	}
}

static void
vtoken(char *s)
{
	value_t *p;
	char *cp;

	if ((cp = strchr(s, '='))) {
		*cp = '\0';
		if ((p = vlookup(s))) {
			cp++;
			if ((p->v_flags & V_TYPEMASK) == V_NUMBER)
				vassign(p, (char *)(long)atoi(cp));
			else {
				if (strcmp(s, "record") == 0)
					cp = expand(cp);
				vassign(p, cp);
			}
			return;
		}
	} else if ((cp = strchr(s, '?'))) {
		*cp = '\0';
		if (p = vlookup(s)) {
			vprint(p);
			return;
		}
	} else {
		if (*s != '!')
			p = vlookup(s);
		else
			p = vlookup(s+1);
		if (p != NULL) {
			vassign(p, s);
			return;
		}
	}
	printf("%s: unknown variable\r\n", s);
}

static void
vprint(value_t *p)
{
	char *cp;

	if (col > 0 && col < MIDDLE)
		while (col++ < MIDDLE)
			putchar(' ');
	col += size(p->v_name);
	switch (p->v_flags & V_TYPEMASK) {

	case V_BOOL:
		if (!p->v_value) {
			col++;
			putchar('!');
		}
		printf("%s", p->v_name);
		break;

	case V_STRING:
		printf("%s=", p->v_name);
		col++;
		if (p->v_value) {
			cp = interp(p->v_value);
			col += size(cp);
			printf("%s", cp);
		}
		break;

	case V_NUMBER:
		col += 6;
		printf("%s=%-5ld", p->v_name, (long)p->v_value);
		break;

	case V_CHAR:
		printf("%s=", p->v_name);
		col++;
		if (p->v_value) {
			cp = ctrl((char)(long)p->v_value);
			col += size(cp);
			printf("%s", cp);
		}
		break;
	}
	if (col >= MIDDLE) {
		col = 0;
		printf("\r\n");
		return;
	}
}

static value_t *
vlookup(char *s)
{
	value_t *p;

	for (p = vtable; p->v_name; p++) {
		if (strcmp(p->v_name, s) == 0 ||
		    (p->v_abbrev && strcmp(p->v_abbrev, s) == 0))
			return (p);
	}
	return (NULL);
}

static char *
vinterp(char *s, int stop)
{
	char *p = s, c;
	int num;

	while ((c = *s++) && c != stop) {
		switch (c) {

		case '^':
			if (*s)
				*p++ = *s++ - 0100;
			else
				*p++ = c;
			break;

		case '\\':
			num = 0;
			c = *s++;
			if (c >= '0' && c <= '7')
				num = (num<<3)+(c-'0');
			else {
				char *q = "n\nr\rt\tb\bf\f";

				for (; *q; q++)
					if (c == *q++) {
						*p++ = *q;
						goto cont;
					}
				*p++ = c;
			cont:
				break;
			}
			if ((c = *s++) >= '0' && c <= '7') {
				num = (num<<3)+(c-'0');
				if ((c = *s++) >= '0' && c <= '7')
					num = (num<<3)+(c-'0');
				else
					s--;
			} else
				s--;
			*p++ = num;
			break;

		default:
			*p++ = c;
		}
	}
	*p = '\0';
	return (c == stop ? s-1 : NULL);
}

/*
 * assign variable s with value v (for NUMBER or STRING or CHAR types)
 */
int
vstring(char *s, char *v)
{
	value_t *p;

	p = vlookup(s);
	if (p == 0)
		return (1);
	if ((p->v_flags & V_TYPEMASK) == V_NUMBER)
		vassign(p, (char *)(long)atoi(v));
	else {
		if (strcmp(s, "record") == 0)
			v = expand(v);
		vassign(p, v);
	}
	return (0);
}
