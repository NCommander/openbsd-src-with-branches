/*	$OpenBSD$ */

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

#ifndef _RESOLVE_H_
#define _RESOLVE_H_

#include <link.h>


/*
 *  Structure describing a loaded object.
 *  The head of this struct must be compatible
 *  with struct link_map in sys/link.h
 */
typedef struct elf_object {
	Elf32_Addr load_addr;		/* Real load address */
	Elf32_Addr load_offs;		/* Load offset from link address */
	char	   *load_name;		/* Pointer to object name */
	Elf32_Dyn  *load_dyn;		/* Pointer to object dynamic data */
	struct elf_object *next;
	struct elf_object *prev;
/* End struct link_map compatible */

	u_int32_t  load_size;

	union {
		u_int32_t	info[DT_NUM + DT_PROCNUM];
		struct {
			Elf32_Word	null;		/* Not used */
			Elf32_Word	needed;		/* Not used */
			Elf32_Word	pltrelsz;
			Elf32_Word	*pltgot;
			Elf32_Word	*hash;
			const char	*strtab;
			const Elf32_Sym	*symtab;
			Elf32_Rela	*rela;
			Elf32_Word	relasz;
			Elf32_Word	relaent;
			Elf32_Word	strsz;
			Elf32_Word	syment;
			void		(*init)(void);
			void		(*fini)(void);
			const char	*soname;
			const char	*rpath;
			Elf32_Word	symbolic;
			Elf32_Rel	*rel;
			Elf32_Word	relsz;
			Elf32_Word	relent;
			Elf32_Word	pltrel;
			Elf32_Word	debug;
			Elf32_Word	textrel;
			Elf32_Word	jmprel;
			Elf32_Word	bind_now;
		} u;
	} Dyn;
#define dyn Dyn.u

	struct elf_object *dep_next;	/* Shadow objects for resolve search */

	int		status;
#define	STAT_RELOC_DONE	1
#define	STAT_GOT_DONE	2
#define	STAT_INIT_DONE	4

	Elf32_Phdr	*phdrp;
	int		phdrc;

	int		refcount;
	int		obj_type;
#define	OBJTYPE_LDR	1
#define	OBJTYPE_EXE	2
#define	OBJTYPE_LIB	3
#define	OBJTYPE_DLO	4

	u_int32_t	*buckets;
	u_int32_t	nbuckets;
	u_int32_t	*chains;
	u_int32_t	nchains;
	Elf32_Dyn	*dynamic;

} elf_object_t;

extern void _dl_rt_resolve(void);

extern elf_object_t *_dl_add_object(const char *objname, Elf32_Dyn *dynp,
				    const u_int32_t *, const int objtype,
				    const int laddr, const int loff);
extern void         _dl_remove_object(elf_object_t *object);

extern elf_object_t *_dl_lookup_object(const char *objname);
extern elf_object_t *_dl_load_shlib(const char *, elf_object_t *, int);
extern void         _dl_unload_shlib(elf_object_t *object);

extern int  _dl_md_reloc(elf_object_t *object, int rel, int relsz);
extern void _dl_md_reloc_got(elf_object_t *object, int lazy);

void * _dl_malloc(const int size);

void _dl_rtld(elf_object_t *object);
void _dl_call_init(elf_object_t *object);

extern elf_object_t *_dl_objects;
extern elf_object_t *_dl_last_object;

extern const char *_dl_progname;
extern struct r_debug *_dl_debug_map;

extern int  _dl_pagesz;
extern int  _dl_trusted;
extern int  _dl_errno;

extern char *_dl_libpath;
extern char *_dl_preload;
extern char *_dl_bindnow;
extern char *_dl_traceld;
extern char *_dl_debug;

#define	DL_NOT_FOUND		1
#define	DL_CANT_OPEN		2
#define	DL_NOT_ELF		3
#define	DL_CANT_OPEN_REF	4
#define	DL_CANT_MMAP		5
#define	DL_NO_SYMBOL		6
#define	DL_INVALID_HANDLE	7
#define	DL_INVALID_CTL		8

#endif /* _RESOLVE_H_ */
