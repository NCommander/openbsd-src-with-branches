/*	$OpenBSD$	*/

/*
 * Copyright (c) 2003 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ELF_TARG_CLASS == ELFCLASS32
#define	swap_addr	swap32
#define	swap_off	swap32
#define	swap_sword	swap32
#define	swap_word	swap32
#define	swap_sxword	swap32
#define	swap_xword	swap32
#define	swap_half	swap16
#define	swap_quarter	swap16
#elif ELF_TARG_CLASS == ELFCLASS64
#define	swap_addr	swap64
#define	swap_off	swap64
#ifdef __alpha__
#define	swap_sword	swap64
#define	swap_word	swap64
#else
#define	swap_sword	swap32
#define	swap_word	swap32
#endif
#define	swap_sxword	swap64
#define	swap_xword	swap64
#define	swap_half	swap64
#define	swap_quarter	swap16
#else
#error "Unsupported ELF class"
#endif

int
elf_fix_header(Elf_Ehdr *eh)
{
	/* nothing to do */
	if (eh->e_ident[EI_DATA] == ELF_TARG_DATA)
		return (0);

	eh->e_type = swap16(eh->e_type);
	eh->e_machine = swap16(eh->e_machine);
	eh->e_version = swap32(eh->e_version);
	eh->e_entry = swap_addr(eh->e_entry);
	eh->e_phoff = swap_off(eh->e_phoff);
	eh->e_shoff = swap_off(eh->e_shoff);
	eh->e_flags = swap32(eh->e_flags);
	eh->e_ehsize = swap16(eh->e_ehsize);
	eh->e_phentsize = swap16(eh->e_phentsize);
	eh->e_phnum = swap16(eh->e_phnum);
	eh->e_shentsize = swap16(eh->e_shentsize);
	eh->e_shnum = swap16(eh->e_shnum);
	eh->e_shstrndx = swap16(eh->e_shstrndx);

	return (1);
}

int
elf_fix_shdrs(Elf_Ehdr *eh, Elf_Shdr *shdr)
{
	int i;

	/* nothing to do */
	if (eh->e_ident[EI_DATA] == ELF_TARG_DATA)
		return (0);

	for (i = eh->e_shnum; i--; shdr++) {
		shdr->sh_name = swap32(shdr->sh_name);
		shdr->sh_type = swap32(shdr->sh_type);
		shdr->sh_flags = swap_xword(shdr->sh_flags);
		shdr->sh_addr = swap_addr(shdr->sh_addr);
		shdr->sh_offset = swap_off(shdr->sh_offset);
		shdr->sh_size = swap_xword(shdr->sh_size);
		shdr->sh_link = swap32(shdr->sh_link);
		shdr->sh_info = swap32(shdr->sh_info);
		shdr->sh_addralign = swap_xword(shdr->sh_addralign);
		shdr->sh_entsize = swap_xword(shdr->sh_entsize);
	}

	return (1);
}

int
elf_fix_phdrs(Elf_Ehdr *eh, Elf_Phdr *phdr)
{
	int i;

	/* nothing to do */
	if (eh->e_ident[EI_DATA] == ELF_TARG_DATA)
		return (0);

	for (i = eh->e_phnum; i--; phdr++) {
		phdr->p_type = swap32(phdr->p_type);
		phdr->p_flags = swap32(phdr->p_flags);
		phdr->p_offset = swap_off(phdr->p_offset);
		phdr->p_vaddr = swap_addr(phdr->p_vaddr);
		phdr->p_paddr = swap_addr(phdr->p_paddr);
		phdr->p_filesz = swap_xword(phdr->p_filesz);
		phdr->p_memsz = swap_xword(phdr->p_memsz);
		phdr->p_align = swap_xword(phdr->p_align);
	}

	return (1);
}

int
elf_fix_sym(Elf_Ehdr *eh, Elf_Sym *sym)
{
	/* nothing to do */
	if (eh->e_ident[EI_DATA] == ELF_TARG_DATA)
		return (0);

	sym->st_name = swap32(sym->st_name);
	sym->st_shndx = swap16(sym->st_shndx);
	sym->st_value = swap_addr(sym->st_value);
	sym->st_size = swap_xword(sym->st_size);

	return (1);
}

/*
 * Devise nlist's type from Elf_Sym.
 * XXX this task is done as well in libc and kvm_mkdb.
 */
int
elf2nlist(Elf_Sym *sym, Elf_Ehdr *eh, Elf_Shdr *shdr, char *shstr, struct nlist *np)
{
	/* extern char *stab; */
	unsigned char n_type;
	const char *sn;

	if (sym->st_shndx < eh->e_shnum)
		sn = shstr + shdr[sym->st_shndx].sh_name;
	else
		sn = "";

/* printf("%s 0x%x %s(0x%x)\n", stab + sym->st_name, sym->st_info, sn, sym->st_shndx); */
	switch(ELF_ST_TYPE(sym->st_info)) {
	case STT_NOTYPE:
		switch (sym->st_shndx) {
		case SHN_UNDEF:
			np->n_type = N_UNDF | N_EXT;
			break;
		case SHN_ABS:
			np->n_type = N_ABS;
			break;
		case SHN_COMMON:
			np->n_type = N_COMM;
			break;
		default:
			if (sym->st_shndx >= eh->e_shnum)
				np->n_type = N_COMM | N_EXT;
			else if (!strcmp(sn, ELF_TEXT))
				np->n_type = N_TEXT;
			else if (!strcmp(sn, ELF_RODATA)) {
				np->n_type = N_DATA;
				np->n_other = 'r';
			} else if (!strcmp(sn, ELF_DATA))
				np->n_type = N_DATA;
			else if (!strcmp(sn, ELF_BSS))
				np->n_type = N_BSS;
			else
				np->n_other = '?';
			break;
		}
		break;

	case STT_FUNC:
		np->n_type = N_TEXT;
		if (ELF_ST_BIND(sym->st_info) == STB_WEAK) {
			np->n_type = N_INDR;
			np->n_other = 'W';
		} else if (sym->st_shndx == SHN_UNDEF)
			np->n_type = N_UNDF | N_EXT;
		else if (strcmp(sn, ELF_TEXT))	/* XXX GNU compat */
			np->n_other = '?';
		break;

	case STT_OBJECT:
		np->n_type = N_DATA;
		if (sym->st_shndx == SHN_ABS)
			np->n_type = N_ABS;
		if (sym->st_shndx == SHN_COMMON)
			np->n_type = N_COMM;
		else if (sym->st_shndx >= eh->e_shnum)
			break;
		else if (!strcmp(sn, ELF_BSS))
			np->n_type = N_BSS;
		else if (!strcmp(sn, ELF_RODATA))
			np->n_other = 'r';
		break;

	case STT_FILE:
		np->n_type = N_FN | N_EXT;
		break;

	/* XXX how about cross-nm then ? */
#ifdef STT_PARISC_MILLI
	case STT_PARISC_MILLI:
		np->n_type = N_TEXT;
		break;
#endif
	default:
		np->n_other = '?';
		break;
	}
	if (np->n_type != N_UNDF && ELF_ST_BIND(sym->st_info) != STB_LOCAL) {
		np->n_type |= N_EXT;
		if (np->n_other)
			np->n_other = toupper(np->n_other);
	}

	return (0);
}
