/*	$OpenBSD: rtld_machine.c,v 1.42 2015/11/02 07:02:53 guenther Exp $ */

/*
 * Copyright (c) 1999 Dale Rahn
 * Copyright (c) 2001 Niklas Hallqvist
 * Copyright (c) 2001 Artur Grabowski
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
 */

#define _DYN_LOADER

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <sys/unistd.h>
#include <machine/cpu.h>
#include <machine/trap.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

int64_t pcookie __attribute__((section(".openbsd.randomdata"))) __dso_hidden;

/*
 * The following table holds for each relocation type:
 *	- the width in bits of the memory location the relocation
 *	  applies to (not currently used)
 *	- the number of bits the relocation value must be shifted to the
 *	  right (i.e. discard least significant bits) to fit into
 *	  the appropriate field in the instruction word.
 *	- flags indicating whether
 *		* the relocation involves a symbol
 *		* the relocation is relative to the current position
 *		* the relocation is for a GOT entry
 *		* the relocation is relative to the load address
 *
 */
#define _RF_S		0x80000000		/* Resolve symbol */
#define _RF_A		0x40000000		/* Use addend */
#define _RF_P		0x20000000		/* Location relative */
#define _RF_G		0x10000000		/* GOT offset */
#define _RF_B		0x08000000		/* Load address relative */
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	((s) & 0xff)		/* right shift */
static int reloc_target_flags[] = {
	0,							/* NONE */
	_RF_S|_RF_A|		_RF_SZ(8)  | _RF_RS(0),		/* RELOC_8 */
	_RF_S|_RF_A|		_RF_SZ(16) | _RF_RS(0),		/* RELOC_16 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* RELOC_32 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(8)  | _RF_RS(0),		/* DISP_8 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(16) | _RF_RS(0),		/* DISP_16 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* DISP_32 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WDISP_30 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WDISP_22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(10),	/* HI22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 22 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* 13 */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* LO10 */
	_RF_G|			_RF_SZ(32) | _RF_RS(0),		/* GOT10 */
	_RF_G|			_RF_SZ(32) | _RF_RS(0),		/* GOT13 */
	_RF_G|			_RF_SZ(32) | _RF_RS(10),	/* GOT22 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* PC10 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(10),	/* PC22 */
	      _RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(2),		/* WPLT30 */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/* COPY */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* GLOB_DAT */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/* JMP_SLOT */
	      _RF_A|	_RF_B|	_RF_SZ(32) | _RF_RS(0),		/* RELATIVE */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* UA_32 */

	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* PLT32 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* HIPLT22 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* LOPLT10 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* LOPLT10 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* PCPLT22 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* PCPLT32 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* 10 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* 11 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* 64 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* OLO10 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* HH22 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* HM10 */
	_RF_S|_RF_A|/*unknown*/	_RF_SZ(32) | _RF_RS(0),		/* LM22 */
	_RF_S|_RF_A|_RF_P|/*unknown*/	_RF_SZ(32) | _RF_RS(0),	/* WDISP16 */
	_RF_S|_RF_A|_RF_P|/*unknown*/	_RF_SZ(32) | _RF_RS(0),	/* WDISP19 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* GLOB_JMP */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* 7 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* 5 */
	/*unknown*/		_RF_SZ(32) | _RF_RS(0),		/* 6 */
};

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)

static int reloc_target_bitmask[] = {
#define _BM(x)	(~(-(1ULL << (x))))
	0,				/* NONE */
	_BM(8), _BM(16), _BM(32),	/* RELOC_8, _16, _32 */
	_BM(8), _BM(16), _BM(32),	/* DISP8, DISP16, DISP32 */
	_BM(30), _BM(22),		/* WDISP30, WDISP22 */
	_BM(22), _BM(22),		/* HI22, _22 */
	_BM(13), _BM(10),		/* RELOC_13, _LO10 */
	_BM(10), _BM(13), _BM(22),	/* GOT10, GOT13, GOT22 */
	_BM(10), _BM(22),		/* _PC10, _PC22 */
	_BM(30), 0,			/* _WPLT30, _COPY */
	-1, -1, -1,			/* _GLOB_DAT, JMP_SLOT, _RELATIVE */
	_BM(32), _BM(32),		/* _UA32, PLT32 */
	_BM(22), _BM(10),		/* _HIPLT22, LOPLT10 */
	_BM(32), _BM(22), _BM(10),	/* _PCPLT32, _PCPLT22, _PCPLT10 */
	_BM(10), _BM(11), -1,		/* _10, _11, _64 */
	_BM(10), _BM(22),		/* _OLO10, _HH22 */
	_BM(10), _BM(22),		/* _HM10, _LM22 */
	_BM(16), _BM(19),		/* _WDISP16, _WDISP19 */
	-1,				/* GLOB_JMP */
	_BM(7), _BM(5), _BM(6)		/* _7, _5, _6 */
#undef _BM
};
#define RELOC_VALUE_BITMASK(t)	(reloc_target_bitmask[t])

static inline void
_dl_reloc_plt(Elf_Addr *where1, Elf_Addr *where2, Elf_Addr value)
{
	/*
	 * At the PLT entry pointed at by `where1' and `where2', we now
	 * construct a direct transfer to the now fully resolved function
	 * address.  The resulting code in the jump slot is:
	 *
	 *		sethi	%hi(roffset), %g1
	 * where1:	sethi	%hi(addr), %g1
	 * where2:	jmp	%g1+%lo(addr)
	 *
	 * If this was being directly applied to the PLT during resolution
	 * of a lazy binding, then to make it thread safe we would need to
	 * update the third instruction first, since that leaves the
	 * previous `b,a' at the second word in place, so that the whole
	 * PLT slot can be atomically change to the new sequence by
	 * writing the `sethi' instruction at word 2.  We would also need
	 * iflush instructions to guarantee that the third instruction
	 * made it to the I-cache before the second instruction.
	 *
	 * HOWEVER, we do lazy binding via the kbind syscall, so we can
	 * write them in order here and reorder by the kbind blocking.
	 */
#define SETHI	0x03000000
#define JMP	0x81c06000
	*where1 = SETHI | ((value >> 10) & 0x003fffff);
	*where2 = JMP   | (value & 0x000003ff);
}

int
_dl_md_reloc(elf_object_t *object, int rel, int relasz)
{
	long	i;
	long	numrela;
	long	relrel;
	int	fails = 0;
	Elf_Addr loff;
	Elf_Addr prev_value = 0;
	const Elf_Sym *prev_sym = NULL;
	Elf_RelA *relas;
	struct load_list *llist;

	loff = object->obj_base;
	numrela = object->Dyn.info[relasz] / sizeof(Elf_RelA);
	relrel = rel == DT_RELA ? object->relacount : 0;
	relas = (Elf_RelA *)(object->Dyn.info[rel]);

	if (relas == NULL)
		return(0);

	if (relrel > numrela) {
		_dl_printf("relacount > numrela: %ld > %ld\n", relrel, numrela);
		_dl_exit(20);
	}

	/*
	 * unprotect some segments if we need it.
	 */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot|PROT_WRITE);
		}
	}

	/* tight loop for leading RELATIVE relocs */
	for (i = 0; i < relrel; i++, relas++) {
		Elf_Addr *where;

#ifdef DEBUG
		if (ELF_R_TYPE(relas->r_info) != R_TYPE(RELATIVE)) {
			_dl_printf("RELACOUNT wrong\n");
			_dl_exit(20);
		}
#endif
		where = (Elf_Addr *)(relas->r_offset + loff);
		*where = relas->r_addend + loff;
	}
	for (; i < numrela; i++, relas++) {
		Elf_Addr *where, ooff;
		Elf_Word type, value, mask;
		const Elf_Sym *sym, *this;
		const char *symn;

		type = ELF_R_TYPE(relas->r_info);

		if (type == R_TYPE(NONE))
			continue;

		if (type == R_TYPE(JMP_SLOT) && rel != DT_JMPREL)
			continue;

		where = (Elf_Addr *)(relas->r_offset + loff);

		if (type == R_TYPE(RELATIVE)) {
			*where += (Elf_Addr)(loff + relas->r_addend);
			continue;
		}

		if (RELOC_USE_ADDEND(type))
			value = relas->r_addend;
		else
			value = 0;

		sym = NULL;
		symn = NULL;
		if (RELOC_RESOLVE_SYMBOL(type)) {
			sym = object->dyn.symtab;
			sym += ELF_R_SYM(relas->r_info);
			symn = object->dyn.strtab + sym->st_name;

			if (sym->st_shndx != SHN_UNDEF &&
			    ELF_ST_BIND(sym->st_info) == STB_LOCAL) {
				value += loff;
			} else if (sym == prev_sym) {
				value += prev_value;
			} else {
				this = NULL;
				ooff = _dl_find_symbol_bysym(object,
				    ELF_R_SYM(relas->r_info), &this,
				    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
				    ((type == R_TYPE(JMP_SLOT)) ?
					SYM_PLT : SYM_NOTPLT),
				    sym, NULL);
				if (this == NULL) {
resolve_failed:
					if (ELF_ST_BIND(sym->st_info) !=
					    STB_WEAK)
						fails++;
					continue;
				}
				prev_sym = sym;
				prev_value = (Elf_Addr)(ooff + this->st_value);
				value += prev_value;
			}
		}

		if (type == R_TYPE(COPY)) {
			void *dstaddr = where;
			const void *srcaddr;
			const Elf_Sym *dstsym = sym, *srcsym = NULL;
			size_t size = dstsym->st_size;
			Elf_Addr soff;

			soff = _dl_find_symbol(symn, &srcsym,
			    SYM_SEARCH_OTHER|SYM_WARNNOTFOUND|SYM_NOTPLT,
			    dstsym, object, NULL);
			if (srcsym == NULL)
				goto resolve_failed;

			srcaddr = (void *)(soff + srcsym->st_value);
			_dl_bcopy(srcaddr, dstaddr, size);
			continue;
		}

		if (type == R_TYPE(JMP_SLOT)) {
			_dl_reloc_plt(&where[1], &where[2], value);
			continue;
		}

		if (RELOC_PC_RELATIVE(type))
			value -= (Elf_Addr)where;

		mask = RELOC_VALUE_BITMASK(type);
		value >>= RELOC_VALUE_RIGHTSHIFT(type);
		value &= mask;

		/* We ignore alignment restrictions here */
		*where &= ~mask;
		*where |= value;
	}

	/* reprotect the unprotected segments */
	if ((object->dyn.textrel == 1) && (rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot);
		}
	}

	return (fails);
}

/*
 * Resolve a symbol at run-time.
 */
Elf_Addr
_dl_bind(elf_object_t *object, int reloff)
{
	const Elf_Sym *sym, *this;
	Elf_Addr *addr, ooff;
	const char *symn;
	const elf_object_t *sobj;
	Elf_Addr value;
	Elf_RelA *rela;
	int64_t cookie = pcookie;
	struct {
		struct __kbind param[2];
		Elf_Word buf[2];
	} buf;

	rela = (Elf_RelA *)(object->Dyn.info[DT_JMPREL] + reloff);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rela->r_info);
	symn = object->dyn.strtab + sym->st_name;

	this = NULL;
	ooff = _dl_find_symbol(symn, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, sym, object, &sobj);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*(volatile int *)0 = 0;		/* XXX */
	}

	value = ooff + this->st_value;

	if (__predict_false(sobj->traced) && _dl_trace_plt(sobj, symn))
		return (value);

	/*
	 * Relocations require two blocks to be written: the second word
	 * then the first word. So the layout of the buffer we pass to
	 * kbind() needs to be this:
	 *   +------------+
	 *   | kbind.addr |
	 *   | kbind.size |
	 *   | kbind.addr |
	 *   | kbind.size |
	 *   |   word 2   |
	 *   |   word 1   |
	 *   +------------+
	 */
	addr = (Elf_Addr *)(object->obj_base + rela->r_offset);
	_dl_reloc_plt(&buf.buf[1], &buf.buf[0], value);
	buf.param[0].kb_addr = &addr[2];
	buf.param[0].kb_size = sizeof(Elf_Word);
	buf.param[1].kb_addr = &addr[1];
	buf.param[1].kb_size = sizeof(Elf_Word);

	/* directly code the syscall, so that it's actually inline here */
	{
		register long syscall_num __asm("g1") = SYS_kbind;
		register void *arg1 __asm("o0") = &buf;
		register long  arg2 __asm("o1") = sizeof(buf);
		register long  arg3 __asm("o2") = 0xffffffff & (cookie >> 32);
		register long  arg4 __asm("o3") = 0xffffffff &  cookie;

		__asm volatile("t %2" : "+r" (arg1), "+r" (arg2)
		    : "i" (ST_SYSCALL), "r" (syscall_num), "r" (arg3),
		    "r" (arg4) : "cc", "memory" );
	}

	/*
	 * iflush requires 5 subsequent cycles to be sure all copies
	 * are flushed from the CPU and the icache.
	 */
	__asm volatile("iflush %0+8; iflush %0+4; nop; nop; nop; nop; nop" :
	    : "r" (addr));

	return (value);
}

int
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	int	fails = 0;
	Elf_Addr *pltgot;
	extern void _dl_bind_start(void);	/* XXX */

	pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];

	if (object->traced)
		lazy = 1;

	if (object->obj_type == OBJTYPE_LDR || !lazy || pltgot == NULL) {
		fails = _dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
	} else {
		/*
		 * PLTGOT is the PLT on the sparc.
		 * The first entry holds the call the dynamic linker.
		 * We construct a `call' sequence that transfers
		 * to `_dl_bind_start()'.
		 * The second entry holds the object identification.
		 * Note: each PLT entry is three words long.
		 */
#define SAVE	0x9de3bfc0	/* i.e. `save %sp,-64,%sp' */
#define CALL	0x40000000
#define NOP	0x01000000
		pltgot[0] = SAVE;
		pltgot[1] = CALL |
		    ((Elf_Addr)&_dl_bind_start - (Elf_Addr)&pltgot[1]) >> 2;
		pltgot[2] = NOP;
		pltgot[3] = (Elf_Addr) object;
		__asm volatile("iflush %0+8"  : : "r" (pltgot));
		__asm volatile("iflush %0+4"  : : "r" (pltgot));
		__asm volatile("iflush %0+0"  : : "r" (pltgot));
		/*
		 * iflush requires 5 subsequent cycles to be sure all copies
		 * are flushed from the CPU and the icache.
		 */
		__asm volatile("nop;nop;nop;nop;nop");
	}

	/* mprotect the GOT */
	_dl_protect_segment(object, 0, "__got_start", "__got_end", PROT_READ);

	/* mprotect the PLT */
	_dl_protect_segment(object, 0, "__plt_start", "__plt_end",
	    PROT_READ|PROT_EXEC);

	return (fails);
}


void __mul(void);
void _mulreplace_end(void);
void _mulreplace(void);
void __umul(void);
void _umulreplace_end(void);
void _umulreplace(void);

void __div(void);
void _divreplace_end(void);
void _divreplace(void);
void __udiv(void);
void _udivreplace_end(void);
void _udivreplace(void);

void __rem(void);
void _remreplace_end(void);
void _remreplace(void);
void __urem(void);
void _uremreplace_end(void);
void _uremreplace(void);

void
_dl_mul_fixup()
{
	int mib[2], v8mul;
	size_t len;


	mib[0] = CTL_MACHDEP;
	mib[1] = CPU_V8MUL;
	len = sizeof(v8mul);
	_dl_sysctl(mib, 2, &v8mul, &len, NULL, 0);


	if (!v8mul)
		return;

	_dl_mprotect(&__mul, _mulreplace_end-_mulreplace,
	    PROT_READ|PROT_WRITE);
	_dl_bcopy(_mulreplace, __mul, _mulreplace_end-_mulreplace);
	_dl_mprotect(&__mul, _mulreplace_end-_mulreplace,
	    PROT_READ|PROT_EXEC);

	_dl_mprotect(&__umul, _umulreplace_end-_umulreplace,
	    PROT_READ|PROT_WRITE);
	_dl_bcopy(_umulreplace, __umul, _umulreplace_end-_umulreplace);
	_dl_mprotect(&__umul, _umulreplace_end-_umulreplace,
	    PROT_READ|PROT_EXEC);


	_dl_mprotect(&__div, _divreplace_end-_divreplace,
	    PROT_READ|PROT_WRITE);
	_dl_bcopy(_divreplace, __div, _divreplace_end-_divreplace);
	_dl_mprotect(&__div, _divreplace_end-_divreplace,
	    PROT_READ|PROT_EXEC);

	_dl_mprotect(&__udiv, _udivreplace_end-_udivreplace,
	    PROT_READ|PROT_WRITE);
	_dl_bcopy(_udivreplace, __udiv, _udivreplace_end-_udivreplace);
	_dl_mprotect(&__udiv, _udivreplace_end-_udivreplace,
	    PROT_READ|PROT_EXEC);


	_dl_mprotect(&__rem, _remreplace_end-_remreplace,
	    PROT_READ|PROT_WRITE);
	_dl_bcopy(_remreplace, __rem, _remreplace_end-_remreplace);
	_dl_mprotect(&__rem, _remreplace_end-_remreplace,
	    PROT_READ|PROT_EXEC);

	_dl_mprotect(&__urem, _uremreplace_end-_uremreplace,
	    PROT_READ|PROT_WRITE);
	_dl_bcopy(_uremreplace, __urem, _uremreplace_end-_uremreplace);
	_dl_mprotect(&__urem, _uremreplace_end-_uremreplace,
	    PROT_READ|PROT_EXEC);
}
