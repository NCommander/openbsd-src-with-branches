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
 * 3. Neither the name of the Institute nor the names of its contributors
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

/* $KTH: types.h,v 1.10 2000/10/02 22:37:20 lha Exp $ */

#ifndef _YDR_TYPES_
#define _YDR_TYPES_

#include "sym.h"

typedef struct {
     char *name;
     int val;
} EnumEntry;

typedef enum {
     TCHAR, TUCHAR, TSHORT, TUSHORT, TLONG, TULONG, TSTRING, TOPAQUE, 
     TUSERDEF, TARRAY,
     TVARRAY, TPOINTER
} TypeType;

struct Type {
     TypeType type;
     Symbol *symbol;
     unsigned size;
     Type *subtype;
     Type *indextype;
     enum { TASIS = 1 } flags;
}; 

typedef struct {
     char *name;
     Type *type;
} StructEntry;

enum argtype { TIN, TOUT, TINOUT };

typedef struct {
     enum argtype argtype;
     char *name;
     Type *type;
} Argument;

Symbol *define_const (char *name, int value);
Symbol *define_enum (char *name, List *list);
Symbol *define_struct (char *name);
Symbol *set_struct_body_sym (Symbol *s, List *list);
Symbol *set_struct_body (char *name, List *list);
Symbol *define_typedef (StructEntry *entry);
Symbol *define_proc (char *name, List *args, unsigned id);
Symbol *createenumentry (char *name, int val);
StructEntry *createstructentry (char *name, Type *type);
struct Type *create_type (TypeType type, Symbol *symbol, unsigned size,
			  Type *subtype, Type *indextype, int flags);


#endif /* _YDR_TYPES_ */
