/* $OpenBSD: md_init.h,v 1.7 2016/03/13 18:35:02 guenther Exp $ */

/*
 * Copyright (c) 2003 Dale Rahn. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */  


#define MD_SECT_CALL_FUNC(section, func)			\
	__asm (".section .rodata			\n"	\
	"	.align 8				\n"	\
	"L$" #func "					\n"	\
	"	.dword "#func "				\n"	\
	"	.previous				\n"	\
	"	.section "#section",\"ax\",@progbits	\n"	\
	"	addil	LR'L$" #func "-$global$, %dp	\n"	\
	"	ldd	RR'L$" #func "-$global$(%r1), %r1\n"	\
	"	bve,l	(%r1), %rp			\n"	\
	"	std	%dp,-80(%sp)			\n"	\
	"	ldd	-80(%sp),%dp			\n"	\
	"	.previous")

#define MD_SECTION_PROLOGUE(sect, entry_pt)			\
	__asm (						   	\
	"	.section "#sect",\"ax\",@progbits	\n"	\
	"	.EXPORT "#entry_pt",ENTRY,PRIV_LEV=3,ARGW0=NO,ARGW1=NO,ARGW2=NO,ARGW3=NO,RTNVAL=NO					\n"	\
	"	.align 4				\n"	\
	#entry_pt"					\n"	\
	"	std %rp, -16(%sp)			\n"	\
	"	ldo 128(%sp),%sp			\n"	\
	"	/* fall thru */				\n"	\
	"	.previous")


#define MD_SECTION_EPILOGUE(sect)				\
	__asm (							\
	"	.section "#sect",\"ax\",@progbits	\n"	\
	"	ldd -144(%sp),%rp			\n"	\
	"	bv %r0(%rp)				\n"	\
	"	ldo -128(%sp),%sp			\n"	\
	"	.previous")

#include <sys/exec.h>		/* for struct psstrings */

/* XXX no cleanup() callback passed to __start yet? */
#define	MD_NO_CLEANUP

#define	MD_CRT0_START						\
	__asm(							\
	".import $global$, data					\n" \
	"	.import ___start, code				\n" \
	"	.text						\n" \
	"	.align	4					\n" \
	"	.export _start, entry				\n" \
	"	.export __start, entry				\n" \
	"	.type	_start,@function			\n" \
	"	.type	__start,@function			\n" \
	"	.label _start					\n" \
	"	.label __start					\n" \
	"	.proc						\n" \
	"	.callinfo frame=0, calls			\n" \
	"	.entry						\n" \
	"	ldil	L%__gp, %r27				\n" \
	"	.call						\n" \
	"	b	___start				\n" \
	"	ldo	R%__gp(%r27), %r27			\n" \
	"	.exit						\n" \
	"	.procend")

#define	MD_START_ARGS	struct ps_strings *arginfo, void (*cleanup)(void)
#define	MD_START_SETUP				\
	char	**argv, **envp;			\
	int	argc;				\
						\
	argv = arginfo->ps_argvstr;		\
	argc = arginfo->ps_nargvstr;		\
	envp = arginfo->ps_envstr;

#define	MD_EPROL_LABEL	__asm (".export _eprol, entry\n\t.label _eprol")
