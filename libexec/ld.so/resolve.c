/*	$OpenBSD: resolve.c,v 1.24 2004/07/05 00:47:40 kjell Exp $ */

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

#define _DYN_LOADER

#include <sys/types.h>

#include <nlist.h>
#include <link.h>
#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

elf_object_t *_dl_objects;
elf_object_t *_dl_last_object;

/*
 * Add a new dynamic object to the object list.
 */
void
_dl_add_object(elf_object_t *object)
{

	/*
	 * if this is a new object, prev will be NULL
	 * != NULL if an object already in the list
	 * prev == NULL for the first item in the list, but that will
	 * be the executable.
	 */
	if (object->prev != NULL)
		return;

	if (_dl_objects == NULL) {			/* First object ? */
		_dl_last_object = _dl_objects = object;
	} else {
		_dl_last_object->next = object;
		object->prev = _dl_last_object;
		_dl_last_object = object;
	}
}

/*
 * Initialize a new dynamic object.
 */
elf_object_t *
_dl_finalize_object(const char *objname, Elf_Dyn *dynp, const u_long *dl_data,
    const int objtype, const long laddr, const long loff)
{
	elf_object_t *object;
#if 0
	_dl_printf("objname [%s], dynp %p, dl_data %p, objtype %x laddr %lx, loff %lx\n",
	    objname, dynp, dl_data, objtype, laddr, loff);
#endif
	object = _dl_malloc(sizeof(elf_object_t));
	object->prev = object->next = NULL;

	object->load_dyn = dynp;
	while (dynp->d_tag != DT_NULL) {
		if (dynp->d_tag < DT_NUM)
			object->Dyn.info[dynp->d_tag] = dynp->d_un.d_val;
		else if (dynp->d_tag >= DT_LOPROC &&
		    dynp->d_tag < DT_LOPROC + DT_NUM)
			object->Dyn.info[dynp->d_tag + DT_NUM - DT_LOPROC] =
			    dynp->d_un.d_val;
		if (dynp->d_tag == DT_TEXTREL)
			object->dyn.textrel = 1;
		if (dynp->d_tag == DT_SYMBOLIC)
			object->dyn.symbolic = 1;
		if (dynp->d_tag == DT_BIND_NOW)
			object->dyn.bind_now = 1;
		dynp++;
	}

	/*
	 *  Now relocate all pointer to dynamic info, but only
	 *  the ones which have pointer values.
	 */
	if (object->Dyn.info[DT_PLTGOT])
		object->Dyn.info[DT_PLTGOT] += loff;
	if (object->Dyn.info[DT_HASH])
		object->Dyn.info[DT_HASH] += loff;
	if (object->Dyn.info[DT_STRTAB])
		object->Dyn.info[DT_STRTAB] += loff;
	if (object->Dyn.info[DT_SYMTAB])
		object->Dyn.info[DT_SYMTAB] += loff;
	if (object->Dyn.info[DT_RELA])
		object->Dyn.info[DT_RELA] += loff;
	if (object->Dyn.info[DT_SONAME])
		object->Dyn.info[DT_SONAME] += loff;
	if (object->Dyn.info[DT_RPATH])
		object->Dyn.info[DT_RPATH] += object->Dyn.info[DT_STRTAB];
	if (object->Dyn.info[DT_REL])
		object->Dyn.info[DT_REL] += loff;
	if (object->Dyn.info[DT_INIT])
		object->Dyn.info[DT_INIT] += loff;
	if (object->Dyn.info[DT_FINI])
		object->Dyn.info[DT_FINI] += loff;
	if (object->Dyn.info[DT_JMPREL])
		object->Dyn.info[DT_JMPREL] += loff;

	if (object->Dyn.info[DT_HASH] != 0) {
		Elf_Word *hashtab = (Elf_Word *)object->Dyn.info[DT_HASH];

		object->nbuckets = hashtab[0];
		object->nchains = hashtab[1];
		object->buckets = hashtab + 2;
		object->chains = object->buckets + object->nbuckets;
	}

	if (dl_data) {
		object->phdrp = (Elf_Phdr *) dl_data[AUX_phdr];
		object->phdrc = dl_data[AUX_phnum];
	}
	object->obj_type = objtype;
	object->load_addr = laddr;
	object->load_offs = loff;
	object->load_name = _dl_strdup(objname);
	object->refcount = 1;
	object->first_child = NULL;
	object->last_child = NULL;
	/* default dev, inode for dlopen-able objects. */
	object->dev = 0;
	object->inode = 0;

	return(object);
}

void
_dl_remove_object(elf_object_t *object)
{
	elf_object_t *depobj;

	object->prev->next = object->next;
	if (object->next)
		object->next->prev = object->prev;

	if (_dl_last_object == object)
		_dl_last_object = object->prev;

	if (object->load_name)
		_dl_free(object->load_name);

	while ((depobj = object->dep_next)) {
		object->dep_next = object->dep_next->dep_next;
		_dl_free(depobj);
	}
	_dl_free(object);
}


elf_object_t *
_dl_lookup_object(const char *name)
{
	elf_object_t *object;

	object = _dl_objects;
	while (object) {
		if (_dl_strcmp(name, object->load_name) == 0)
			return(object);
		object = object->next;
	}
	return(0);
}

int find_symbol_obj(elf_object_t *object, const char *name, unsigned long hash,
    int flags, const Elf_Sym **ref, const Elf_Sym **weak_sym,
    elf_object_t **weak_object);

sym_cache *_dl_symcache;
int _dl_symcachestat_hits;
int _dl_symcachestat_lookups;

Elf_Addr
_dl_find_symbol_bysym(elf_object_t *req_obj, unsigned int symidx,
    elf_object_t *startlook, const Elf_Sym **ref, const elf_object_t **pobj,
    int flags, int req_size)
{
	Elf_Addr ret;
	const Elf_Sym *sym;
	const char *symn;
	const elf_object_t *sobj;

	_dl_symcachestat_lookups ++;
	if ((_dl_symcache != NULL) &&
	     (symidx < req_obj->nchains) &&
	     (_dl_symcache[symidx].obj != NULL) &&
	     (_dl_symcache[symidx].sym != NULL) &&
	     (_dl_symcache[symidx].flags == flags)) {

		_dl_symcachestat_hits++;
		sobj = _dl_symcache[symidx].obj;
		*ref = _dl_symcache[symidx].sym;
		if (pobj)
			*pobj = sobj;
		return sobj->load_offs;
	}

	sym = req_obj->dyn.symtab;
	sym += symidx;
	symn = req_obj->dyn.strtab + sym->st_name;

	ret = _dl_find_symbol(symn, startlook, ref, &sobj,
	    flags, req_size, req_obj);

	if (pobj)
		*pobj = sobj;

	if ((_dl_symcache != NULL) &&
	     (symidx < req_obj->nchains)) {
		_dl_symcache[symidx].sym = *ref;
		_dl_symcache[symidx].obj = sobj;
		_dl_symcache[symidx].flags = flags;
	}

	return ret;
}

Elf_Addr
_dl_find_symbol(const char *name, elf_object_t *startlook,
    const Elf_Sym **ref, const elf_object_t **pobj,
    int flags, int req_size, elf_object_t *req_obj)
{
	const Elf_Sym *weak_sym = NULL;
	unsigned long h = 0;
	const char *p = name;
	elf_object_t *object, *weak_object = NULL;
	int found = 0;
	int lastchance = 0;

	while (*p) {
		unsigned long g;
		h = (h << 4) + *p++;
		if ((g = h & 0xf0000000))
			h ^= g >> 24;
		h &= ~g;
	}

	if (req_obj->dyn.symbolic)
		if (find_symbol_obj(req_obj, name, h, flags, ref, &weak_sym,
		    &weak_object)) {
			object = req_obj;
			found = 1;
			goto found;
		}

retry_nonglobal_dlo:
	for (object = startlook; object;
	    object = ((flags & SYM_SEARCH_SELF) ? 0 : object->next)) {

		if ((lastchance == 0) &&
		    ((object->obj_flags & RTLD_GLOBAL) == 0) &&
		    (object->obj_type == OBJTYPE_DLO) &&
		    (object != req_obj))
			continue;

		if (find_symbol_obj(object, name, h, flags, ref, &weak_sym,
		    &weak_object)) {
			found = 1;
			break;
		}
	}

found:
	if (weak_object != NULL && found == 0) {
		object=weak_object;
		*ref = weak_sym;
		found = 1;
	}

	if (found == 0) {
		if (lastchance == 0) {
			lastchance = 1;
			goto retry_nonglobal_dlo;
		}
		if (flags & SYM_WARNNOTFOUND)
			_dl_printf("%s:%s: undefined symbol '%s'\n",
			    _dl_progname, req_obj->load_name, name);
		return (0);
	}

	if (req_size != (*ref)->st_size && req_size != 0 &&
	    (ELF_ST_TYPE((*ref)->st_info) != STT_FUNC)) {
		_dl_printf("%s:%s: %s : WARNING: "
		    "symbol(%s) size mismatch, relink your program\n",
		    _dl_progname, req_obj->load_name,
		    object->load_name, name);
	}

	if (pobj)
		*pobj = object;

	return (object->load_offs);
}

int
find_symbol_obj(elf_object_t *object, const char *name, unsigned long hash,
    int flags, const Elf_Sym **ref, const Elf_Sym **weak_sym,
    elf_object_t **weak_object)
{
	const Elf_Sym	*symt = object->dyn.symtab;
	const char	*strt = object->dyn.strtab;
	long	si;
	const char *symn;

	for (si = object->buckets[hash % object->nbuckets];
	    si != STN_UNDEF; si = object->chains[si]) {
		const Elf_Sym *sym = symt + si;

		if (sym->st_value == 0)
			continue;

		if (ELF_ST_TYPE(sym->st_info) != STT_NOTYPE &&
		    ELF_ST_TYPE(sym->st_info) != STT_OBJECT &&
		    ELF_ST_TYPE(sym->st_info) != STT_FUNC)
			continue;

		symn = strt + sym->st_name;
		if (sym != *ref && _dl_strcmp(symn, name))
			continue;

		/* allow this symbol if we are referring to a function
		 * which has a value, even if section is UNDEF.
		 * this allows &func to refer to PLT as per the
		 * ELF spec. st_value is checked above.
		 * if flags has SYM_PLT set, we must have actual
		 * symbol, so this symbol is skipped.
		 */
		if (sym->st_shndx == SHN_UNDEF) {
			if ((flags & SYM_PLT) || sym->st_value == 0 ||
			    ELF_ST_TYPE(sym->st_info) != STT_FUNC)
				continue;
		}

		if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL) {
			*ref = sym;
			return 1;
		} else if (ELF_ST_BIND(sym->st_info) == STB_WEAK) {
			if (!*weak_sym) {
				*weak_sym = sym;
				*weak_object = object;
			}
		}
	}
	return 0;
}
