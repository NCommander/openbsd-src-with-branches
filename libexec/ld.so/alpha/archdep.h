/*	$OpenBSD: archdep.h,v 1.7 2002/08/12 01:05:23 drahn Exp $ */

/*
 * Copyright (c) 1998 Per Fogelstrom, Opsycon AB
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
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom, Opsycon AB, Sweden.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef _ALPHA_ARCHDEP_H_
#define _ALPHA_ARCHDEP_H_

#define	DL_MALLOC_ALIGN	8	/* Arch constraint or otherwise */

#define	MACHID	EM_ALPHA_EXP	/* ELF e_machine ID value checked */

#define	RELTYPE	Elf64_Rela
#define	RELSIZE	sizeof(Elf64_Rela)

#include <elf_abi.h>
#include <machine/reloc.h>
#include "syscall.h"
#include "util.h"

static inline void
RELOC_REL(Elf64_Rel *r, const Elf64_Sym *s, Elf64_Addr *p, unsigned long v)
{
	/* Alpha does not use REL type relocations */
	_dl_exit(20);
}

static inline void
RELOC_RELA(Elf64_Rela *r, const Elf64_Sym *s, Elf64_Addr *p, unsigned long v)
{
	extern Elf_Addr  _GLOBAL_OFFSET_TABLE_[];

	if (ELF64_R_TYPE(r->r_info) == RELOC_RELATIVE) {
		if ((caddr_t)p < (caddr_t)_GLOBAL_OFFSET_TABLE_ ||
		    (caddr_t)p >= (caddr_t)&_DYNAMIC)
			*p += (Elf_Addr)v;
	} else if (ELF64_R_TYPE(r->r_info) == RELOC_JMP_SLOT) {
		Elf64_Addr val = v + s->st_value + r->r_addend -
			(Elf64_Addr)(p);
		*p = val;
		__asm __volatile("imb" : : : "memory");
	} else if (ELF64_R_TYPE(r->r_info) == RELOC_GLOB_DAT) {
		*p = v + s->st_value + r->r_addend;
	} else {
		_dl_printf("unknown bootstrap relocation\n");
		_dl_exit(6);
	}
}

#define RELOC_GOT(obj, offs)

#endif /* _ALPHA_ARCHDEP_H_ */
