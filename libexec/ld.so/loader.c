/*	$OpenBSD: loader.c,v 1.10 2001/05/28 21:38:14 drahn Exp $ */

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

#define	_DYN_LOADER

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/exec.h>
#include <nlist.h>
#include <link.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

/*
 *  Local decls.
 */
/* static */ char *_dl_getenv(const char *var, const char **env);

/*
 *   Static vars usable after bootsrapping.
 */
static void *_dl_malloc_base;
static void *_dl_malloc_pool = 0;
static long *_dl_malloc_free = 0;

const char *_dl_progname;
int  _dl_pagesz;
int  _dl_trusted;

char *_dl_libpath;
char *_dl_preload;
char *_dl_bindnow;
char *_dl_traceld;
char *_dl_debug;
char *_dl_showmap;

struct r_debug *_dl_debug_map;
void _dl_unmaphints();

void
_dl_debug_state(void)
{
	/* Debugger stub */
}


#if 1
static inline void
put_x(unsigned int x)
{
	char string[8];
	char *pchr;
	unsigned int rem;
	int len = 0;
	string[19] = '\0';
	pchr = &string[7];
	do {
		rem = x % 16;
		x =   x / 16;
		if (rem < 10) {
		*pchr = rem + '0';
		} else  {
			*pchr = rem - 10 + 'a';
		}
		pchr--;
		len++;
	} while (len < 8);
	_dl_write(1, string, len);

}

static inline int
putstring(char *string, unsigned int off)
{
	int len = 0;
	char * str1;
	if ((unsigned long) string < 0x10000000) {
		string += off;
	}
	for ( str1 = string; len < 30 && *str1++ != '\0'; len++);
	return _dl_write(1, string, len);
	
}
static inline int
putc(char c)
{
	return _dl_write(1, &c, 1);
	
}
#endif

/*
 * Routine to walk thru all of the objects except the first (main executable).
 */

void
_dl_run_dtors(elf_object_t *object)
{
	if(_dl_debug)
		_dl_printf("doing dtors: [%s]\n", object->load_name);
	if(object->dyn.fini) {
		(*object->dyn.fini)();
	}
	if (object->next) {
		_dl_run_dtors(object->next);
	}
}
void
_dl_dtors()
{
	if(_dl_debug)
		_dl_printf("doing dtors\n");
	if (_dl_objects->next) {
		_dl_run_dtors(_dl_objects->next);
	}
}
/*
 *  This is the dynamic loader entrypoint. When entering here, depending
 *  on architecture type, the stack and registers are set up according
 *  to the architectures ABI specification. The first thing required
 *  to do is to dig out all information we need to accomplish out task.
 */
unsigned long
_dl_boot(const char **argv, const char **envp, const long loff,
	Elf_Dyn *dynp, long *dl_data)
{
	int		n;
	int		brk_addr;
	Elf_Phdr	*phdp;
	char		*us = "";
	elf_object_t	*dynobj;
	struct elf_object  *exe_obj;	/* Pointer to executable object */
	struct elf_object  *dyn_obj;	/* Pointer to executable object */
	struct r_debug * debug_map;
#ifdef __mips__
	struct r_debug	   **map_link;	/* Where to put pointer for gdb */
#endif /* __mips__ */

#if 0
_dl_printf("%p %p 0x%lx %p %p\n", argv, envp, loff, dynp, dl_data);
#endif

	/*
	 *  Get paths to various things we are going to use.
	 */

	_dl_libpath = _dl_getenv("LD_LIBRARY_PATH", envp);
	_dl_preload = _dl_getenv("LD_PRELOAD", envp);
	_dl_bindnow = _dl_getenv("LD_BIND_NOW", envp);
	_dl_traceld = _dl_getenv("LD_TRACE_LOADED_OBJECTS", envp);
	_dl_debug   = _dl_getenv("LD_DEBUG", envp);

	_dl_progname = argv[0];
	if(dl_data[AUX_pagesz] != 0) {
		_dl_pagesz = dl_data[AUX_pagesz];
	}
	else {
		_dl_pagesz = 4096;
	}
	if(_dl_debug)
		_dl_printf("rtld loading: '%s'\n", _dl_progname);

	/*
	 *  Don't allow someone to change the search paths if he runs
	 *  a suid program without credentials high enough.
	 */
	if((_dl_trusted = !_dl_suid_ok())) {	/* Zap paths if s[ug]id... */
		if(_dl_preload) {
			*_dl_preload = '\0';
		}
		if(_dl_libpath) {
			*_dl_libpath = '\0';
		}
	}

	/*
	 *  Examine the user application and set up object information.
	 */
	phdp = (Elf_Phdr *) dl_data[AUX_phdr];
	for(n = 0; n < dl_data[AUX_phnum]; n++) {
		if(phdp->p_type == PT_LOAD) {				/*XXX*/
			if(phdp->p_vaddr + phdp->p_memsz > brk_addr)	/*XXX*/
				brk_addr = phdp->p_vaddr + phdp->p_memsz;
		}							/*XXX*/
		if(phdp->p_type == PT_DYNAMIC) {
			exe_obj = _dl_add_object("", (Elf_Dyn *)phdp->p_vaddr,
						   dl_data, OBJTYPE_EXE, 0, 0);
		}
		if(phdp->p_type == PT_INTERP) {
			us = (char *)_dl_malloc(_dl_strlen((char *)phdp->p_vaddr));
			_dl_strcpy(us, (char *)phdp->p_vaddr);
		}
		phdp++;
	}

	/*
	 *  Now, pick up and 'load' all libraries requierd. Start
	 *  With the first on the list and then do whatever gets
	 *  added along the tour.
	 */

	dynobj = _dl_objects;
	while(dynobj) {
		if(_dl_debug)
			_dl_printf("examining: '%s'\n", dynobj->load_name);
		for(dynp = dynobj->load_dyn; dynp->d_tag; dynp++) {
			if(dynp->d_tag == DT_NEEDED) {
				const char *libname;
				libname = dynobj->dyn.strtab;
				libname += dynp->d_un.d_val;
				if(_dl_debug) 
					_dl_printf("needs: '%s'\n", libname);
				if(_dl_load_shlib(libname, dynobj, OBJTYPE_LIB) == 0) {
					_dl_printf("%s: can't load library '%s'\n",
						_dl_progname, libname);
					_dl_exit(4);
				}
			}
		}
		dynobj = dynobj->next;
	}

	/*
	 *  Now add the dynamic loader itself last in the object list
	 *  so we can use the _dl_ code when serving dl.... calls.
	 */

	dynp = (Elf_Dyn *)((void *)_DYNAMIC);
	dyn_obj = _dl_add_object(us, dynp, 0, OBJTYPE_LDR, dl_data[AUX_base], loff);
	dyn_obj->status |= STAT_RELOC_DONE;

	/*
	 *  Everything should be in place now for doing the relocation
	 *  and binding. Call _dl_rtld to do the job. Fingers crossed.
	 */

	_dl_rtld(_dl_objects);
	/* the first object is the executable itself, 
	 * it is responsible for running it's own ctors/dtors
	 * thus do NOT run the ctors for the executable, all of
	 * the shared libraries which follow.
	 */
	if (_dl_objects->next) {
		_dl_call_init(_dl_objects->next);
	}

	/* schedule a routine to be run at shutdown, by using atexit.
	 * cannot call atexit directly from ld.so ??
	 */
	{
		const Elf_Sym  *sym;
		Elf_Addr ooff;

		ooff = _dl_find_symbol("atexit", _dl_objects, &sym, 0, 0);
		(*(void (*)(Elf_Addr))(sym->st_value + ooff))((Elf_Addr)_dl_dtors);
	}


	/*
	 *  Finally make something to help gdb when poking around in the code.
	 */
#ifdef __powerpc__
	{
		int done = 0;
		 
		debug_map = (struct r_debug *)_dl_malloc(sizeof(*debug_map));
		debug_map->r_version = 1;
		debug_map->r_map = (struct link_map *)_dl_objects;
		debug_map->r_brk = (Elf_Addr)_dl_debug_state;
		debug_map->r_state = RT_CONSISTENT;
		debug_map->r_ldbase = loff;
		_dl_debug_map = debug_map;

		/* picks up the first object, the executable itself */
		dynobj = _dl_objects;

		for(dynp = dynobj->load_dyn; dynp->d_tag; dynp++) {
			if (dynp->d_tag == DT_DEBUG) {
				dynp->d_un.d_ptr = (Elf_Addr) debug_map;
				done = 1;
				break;
			}
		}
		if (done == 0) {
			_dl_printf("failed to mark DTDEBUG\n");
		}
	}
#endif

#ifdef __mips__
	map_link = (struct r_debug **)(exe_obj->Dyn.info[DT_MIPS_RLD_MAP - DT_LOPROC + DT_NUM]);
	if(map_link) {
		debug_map = (struct r_debug *)_dl_malloc(sizeof(*debug_map));
		debug_map->r_version = 1;
		debug_map->r_map = (struct link_map *)_dl_objects;
		debug_map->r_brk = (Elf_Addr)_dl_debug_state;
		debug_map->r_state = RT_CONSISTENT;
		debug_map->r_ldbase = loff;
		_dl_debug_map = debug_map;
		*map_link = _dl_debug_map;
	}
#endif

	_dl_debug_state();

	if(_dl_debug || _dl_traceld) {
		void _dl_show_objects(); /* remove -Wall warning */
		_dl_show_objects();
		if (_dl_debug)
			_dl_printf("dynamic loading done.\n");
	}
	_dl_unmaphints();
	if (_dl_traceld) {
		_dl_exit(0);
	}
#ifdef DL_PRINTF_DEBUG
_dl_printf("Entry point: 0x%lx\n", dl_data[AUX_entry]);
#endif

	return(dl_data[AUX_entry]);
}


void
_dl_boot_bind(const long sp, const long loff,  int argc, const char **argv,
	const char **envp, Elf_Dyn *dynamicp, long *dl_data)
{
	Elf_Dyn	*dynp;
	int		n;
	long		*stack;
	AuxInfo		*auxstack;

	struct elf_object  dynld;	/* Resolver data for the loader */

#if 0
_dl_printf("0x%lx 0x%lx %d %p %p %p %p\n", sp, loff, argc, argv, envp, dynamicp, dl_data);
 _dl_printf("%p 0x%lx 0x%lx 0x%lx 0x%lx\n", &((long *)sp)[0], ((long *)sp)[0], ((long *)sp)[1], ((long *)sp)[2], ((long *)sp)[3]);
 _dl_printf("%p 0x%lx 0x%lx 0x%lx 0x%lx\n", &((long *)sp)[4], ((long *)sp)[4], ((long *)sp)[5], ((long *)sp)[6], ((long *)sp)[7]);
 _dl_printf("%p 0x%lx 0x%lx 0x%lx 0x%lx\n", &((long *)sp)[8], ((long *)sp)[8], ((long *)sp)[9], ((long *)sp)[10], ((long *)sp)[11]);
 _dl_printf("%p 0x%lx 0x%lx 0x%lx 0x%lx\n", &((long *)sp)[12], ((long *)sp)[12], ((long *)sp)[13], ((long *)sp)[14], ((long *)sp)[15]);
 _dl_printf("%p 0x%lx 0x%lx 0x%lx 0x%lx\n", &((long *)sp)[16], ((long *)sp)[16], ((long *)sp)[17], ((long *)sp)[18], ((long *)sp)[19]);
 _dl_printf("%p 0x%lx 0x%lx 0x%lx 0x%lx\n", &((long *)sp)[20], ((long *)sp)[20], ((long *)sp)[21], ((long *)sp)[22], ((long *)sp)[23]);
 _dl_printf("%p 0x%lx 0x%lx 0x%lx 0x%lx\n", &((long *)sp)[24], ((long *)sp)[24], ((long *)sp)[25], ((long *)sp)[26], ((long *)sp)[27]);
 _dl_printf("%p 0x%lx 0x%lx 0x%lx 0x%lx\n", &((long *)sp)[28], ((long *)sp)[28], ((long *)sp)[29], ((long *)sp)[30], ((long *)sp)[31]);
 _dl_printf("%p 0x%lx 0x%lx 0x%lx 0x%lx\n", &((long *)sp)[32], ((long *)sp)[32], ((long *)sp)[33], ((long *)sp)[34], ((long *)sp)[35]);
 _dl_printf("XXX 0x%lx\n", ((long *)sp)[23]);
#endif

	/*
	 * Scan argument and environment vectors. Find dynamic
	 * data vector put after them.
	 */
#ifdef _mips_
	stack = (long *)sp;
	argc = *stack++;
	argv = (const char **)stack;
	envp = &argv[argc + 1];
#endif /* _mips_ */
	stack = (long *)envp;
	while(*stack++ != NULL) {};

	/*
	 * Dig out auxiliary data set up by exec call. Move all known
	 * tags to an indexed local table for easy access.
	 */

	auxstack = (AuxInfo *)stack;

#if 0
 _dl_printf("XXX 0x%lx\n", ((long *)sp)[23]);
_dl_printf("---\n");
#endif
	while(auxstack->au_id != AUX_null) {
#if 0
 _dl_printf("XXX %p 0x%lx\n", &((long *)sp)[23], ((long *)sp)[23]);
 _dl_printf("XXX %p 0x%lx\n", &auxstack->au_v, auxstack->au_v);
_dl_printf("%p 0x%lx 0x%lx %d 0x%lx\n", auxstack, ((long *)auxstack)[0], ((long *)auxstack)[1], auxstack->au_id, auxstack->au_v);
#endif
		if(auxstack->au_id <= AUX_entry) {
			dl_data[auxstack->au_id] = auxstack->au_v;
		}
		auxstack++;
	}

#if 0
_dl_printf("---\n");
 _dl_printf("0x%lx 0x%lx 0x%lx 0x%lx\n", dl_data[0], dl_data[1], dl_data[2], dl_data[3]);
 _dl_printf("0x%lx 0x%lx 0x%lx 0x%lx\n", dl_data[4], dl_data[5], dl_data[6], dl_data[7]);
 _dl_printf("0x%lx 0x%lx 0x%lx 0x%lx\n", dl_data[8], dl_data[9], dl_data[10], dl_data[11]);
#endif

	/*
	 *  We need to do 'selfreloc' in case the code were'nt
	 *  loaded at the address it was linked to.
	 *
	 *  Scan the DYNAMIC section for the loader.
	 *  Cache the data for easier access.
	 */

#if defined(__powerpc__) || defined(__alpha__)
	dynp = dynamicp;
#else
	dynp = (Elf_Dyn *)((long)_DYNAMIC + loff);
#endif
	while(dynp != NULL && dynp->d_tag != DT_NULL) {
		if(dynp->d_tag < DT_LOPROC) {
			dynld.Dyn.info[dynp->d_tag] = dynp->d_un.d_val;
		}
		else if(dynp->d_tag >= DT_LOPROC && dynp->d_tag < DT_LOPROC + DT_NUM) {
			dynld.Dyn.info[dynp->d_tag + DT_NUM - DT_LOPROC] = dynp->d_un.d_val;
		}
		if(dynp->d_tag == DT_TEXTREL)
			dynld.dyn.textrel = 1;
		dynp++;
	}

	/*
	 *  Do the 'bootstrap relocation'. This is really only needed if
	 *  the code was loaded at another location than it was linked to.
	 *  We don't do undefined symbols resolving (to difficult..)
	 */

	/* "relocate" dyn.X values if they represent addresses */
	{
		int i, val;
		/* must be code, not pic data */
		int table[20]; 
		i = 0;
		table[i++] = DT_PLTGOT;
		table[i++] = DT_HASH;
		table[i++] = DT_STRTAB;
		table[i++] = DT_SYMTAB;
		table[i++] = DT_RELA;
		table[i++] = DT_INIT;
		table[i++] = DT_FINI;
		table[i++] = DT_REL;
		table[i++] = DT_JMPREL;
		/* other processors insert there extras here */
		table[i++] = DT_NULL;
#if 0
		= {
		DT_PLTGOT,
		DT_HASH,
		DT_STRTAB,
		DT_SYMTAB,
		DT_RELA,
		DT_INIT,
		DT_FINI,
		DT_REL,
		DT_JMPREL,
		/* other processors insert there extras here */
		DT_NULL
		};
#endif
		for (i = 0; table[i] != DT_NULL; i++)
		{
			val = table[i];
			if ( val > DT_HIPROC) {
				/* ??? */
				continue;
			}
			if ( val > DT_LOPROC) {
				val -= DT_LOPROC + DT_NUM;
			}
			if ( dynld.Dyn.info[val] != 0 ) {
				dynld.Dyn.info[val] += loff;
			}
		}

	}

	{
		int	  i;
		u_int32_t rs;
		Elf_Rel  *rp;

		rp = (Elf_Rel *)(dynld.Dyn.info[DT_REL]);
		rs = dynld.dyn.relsz;

		for(i = 0; i < rs; i += sizeof (Elf_Rel)) {
			Elf_Addr *ra;
			const Elf_Sym  *sp;

			sp = dynld.dyn.symtab;
			sp += ELF_R_SYM(rp->r_info);
#if 1
			putstring("reloc  ", loff);
			putstring(((char *)dynld.dyn.strtab) + sp->st_name, 0);
			putstring(" ", loff);
#endif

			if(ELF_R_SYM(rp->r_info) && sp->st_value == 0) {
#if 0
/* cannot printf in this function */
				_dl_wrstderr("Dynamic loader failure: self bootstrapping impossible.\n");
				_dl_wrstderr("Undefined symbol: ");
				_dl_wrstderr((char *)dynld.dyn.strtab
					+ sp->st_name);
#endif
				_dl_exit(5);
			}

			ra = (Elf_Addr *)(rp->r_offset + loff);
#if 0
			put_x((unsigned int)ra);
			putstring("\n", loff);
#endif
			/*
			RELOC_REL(rp, sp, ra, loff);
			*/
			rp++;
		}

	}
	for(n = 0; n < 2; n++) {
		int	  i;
		unsigned long rs;
		Elf_RelA  *rp;

		switch (n) {
		case 0:
			rp = (Elf_RelA *)(dynld.Dyn.info[DT_JMPREL]);
			rs = dynld.dyn.pltrelsz;
			break;
		case 1:
			rp = (Elf_RelA *)(dynld.Dyn.info[DT_RELA]);
			rs = dynld.dyn.relasz;

			break;
		default:
			rp = NULL;
			rs = 0;
			;
		}
		for(i = 0; i < rs; i += sizeof (Elf_RelA)) {
			Elf_Addr *ra;
			const Elf_Sym  *sp;

			sp = dynld.dyn.symtab;
			sp += ELF_R_SYM(rp->r_info);
			if(ELF_R_SYM(rp->r_info) && sp->st_value == 0) {
#if 0
				_dl_wrstderr("Dynamic loader failure: self bootstrapping impossible.\n");
				_dl_wrstderr("Undefined symbol: ");
				_dl_wrstderr((char *)dynld.dyn.strtab
					+ sp->st_name);
#endif
				_dl_exit(6);
			}

			ra = (Elf_Addr *)(rp->r_offset + loff);

			RELOC_RELA(rp, sp, ra, loff);
			/*
			*/

			/*
			*/
			rp++;
		}
	}
	/* we have been fully relocated here, so most things no longer
	 * need the loff adjustment
	 */
	return;
}


void
_dl_rtld(elf_object_t *object)
{
	if(object->next) {
		_dl_rtld(object->next);
	}

	/*
	 *  Do relocation information first, then GOT.
	 */
	_dl_md_reloc(object, DT_REL, DT_RELSZ);
	_dl_md_reloc(object, DT_RELA, DT_RELASZ);
#if defined(__alpha__)
	_dl_md_reloc(object, DT_JMPREL, DT_PLTRELSZ);
#endif
	if(_dl_bindnow) {	/* XXX Perhaps more checking ? */
		_dl_md_reloc_got(object, 1);
	} else {
		_dl_md_reloc_got(object, 0);
	}
	object->status |= STAT_RELOC_DONE;
}

void
_dl_call_init(elf_object_t *object)
{
	Elf_Addr ooff;
	const Elf_Sym  *sym;
	static void (*_dl_atexit)(Elf_Addr) = NULL;

	if(object->next) {
		_dl_call_init(object->next);
	}

	if(object->status & STAT_INIT_DONE) {
		return;
	}


#ifndef __mips__
	if(object->dyn.init) {
		(*object->dyn.init)();
	}
#endif
#ifdef __mips__
/* XXX We perform relocation of DTOR/CTOR. This is a ld bug problem
 * XXX that should be fixed.
 */
	sym = 0;
	ooff = _dl_find_symbol("__CTOR_LIST__", object, &sym, 1, 1);
	if(sym) {
		int i = *(int *)(sym->st_value + ooff);
		while(i--) {
			*(int *)(sym->st_value + ooff + 4 + 4 * i) += ooff;
		}
	}
	sym = 0;
	ooff = _dl_find_symbol("__DTOR_LIST__", object, &sym, 1, 1);
	if(sym) {
		int i = *(int *)(sym->st_value + ooff);
		while(i--) {
			*(int *)(sym->st_value + ooff + 4 + 4 * i) += ooff;
		}
	}

/* XXX We should really call any code which resides in the .init segment
 * XXX but at the moment this functionality is not provided by the toolchain.
 * XXX Instead we rely on a symbol named '.init' and call it if it exists.
 */
	sym = 0;
	ooff = _dl_find_symbol(".init", object, &sym, 1, 1);
	if(sym) {
		if(_dl_debug)
			_dl_printf("calling .init in '%s'\n",object->load_name);
		(*(void(*)(void))(sym->st_value + ooff))();
	}
#if 0 /*XXX*/
	if(object->dyn.init) {
		(*object->dyn.init)();
	}
#endif
#endif /* __mips__ */
	object->status |= STAT_INIT_DONE;
}

/* static */ char *
_dl_getenv(const char *var, const char **env)
{
	const char *ep;

	while((ep = *env++)) {
		const char *vp = var;
		while(*vp && *vp == *ep) {
			vp++;
			ep++;
		}
		if(*vp == '\0' && *ep++ == '=') {
			return((char *)ep);
		}
	}
	return(0);
}


/*
 *  The following malloc/free code is a very simplified implementation
 *  of a malloc function. However, we do not need to be very complex here
 *  because we only free memory when 'dlclose()' is called and we can
 *  reuse at least the memory allocated for the object descriptor. We have
 *  one dynamic string allocated, the library name and it is likely that
 *  we can reuse that one to without a lot of complex colapsing code.
 */

void *
_dl_malloc(int size)
{
	long *p;
	long *t, *n;

	size = (size + 8 + DL_MALLOC_ALIGN - 1) & ~(DL_MALLOC_ALIGN - 1);

	if((t = _dl_malloc_free) != 0) {	/* Try free list first */
		n = (long *)&_dl_malloc_free;
		while(t && t[-1] < size) {
			n = t;
			t = (long *)*t;
		}
		if(t) {
			*n = *t;
			_dl_memset(t, 0, t[-1] - 4);
			return((void *)t);
		}
	}
	if((_dl_malloc_pool == 0) ||
	   (_dl_malloc_pool + size > _dl_malloc_base + 4096)) {
		_dl_malloc_pool = (void *)_dl_mmap((void *)0, 4096,
						PROT_READ|PROT_WRITE,
						MAP_ANON|MAP_PRIVATE, -1, 0);
		if(_dl_malloc_pool == 0 ||
			_dl_malloc_pool == MAP_FAILED )
		{
			_dl_printf("Dynamic loader failure: malloc.\n");
			_dl_exit(7);
		}
		_dl_malloc_base = _dl_malloc_pool;
	}
	p = _dl_malloc_pool;
	_dl_malloc_pool += size;
	_dl_memset(p, 0, size);
	*p = size;
	return((void *)(p + 1));
}

void
_dl_free(void *p)
{
	long *t = (long *)p;

	*t = (long)_dl_malloc_free;
	_dl_malloc_free = p;
}
