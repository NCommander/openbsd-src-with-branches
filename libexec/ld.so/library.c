/*	$OpenBSD: library.c,v 1.2 2000/10/06 17:33:51 drahn Exp $ */

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

#define _DYN_LOADER

#include <sys/types.h>
#include <sys/syslimits.h>
#include <fcntl.h>
#include <nlist.h>
#include <link.h>
#include <sys/mman.h>

#include "syscall.h"
#include "archdep.h"
#include "resolve.h"

#define PFLAGS(X) ((((X) & PF_R) ? PROT_READ : 0) | \
		   (((X) & PF_W) ? PROT_WRITE : 0) | \
		   (((X) & PF_X) ? PROT_EXEC : 0))

elf_object_t * _dl_tryload_shlib(const char *libname, int type);
void _dl_build_sod(const char *name, struct sod *sodp);
char * _dl_findhint(char *name, int major, int minor, char *prefered_path);

/*
 *  Load a shared object. Search order is:
 *	If the name contains a '/' use the name exactly as is.
 *	Otherwise first check DT_RPATH paths,
 *	then try the LD_LIBRARY_PATH specification and
 *	last look in /usr/lib.
 */

elf_object_t *
_dl_load_shlib(const char *libname, elf_object_t *parent, int type)
{
	char	lp[PATH_MAX + 10];
	char	*path = lp;
	const char *pp;
	elf_object_t *object;
	struct sod sodp;
	char *hint;

	_dl_build_sod(libname, &sodp);
	if ((hint = _dl_findhint((char *)sodp.sod_name, sodp.sod_major,
		sodp.sod_minor, NULL)) != NULL)
	{
		object = _dl_tryload_shlib(hint, type);
		return(object);
		
	}

	if(_dl_strchr(libname, '/')) {
		object = _dl_tryload_shlib(libname, type);
		return(object);
	}

	/*
	 *  No '/' in name. Scan the known places, LD_LIBRARY_PATH first.
	 */
	pp = _dl_libpath;
	while(pp) {
		const char *ln = libname;

		path = lp;
		while(path < lp + PATH_MAX && *pp && *pp != ':' && *pp != ';') {
			*path++ = *pp++;
		}
		if(path != lp && *(path - 1) != '/') {	/* Insert '/' */
			*path++ = '/';
		}
		while(path < lp + PATH_MAX && (*path++ = *ln++)) {};
		if(path < lp + PATH_MAX) {
			object = _dl_tryload_shlib(lp, type);
			if(object) {
				return(object);
			}
		}
		if(*pp) {	/* Try curdir if ':' at end */
			pp++;
		}
		else {
			pp = 0;
		}
	}

	/*
	 *  Check DT_RPATH.
	 */
	pp = parent->dyn.rpath;
	while(pp) {
		const char *ln = libname;

		path = lp;
		while(path < lp + PATH_MAX && *pp && *pp != ':') {
			*path++ = *pp++;
		}
		if(*(path - 1) != '/') {/* Make sure '/' after dir path */
			*path++ = '/';
		}
		if(*pp) {		/* ':' if not end. skip over. */
			pp++;
		}
		while(path < lp + PATH_MAX && (*path++ = *ln++)) {};
		if(path < lp + PATH_MAX) {
			object = _dl_tryload_shlib(lp, type);
			if(object) {
				return(object);
			}
		}
		if(*pp) {	/* Try curdir if ':' at end */
			pp++;
		}
		else {
			pp = 0;
		}
	}

	/*
	 *  Check '/usr/lib'
	 */

	_dl_strcpy(lp, "/usr/lib/");
	path = lp + sizeof("/usr/lib/") - 1;
	while(path < lp + PATH_MAX && (*path++ = *libname++)) {};
	if(path < lp + PATH_MAX) {
		object = _dl_tryload_shlib(lp, type);
		if(object) {
			return(object);
		}
	}
	_dl_errno = DL_NOT_FOUND;
	return(0);
}

void
_dl_unload_shlib(elf_object_t *object)
{
	if(--object->refcount == 0) {
		_dl_munmap((void *)object->load_addr, object->load_size);
		_dl_remove_object(object);
	}
}


elf_object_t *
_dl_tryload_shlib(const char *libname, int type)
{
	int	libfile;
	int	i;
	char 	hbuf[4096];
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdp;
	Elf32_Dyn  *dynp = 0;
	Elf32_Addr maxva = 0;
	Elf32_Addr minva = 0x7fffffff;
	Elf32_Addr libaddr;
	Elf32_Addr loff;
	int	align = _dl_pagesz - 1;
	elf_object_t *object;

	object = _dl_lookup_object(libname);
	if(object) {
		object->refcount++;
		return(object);		/* Already loaded */
	}

	libfile = _dl_open(libname, O_RDONLY);
	if(libfile < 0) {
		_dl_errno = DL_CANT_OPEN;
		return(0);
	}

	_dl_read(libfile, hbuf, sizeof(hbuf));
	ehdr = (Elf32_Ehdr *)hbuf;
	if(_dl_strncmp(ehdr->e_ident, ELFMAG, SELFMAG) ||
	   ehdr->e_type != ET_DYN || ehdr->e_machine != MACHID) {
		_dl_close(libfile);
		_dl_errno = DL_NOT_ELF;
		return(0);
	}

	/*
	 *  Alright, we might have a winner!
	 *  Figure out how much VM space we need.
	 */

	phdp = (Elf32_Phdr *)(hbuf + ehdr->e_phoff);
	for(i = 0; i < ehdr->e_phnum; i++, phdp++) {
		switch(phdp->p_type) {
		case PT_LOAD:
			if(phdp->p_vaddr < minva) {
				minva = phdp->p_vaddr;
			}
			if(phdp->p_vaddr + phdp->p_memsz > maxva) {
				maxva = phdp->p_vaddr + phdp->p_memsz;
			}
			break;

		case PT_DYNAMIC:
			dynp = (Elf32_Dyn *)phdp->p_vaddr;
			break;

		default:
			break;
		}
	}
	minva &= ~align;
	maxva = (maxva + align) & ~(align);

	/*
	 *  We map the entire area to see that we can get the VM
	 *  space requiered. Map it unaccessible to start with.
	 */
	libaddr = (Elf32_Addr)_dl_mmap(0, maxva - minva, PROT_NONE,
					MAP_COPY|MAP_ANON, -1, 0);
	if(_dl_check_error(libaddr)) {
		_dl_printf("%s: rtld mmap failed mapping %s.\n",
				_dl_progname, libname);
		_dl_close(libfile);
		_dl_errno = DL_CANT_MMAP;
		return(0);
	}

	loff = libaddr - minva;
	phdp = (Elf32_Phdr *)(hbuf + ehdr->e_phoff);
	for(i = 0; i < ehdr->e_phnum; i++, phdp++) {
		if(phdp->p_type == PT_LOAD) {
			int res;
			char *start = (char *)(phdp->p_vaddr & ~align) + loff;
			int size  = (phdp->p_vaddr & align) + phdp->p_filesz;
			res = _dl_mmap(start, size, PFLAGS(phdp->p_flags),
					MAP_FIXED|MAP_COPY, libfile,
					phdp->p_offset & ~align);
#ifdef __powerpc__
			_dl_syncicache(start, size);
#endif
			if(_dl_check_error(res)) {
				_dl_printf("%s: rtld mmap failed mapping %s.\n",
						_dl_progname, libname);
				_dl_close(libfile);
				_dl_errno = DL_CANT_MMAP;
				_dl_munmap((void *)libaddr, maxva - minva);
				return(0);
			}
			if(phdp->p_flags & PF_W) {
				_dl_memset(start + size, 0,
					_dl_pagesz - (size & align));
				start = start + ((size + align) & ~align);
				size  = size - (phdp->p_vaddr & align);
				size  = phdp->p_memsz - size;
				res = _dl_mmap(start, size,
					       PFLAGS(phdp->p_flags),
					       MAP_FIXED|MAP_COPY|MAP_ANON,
						-1, 0);
				if(_dl_check_error(res)) {
					_dl_printf("%s: rtld mmap failed mapping %s.\n",
							_dl_progname, libname);
					_dl_close(libfile);
					_dl_errno = DL_CANT_MMAP;
					_dl_munmap((void *)libaddr, maxva - minva);
					return(0);
				}
			}
		}
	}
	_dl_close(libfile);

	dynp = (Elf32_Dyn *)((int)dynp + loff);
	object = _dl_add_object(libname, dynp, 0, type, libaddr, loff);
	if(object) {
		object->load_size = maxva - minva;	/*XXX*/
	}
	else {
		_dl_munmap((void *)libaddr, maxva - minva);
	}
	return(object);
}
