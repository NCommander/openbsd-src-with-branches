/*	$OpenBSD: extern.h,v 1.24 2000/06/23 16:18:09 espie Exp $	*/
/*	$NetBSD: nonints.h,v 1.12 1996/11/06 17:59:19 christos Exp $	*/

/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
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
 *
 *	from: @(#)nonints.h	8.3 (Berkeley) 3/19/94
 */

/* arch.c */
ReturnStatus Arch_ParseArchive __P((char **, Lst, SymTable *));
void Arch_Touch __P((GNode *));
void Arch_TouchLib __P((GNode *));
Boolean Arch_MTime __P((GNode *));
Boolean Arch_MemMTime __P((GNode *));
void Arch_FindLib __P((GNode *, Lst));
Boolean Arch_LibOODate __P((GNode *));
void Arch_Init __P((void));
void Arch_End __P((void));
int Arch_IsLib __P((GNode *));

/* compat.c */
void Compat_Run __P((Lst));

/* cond.c */
int Cond_Eval __P((char *));
void Cond_End __P((void));

#include "error.h"

/* for.c */
typedef struct For_ For;
For *For_Eval __P((char *));
Boolean For_Accumulate __P((For *, const char *));
void For_Run  __P((For *));

/* main.c */
void Main_ParseArgLine __P((char *));
char *Cmd_Exec __P((char *, char **));
void Error __P((char *, ...));
void Fatal __P((char *, ...));
void Punt __P((char *, ...));
void DieHorribly __P((void));
void PrintAddr __P((void *));
void Finish __P((int));

/* make.c */
void Make_TimeStamp __P((GNode *, GNode *));
Boolean Make_OODate __P((GNode *));
void Make_HandleUse __P((GNode *, GNode *));
void Make_Update __P((GNode *));
void Make_DoAllVar __P((GNode *));
Boolean Make_Run __P((Lst));

/* parse.c */
void Parse_Error __P((int, char *, ...));
Boolean Parse_AnyExport __P((void));
Boolean Parse_IsVar __P((char *));
void Parse_DoVar __P((char *, SymTable *));
void Parse_AddIncludeDir __P((char *));
void Parse_File __P((char *, FILE *));
void Parse_Init __P((void));
void Parse_End __P((void));
void Parse_FromString __P((char *, unsigned long));
void Parse_MainName __P((Lst));
unsigned long Parse_Getlineno __P((void));
const char *Parse_Getfilename __P((void));

/* str.c */
void str_init __P((void));
void str_end __P((void));
char *str_concat __P((char *, char *, int));
char **brk_string __P((char *, int *, Boolean, char **));
int Str_Match __P((char *, char *));
char *Str_SYSVMatch __P((char *, char *, int *len));
void Str_SYSVSubst __P((Buffer, char *, char *, int));
char *interval_dup __P((const char *begin, const char *end));

/* suff.c */
void Suff_ClearSuffixes __P((void));
Boolean Suff_IsTransform __P((char *));
GNode *Suff_AddTransform __P((char *));
void Suff_EndTransform __P((void *));
void Suff_AddSuffix __P((char *));
Lst Suff_GetPath __P((char *));
void Suff_DoPaths __P((void));
void Suff_AddInclude __P((char *));
void Suff_AddLib __P((char *));
void Suff_FindDeps __P((GNode *));
void Suff_SetNull __P((char *));
void Suff_Init __P((void));
void Suff_End __P((void));
void Suff_PrintAll __P((void));

/* targ.c */
void Targ_Init __P((void));
void Targ_End __P((void));
GNode *Targ_NewGN __P((char *));
GNode *Targ_FindNode __P((char *, int));
void Targ_FindList __P((Lst, Lst));
Boolean Targ_Ignore __P((GNode *));
Boolean Targ_Silent __P((GNode *));
Boolean Targ_Precious __P((GNode *));
void Targ_SetMain __P((GNode *));
void Targ_PrintCmd __P((void *));
char *Targ_FmtTime __P((time_t));
void Targ_PrintType __P((int));
void Targ_PrintGraph __P((int));

/* var.c */
void Var_Delete __P((char *, SymTable *));
void Var_Set __P((char *, char *, SymTable *));
void Varq_Set __P((int, char *, GNode *));
void Var_Append __P((char *, char *, SymTable *));
void Varq_Append __P((int, char *, GNode *));
Boolean Var_Exists __P((char *, SymTable *));
Boolean Varq_Exists __P((int, GNode *));
char *Var_Value __P((char *, SymTable *));
char *Varq_Value __P((int,  GNode *));
char *Var_Parse __P((char *, SymTable *, Boolean, size_t *, Boolean *));
char *Var_Subst __P((char *, SymTable *, Boolean));
void Var_SubstVar __P((Buffer, char *, const char *, SymTable *));
char *Var_GetTail __P((char *));
char *Var_GetHead __P((char *));
void Var_Init __P((void));
void Var_End __P((void));
void Var_Dump __P((SymTable *));
void SymTable_Init __P((SymTable *));
