/*	$OpenBSD: archdep.h,v 1.3 2001/09/24 23:40:10 art Exp $ */

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

#ifndef _SPARC64_ARCHDEP_H_
#define _SPARC64_ARCHDEP_H_

#define	DL_MALLOC_ALIGN	8	/* Arch constraint or otherwise */

#define	MACHID	EM_SPARCV9	/* ELF e_machine ID value checked */

#define	RELTYPE	Elf64_Rela
#define	RELSIZE	sizeof(Elf64_Rela)

#include <elf_abi.h>
#include <machine/exec.h>
#include <machine/reloc.h>
#include <sys/syscall.h>

int	_dl_write __P((int, const char *, int));

/*
 *	The following functions are declared inline so they can
 *	be used before bootstrap linking has been finished.
 */
static inline void
_dl_wrstderr(const char *s)
{
	while(*s) {
		_dl_write(2, s, 1);
		s++;
	}
}

static inline void *
_dl_memset(void *p, const char v, size_t c)
{
	char *ip = p;

	while(c--)
		*ip++ = v;
	return(p);
}

static inline int
_dl_strlen(const char *p)
{
	const char *s = p;

	while(*s != '\0')
		s++;
	return(s - p);
}

static inline char *
_dl_strcpy(char *d, const char *s)
{
	char *rd = d;

	while((*d++ = *s++) != '\0');

	return(rd);
}

static inline int
_dl_strncmp(const char *d, const char *s, int c)
{
	while(c-- && *d && *d == *s) {
		d++;
		s++;
	};
	if(c < 0) {
		return(0);
	}
	return(*d - *s);
}
 
static inline int
_dl_strcmp(const char *d, const char *s)
{
	while(*d && *d == *s) {
		d++;
		s++;
	}
	return(*d - *s);
}
 
static inline const char *
_dl_strchr(const char *p, const int c)
{
	while(*p) {
		if(*p == c) {
			return(p);
		}
		p++;
	}
	return(0);
}

static inline long
_dl_mmap(void *addr, unsigned int len, unsigned int prot,
	unsigned int flags, int fd, off_t offset)
{ 
	return(_dl___syscall((quad_t)SYS_mmap, addr, len, prot,
		flags, fd, 0, offset));
}

static inline void
RELOC_RELA(Elf_RelA *r, const Elf_Sym *s, Elf_Addr *p, unsigned long v)
{
	if (ELF_R_TYPE(r->r_info) == RELOC_RELATIVE) {
		*p += (Elf_Addr)v;
	} else {
		/* XXX - printf might not work here, but we give it a shot. */
		_dl_printf("Unkown bootstrap relocation.\n");
		_dl_exit(6);
	}
}

#endif /* _SPARC64_ARCHDEP_H_ */
