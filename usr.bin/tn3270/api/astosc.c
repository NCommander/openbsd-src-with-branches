/*	$OpenBSD: astosc.c,v 1.4 2003/06/03 02:56:18 millert Exp $	*/

/*-
 * Copyright (c) 1988 The Regents of the University of California.
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

#include <ctype.h>

#include "../general/general.h"

#include "../ctlr/function.h"

#include "astosc.h"

struct astosc astosc[256] = {
#include "astosc.out"
};

/* compare two strings, ignoring case */

static
ustrcmp(string1, string2)
char *string1;
char *string2;
{
    int c1, c2;

    while ((c1 = (unsigned char) *string1++) != 0) {
	if (isupper(c1)) {
	    c1 = tolower(c1);
	}
	if (isupper(c2 = (unsigned char) *string2++)) {
	    c2 = tolower(c2);
	}
	if (c1 < c2) {
	    return(-1);
	} else if (c1 > c2) {
	    return(1);
	}
    }
    if (*string2) {
	return(-1);
    } else {
	return(0);
    }
}


/*
 * This routine takes a string and returns an integer.  It may return
 * -1 if there is no other integer which corresponds to the
 * string.  -1 implies an error.
 */

int
ascii_to_index(string)
char *string;
{
    struct astosc *this;

    for (this = astosc; this <= &astosc[highestof(astosc)]; this++) {
	if ((this->name != 0) && (ustrcmp(this->name, string) == 0)) {
	    return this-astosc;
	}
    }
    return -1;
}
