/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: isctype.c,v 1.13 1995/02/27 04:34:43 cgd Exp $";
#endif /* LIBC_SCCS and not lint */

#define _ANSI_LIBRARY
#include <ctype.h>

#undef isalnum
int
isalnum(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_U|_L|_N));
}

#undef isalpha
int
isalpha(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_U|_L));
}

#undef isblank
int
isblank(c)
	int c;
{
	return(c == ' ' || c == '\t');
}

#undef iscntrl
int
iscntrl(c)
	int c;
{
	return((_ctype_ + 1)[c] & _C);
}

#undef isdigit
int
isdigit(c)
	int c;
{
	return((_ctype_ + 1)[c] & _N);
}

#undef isgraph
int
isgraph(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_P|_U|_L|_N));
}

#undef islower
int
islower(c)
	int c;
{
	return((_ctype_ + 1)[c] & _L);
}

#undef isprint
int
isprint(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_P|_U|_L|_N|_B));
}

#undef ispunct
int
ispunct(c)
	int c;
{
	return((_ctype_ + 1)[c] & _P);
}

#undef isspace
int
isspace(c)
	int c;
{
	return((_ctype_ + 1)[c] & _S);
}

#undef isupper
int
isupper(c)
	int c;
{
	return((_ctype_ + 1)[c] & _U);
}

#undef isxdigit
int
isxdigit(c)
	int c;
{
	return((_ctype_ + 1)[c] & (_N|_X));
}

#undef isascii
int
isascii(c)
	int c;
{
	return ((unsigned)(c) <= 0177);
}

#undef toascii
int
toascii(c)
	int c;
{
	return ((c) & 0177);
}

#undef _toupper
int
_toupper(c)
	int c;
{
	return (c - 'a' + 'A');
}

#undef _tolower
int
_tolower(c)
	int c;
{
	return (c - 'A' + 'a');
}
