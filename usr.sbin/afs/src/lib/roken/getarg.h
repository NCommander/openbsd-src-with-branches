/*
 * Copyright (c) 1997, 1998 Kungliga Tekniska H�gskolan
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
 *      This product includes software developed by Kungliga Tekniska 
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

/* $Id: getarg.h,v 1.8 2000/08/06 14:01:27 lha Exp $ */

#ifndef __GETARG_H__
#define __GETARG_H__

#include <stddef.h>

#define ARG_DEFAULT     0x0    /* ARG_GNUSTYLE */
#define ARG_LONGARG     0x1    /* --foo=bar */
#define ARG_SHORTARG    0x2    /* -abc   a, b and c are all three flags */
#define ARG_TRANSLONG   0x4    /* Incompatible with {SHORT,LONG}ARG */
#define ARG_SWITCHLESS  0x8    /* No switches */
#define ARG_SUBOPTION   0xF    /* For manpage generation */
#define ARG_USEFIRST	0x10   /* Use first partial found instead of failing */

#define ARG_GNUSTYLE (ARG_LONGARG|ARG_SHORTARG)
#define ARG_AFSSTYLE (ARG_TRANSLONG|ARG_SWITCHLESS)

struct getargs{
    const char *long_name;
    char short_name;
    enum { arg_end = 0, arg_integer, arg_string, 
	   arg_flag, arg_negative_flag, arg_strings,
           arg_generic_string } type;
    void *value;
    const char *help;
    const char *arg_help;
    enum { arg_optional = 0, arg_mandatory } mandatoryp;
};

enum {
    ARG_ERR_NO_MATCH  = 1,
    ARG_ERR_BAD_ARG,
    ARG_ERR_NO_ARG
};

typedef struct getarg_strings {
    int num_strings;
    char **strings;
} getarg_strings;

int getarg(struct getargs *args,
	   int argc, char **argv, int *optind, int style);

void arg_printusage (struct getargs *args,
		     const char *progname,
		     const char *extra_string,
		     int style);

#endif /* __GETARG_H__ */
