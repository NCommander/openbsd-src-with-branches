#define _DYN_LOADER

#include <sys/types.h>
#include <sys/cdefs.h>
#include <sys/mman.h>

#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

void
_dl_bcopy(const void *src, void *dest, int size)
{
	const unsigned char *psrc = src;
	unsigned char *pdest = dest;
	int i;

	for (i = 0; i < size; i++)
		pdest[i] = psrc[i];
}

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
#define _RF_U		0x04000000		/* Unaligned */
#define _RF_SZ(s)	(((s) & 0xff) << 8)	/* memory target size */
#define _RF_RS(s)	((s) & 0xff)		/* right shift */
static int reloc_target_flags[] = {
	0,							/* NONE */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* RELOC_32*/
	_RF_S|_RF_A|_RF_P|	_RF_SZ(32) | _RF_RS(0),		/* PC32 */
	_RF_G|			_RF_SZ(32) | _RF_RS(00),	/* GOT32 */
	      _RF_A|		_RF_SZ(32) | _RF_RS(0),		/* PLT32 */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/* COPY */
	_RF_S|_RF_A|		_RF_SZ(32) | _RF_RS(0),		/* GLOB_DAT */
	_RF_S|			_RF_SZ(32) | _RF_RS(0),		/* JUMP_SLOT */
	      _RF_A|	_RF_B|	_RF_SZ(32) | _RF_RS(0),		/* RELATIVE */
	0,							/* GOTOFF XXX */
	0,							/* GOTPC XXX */
	0,							/* DUMMY 11 */
	0,							/* DUMMY 12 */
	0,							/* DUMMY 13 */
	0,							/* DUMMY 14 */
	0,							/* DUMMY 15 */
	0,							/* DUMMY 16 */
	0,							/* DUMMY 17 */
	0,							/* DUMMY 18 */
	0,							/* DUMMY 19 */
	_RF_S|_RF_A|		_RF_SZ(16) | _RF_RS(0),		/* RELOC_16 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(16) | _RF_RS(0),		/* PC_16 */
	_RF_S|_RF_A|		_RF_SZ(8) | _RF_RS(0),		/* RELOC_8 */
	_RF_S|_RF_A|_RF_P|	_RF_SZ(8) | _RF_RS(0),		/* RELOC_PC8 */
};

#define RELOC_RESOLVE_SYMBOL(t)		((reloc_target_flags[t] & _RF_S) != 0)
#define RELOC_PC_RELATIVE(t)		((reloc_target_flags[t] & _RF_P) != 0)
#define RELOC_BASE_RELATIVE(t)		((reloc_target_flags[t] & _RF_B) != 0)
#define RELOC_UNALIGNED(t)		((reloc_target_flags[t] & _RF_U) != 0)
#define RELOC_USE_ADDEND(t)		((reloc_target_flags[t] & _RF_A) != 0)
#define RELOC_TARGET_SIZE(t)		((reloc_target_flags[t] >> 8) & 0xff)
#define RELOC_VALUE_RIGHTSHIFT(t)	(reloc_target_flags[t] & 0xff)

static long reloc_target_bitmask[] = {
#define _BM(x)	(~(-(1ULL << (x))))
	0,		/* NONE */
	_BM(32),	/* RELOC_32*/
	_BM(32),	/* PC32 */
	_BM(32),	/* GOT32 */
	_BM(32),	/* PLT32 */
	0,		/* COPY */
	_BM(32),	/* GLOB_DAT */
	_BM(32),	/* JUMP_SLOT */
	_BM(32),	/* RELATIVE */
	0,		/* GOTOFF XXX */
	0,		/* GOTPC XXX */
	0,		/* DUMMY 11 */
	0,		/* DUMMY 12 */
	0,		/* DUMMY 13 */
	0,		/* DUMMY 14 */
	0,		/* DUMMY 15 */
	0,		/* DUMMY 16 */
	0,		/* DUMMY 17 */
	0,		/* DUMMY 18 */
	0,		/* DUMMY 19 */
	_BM(16),	/* RELOC_16 */
	_BM(8),		/* PC_16 */
	_BM(8),		/* RELOC_8 */
	_BM(8),		/* RELOC_PC8 */
#undef _BM
};
#define RELOC_VALUE_BITMASK(t)	(reloc_target_bitmask[t])

void _dl_reloc_plt(Elf_Addr *where, Elf_Addr value);

int
_dl_md_reloc(elf_object_t *object, int rel, int relsz)
{
	long	i;
	long	numrel;
	long	fails = 0;
	Elf_Addr loff;
	Elf_Rel *rels;
	struct load_list *llist;

	loff = object->load_offs;
	numrel = object->Dyn.info[relsz] / sizeof(Elf32_Rel);
	rels = (Elf32_Rel *)(object->Dyn.info[rel]);
	if (rels == NULL)
		return(0);

	/*
	 * unprotect some segments if we need it.
	 */
	if ((rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot|PROT_WRITE);
		}
	}

	for (i = 0; i < numrel; i++, rels++) {
		Elf_Addr *where, value, ooff, mask;
		Elf_Word type;
		const Elf_Sym *sym, *this;
		const char *symn;

		type = ELF_R_TYPE(rels->r_info);

		if (type == R_TYPE(NONE))
			continue;

		if (type == R_TYPE(JUMP_SLOT) && rel != DT_JMPREL)
			continue;

		where = (Elf_Addr *)(rels->r_offset + loff);

		if (RELOC_USE_ADDEND(type))
			value = *where & RELOC_VALUE_BITMASK(type);
		else
			value = 0;

		sym = NULL;
		symn = NULL;
		if (RELOC_RESOLVE_SYMBOL(type)) {
			sym = object->dyn.symtab;
			sym += ELF_R_SYM(rels->r_info);
			symn = object->dyn.strtab + sym->st_name;

			if (sym->st_shndx != SHN_UNDEF &&
			    ELF_ST_BIND(sym->st_info) == STB_LOCAL) {
				value += loff;
			} else {
				this = NULL;
				ooff = _dl_find_symbol(symn, _dl_objects,
				    &this, SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
				    ((type == R_TYPE(JUMP_SLOT))?
					SYM_PLT:SYM_NOTPLT),
				    sym->st_size);
				if (this == NULL) {
resolve_failed:
					_dl_printf("%s: %s: can't resolve "
					    "reference '%s'\n",
					    _dl_progname, object->load_name,
					    symn);
					fails++;
					continue;
				}
				value += (Elf_Addr)(ooff + this->st_value);
			}
		}

		if (type == R_TYPE(JUMP_SLOT)) {
			_dl_reloc_plt((Elf_Word *)where, value);
			continue;
		}

		if (type == R_TYPE(COPY)) {
			void *dstaddr = where;
			const void *srcaddr;
			const Elf_Sym *dstsym = sym, *srcsym = NULL;
			size_t size = dstsym->st_size;
			Elf_Addr soff;

			soff = _dl_find_symbol(symn, object->next, &srcsym,
			    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|
			    ((type == R_TYPE(JUMP_SLOT)) ? SYM_PLT:SYM_NOTPLT),
			    size);
			if (srcsym == NULL)
				goto resolve_failed;

			srcaddr = (void *)(soff + srcsym->st_value);
			_dl_bcopy(srcaddr, dstaddr, size);
			continue;
		}

		if (RELOC_PC_RELATIVE(type))
			value -= (Elf_Addr)where;
		if (RELOC_BASE_RELATIVE(type))
			value += loff;

		mask = RELOC_VALUE_BITMASK(type);
		value >>= RELOC_VALUE_RIGHTSHIFT(type);
		value &= mask;

		if (RELOC_UNALIGNED(type)) {
			/* Handle unaligned relocations. */
			Elf_Addr tmp = 0;
			char *ptr = (char *)where;
			int i, size = RELOC_TARGET_SIZE(type)/8;

			/* Read it in one byte at a time. */
			for (i=0; i<size; i++)
				tmp = (tmp << 8) | ptr[i];

			tmp &= ~mask;
			tmp |= value;

			/* Write it back out. */
			for (i=0; i<size; i++)
				ptr[i] = ((tmp >> (8*i)) & 0xff);
		} else if (RELOC_TARGET_SIZE(type) > 32) {
			*where &= ~mask;
			*where |= value;
		} else {
			Elf32_Addr *where32 = (Elf32_Addr *)where;

			*where32 &= ~mask;
			*where32 |= value;
		}
	}

	/* reprotect the unprotected segments */
	if ((rel == DT_REL || rel == DT_RELA)) {
		for (llist = object->load_list; llist != NULL; llist = llist->next) {
			if (!(llist->prot & PROT_WRITE))
				_dl_mprotect(llist->start, llist->size,
				    llist->prot);
		}
	}

	return (fails);
}

struct jmpslot {
	u_short opcode;
	u_short addr[2];
	u_short reloc_index;
#define JMPSLOT_RELOC_MASK              0xffff
};
#define JUMP    0xe990          /* NOP + JMP opcode */

void
_dl_reloc_plt(Elf_Addr *where, Elf_Addr value)
{
	*where = value;
}

/*
 * Resolve a symbol at run-time.
 */
Elf_Addr
_dl_bind(elf_object_t *object, int index)
{
	Elf_Rel *rel;
	Elf_Word *addr;
	const Elf_Sym *sym, *this;
	const char *symn;
	Elf_Addr ooff;

	rel = (Elf_Rel *)(object->Dyn.info[DT_JMPREL]);

	rel += index/sizeof(Elf_Rel);

	sym = object->dyn.symtab;
	sym += ELF_R_SYM(rel->r_info);
	symn = object->dyn.strtab + sym->st_name;

	addr = (Elf_Word *)(object->load_offs + rel->r_offset);
	this = NULL;
	ooff = _dl_find_symbol(symn, _dl_objects, &this,
	    SYM_SEARCH_ALL|SYM_WARNNOTFOUND|SYM_PLT, 0);
	if (this == NULL) {
		_dl_printf("lazy binding failed!\n");
		*((int *)0) = 0;        /* XXX */
	}

	_dl_reloc_plt(addr, ooff + this->st_value);

	return((Elf_Addr)ooff + this->st_value);
}

void
_dl_md_reloc_got(elf_object_t *object, int lazy)
{
	extern void _dl_bind_start(void);       /* XXX */
	Elf_Addr *pltgot = (Elf_Addr *)object->Dyn.info[DT_PLTGOT];
	int i, num;
	Elf_Rel *rel;
	struct load_list *llist;

	pltgot[1] = (Elf_Addr)object;
	pltgot[2] = (Elf_Addr)&_dl_bind_start;

	if (object->Dyn.info[DT_PLTREL] != DT_REL)
		return;

	if (!lazy) {
		_dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
		return;
	}

	rel = (Elf_Rel *)(object->Dyn.info[DT_JMPREL]);
	num = (object->Dyn.info[DT_PLTRELSZ]);
	for (llist = object->load_list; llist != NULL; llist = llist->next) {
		if (!(llist->prot & PROT_WRITE))
			_dl_mprotect(llist->start, llist->size,
			    llist->prot|PROT_WRITE);
	}
	for (i = 0; i < num/sizeof(Elf_Rel); i++, rel++) {
		Elf_Addr *where;
		where = (Elf_Addr *)(rel->r_offset + object->load_offs);
		*where += object->load_offs;
	}
	for (llist = object->load_list; llist != NULL; llist = llist->next) {
		if (!(llist->prot & PROT_WRITE))
			_dl_mprotect(llist->start, llist->size,
			    llist->prot);
	}
}


