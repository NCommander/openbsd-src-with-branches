/* $OpenBSD$ */

/*
 * Copyright (c) 1999 Marc Espie.
 *
 * Code written for the OpenBSD project.
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

/* regression tests */
#include "make.h"
#include <stdio.h>

int main __P((void));
#define CHECK(s)		\
do {				\
    printf("%-65s", #s);	\
    if (s)			\
    	printf("ok\n"); 	\
    else {			\
    	printf("failed\n");	\
	errors++;		\
    }				\
} while (0);

int main()
{
    unsigned errors = 0;

    CHECK(Str_Match("string", "string") == 1);
    CHECK(Str_Match("string", "string2") == 0);
    CHECK(Str_Match("string", "string*") == 1);
    CHECK(Str_Match("Long string", "Lo*ng") == 1);
    CHECK(Str_Match("Long string", "Lo*ng ") == 0);
    CHECK(Str_Match("Long string", "Lo*ng *") == 1);
    CHECK(Str_Match("string", "stri?g") == 1);
    CHECK(Str_Match("str?ng", "str\\?ng") == 1);
    CHECK(Str_Match("striiiing", "str?*ng") == 1);
    CHECK(Str_Match("Very long string just to see", "******a****") == 0);
    CHECK(Str_Match("d[abc?", "d\\[abc\\?") == 1);
    CHECK(Str_Match("d[abc!", "d\\[abc\\?") == 0);
    CHECK(Str_Match("dwabc?", "d\\[abc\\?") == 0);
    CHECK(Str_Match("da0", "d[bcda]0") == 1);
    CHECK(Str_Match("da0", "d[z-a]0") == 1);
    CHECK(Str_Match("d-0", "d[-a-z]0") == 1);
    CHECK(Str_Match("dy0", "d[a\\-z]0") == 0);
    CHECK(Str_Match("d-0", "d[a\\-z]0") == 1);
    CHECK(Str_Match("dz0", "d[a\\]z]0") == 1);

    if (errors != 0)
	printf("Errors: %d\n", errors);
    exit(0);
}


