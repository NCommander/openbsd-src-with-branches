/*	$OpenBSD: archdep.h,v 1.15 2017/01/24 07:48:37 guenther Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
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

#ifndef _HPPA_ARCHDEP_H_
#define _HPPA_ARCHDEP_H_

#define	RELOC_TAG	DT_RELA
#define	HAVE_JMPREL	1

#define	MACHID	EM_PARISC		/* ELF e_machine ID value checked */

#include <elf.h>
#include <machine/reloc.h>
#include "syscall.h"
#include "util.h"


static inline void
RELOC_JMPREL(Elf_RelA *r, const Elf_Sym *s, Elf_Addr *p, unsigned long v,
    Elf_Addr *pltgot)
{
	if (ELF_R_TYPE(r->r_info) == RELOC_IPLT) {
		p[0] = v + s->st_value + r->r_addend;
		p[1] = (Elf_Addr)pltgot;
	} else {
		_dl_exit(5);
	}
}

static inline void
RELOC_DYN(Elf_RelA *r, const Elf_Sym *s, Elf_Addr *p, unsigned long v)
{
	if (ELF_R_TYPE(r->r_info) == RELOC_DIR32) {
		if (ELF_R_SYM(r->r_info) != 0)
			*p = v + s->st_value + r->r_addend;
		else
			*p = v + r->r_addend;
	} else if (ELF_R_TYPE(r->r_info) == RELOC_PLABEL32) {
		*p = v + s->st_value + r->r_addend;
	} else {
		_dl_exit(6);
	}
}

#define RELOC_GOT(obj, offs)

void _hppa_dl_dtors(void);
Elf_Addr _dl_md_plabel(Elf_Addr, Elf_Addr *);

#endif /* _HPPA_ARCHDEP_H_ */
