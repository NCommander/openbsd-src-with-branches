/*	$OpenBSD: library_subr.c,v 1.1 2005/03/23 19:48:05 drahn Exp $ */

/*
 * Copyright (c) 2002 Dale Rahn
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
#include <sys/syslimits.h>
#include <sys/param.h>
#include <dirent.h>

#include "archdep.h"
#include "resolve.h"
#include "dir.h"
#include "sod.h"

#define DEFAULT_PATH "/usr/lib"

/*
 * _dl_match_file()
 *
 * This fucntion determines if a given name matches what is specified
 * in a struct sod. The major must match exactly, and the minor must
 * be same or larger.
 *
 * sodp is updated with the minor if this matches.
 */

int
_dl_match_file(struct sod *sodp, char *name, int namelen)
{
	int match;
	struct sod lsod;
	char *lname;

	lname = name;
	if (sodp->sod_library) {
		if (_dl_strncmp(name, "lib", 3))
			return 0;
		lname += 3;
	}
	if (_dl_strncmp(lname, (char *)sodp->sod_name,
	    _dl_strlen((char *)sodp->sod_name)))
		return 0;

	_dl_build_sod(name, &lsod);

	match = 0;
	if ((_dl_strcmp((char *)lsod.sod_name, (char *)sodp->sod_name) == 0) &&
	    (lsod.sod_library == sodp->sod_library) &&
	    ((sodp->sod_major == -1) || (sodp->sod_major == lsod.sod_major)) &&
	    ((sodp->sod_minor == -1) ||
	    (lsod.sod_minor >= sodp->sod_minor))) {
		match = 1;

		/* return version matched */
		sodp->sod_major = lsod.sod_major;
		sodp->sod_minor = lsod.sod_minor;
	}
	_dl_free((char *)lsod.sod_name);
	return match;
}

char _dl_hint_store[MAXPATHLEN];

char *
_dl_find_shlib(struct sod *sodp, const char *searchpath, int nohints)
{
	char *hint, lp[PATH_MAX + 10], *path;
	struct dirent *dp;
	const char *pp;
	int match, len;
	DIR *dd;
	struct sod tsod, bsod;		/* transient and best sod */

	/* if we are to search default directories, and hints
	 * are not to be used, search the standard path from ldconfig
	 * (_dl_hint_search_path) or use the default path
	 */
	if (nohints)
		goto nohints;

	if (searchpath == NULL) {
		/* search 'standard' locations, find any match in the hints */
		hint = _dl_findhint((char *)sodp->sod_name, sodp->sod_major,
		    sodp->sod_minor, NULL);
		if (hint)
			return hint;
	} else {
		/* search hints requesting matches for only
		 * the searchpath directories,
		 */
		pp = searchpath;
		while (pp) {
			path = lp;
			while (path < lp + PATH_MAX &&
			    *pp && *pp != ':' && *pp != ';')
				*path++ = *pp++;
			*path = 0;

			/* interpret "" as curdir "." */
			if (lp[0] == '\0') {
				lp[0] = '.';
				lp[1] = '\0';
			}

			hint = _dl_findhint((char *)sodp->sod_name,
			    sodp->sod_major, sodp->sod_minor, lp);
			if (hint != NULL)
				return hint;

			if (*pp)	/* Try curdir if ':' at end */
				pp++;
			else
				pp = 0;
		}
	}

	/*
	 * For each directory in the searchpath, read the directory
	 * entries looking for a match to sod. filename compare is
	 * done by _dl_match_file()
	 */
nohints:
	if (searchpath == NULL) {
		if (_dl_hint_search_path != NULL)
			searchpath = _dl_hint_search_path;
		else
			searchpath = DEFAULT_PATH;
	}
	pp = searchpath;
	while (pp) {
		path = lp;
		while (path < lp + PATH_MAX && *pp && *pp != ':' && *pp != ';')
			*path++ = *pp++;
		*path = 0;

		/* interpret "" as curdir "." */
		if (lp[0] == '\0') {
			lp[0] = '.';
			lp[1] = '\0';
		}

		if ((dd = _dl_opendir(lp)) != NULL) {
			match = 0;
			while ((dp = _dl_readdir(dd)) != NULL) {
				tsod = *sodp;
				if (_dl_match_file(&tsod, dp->d_name,
				    dp->d_namlen)) {
					/*
					 * When a match is found, tsod is
					 * updated with the major+minor found.
					 * This version is compared with the
					 * largest so far (kept in bsod),
					 * and saved if larger.
					 */
					if (!match ||
					    tsod.sod_major == -1 ||
					    tsod.sod_major > bsod.sod_major ||
					    ((tsod.sod_major ==
					    bsod.sod_major) &&
					    tsod.sod_minor > bsod.sod_minor)) {
						bsod = tsod;
						match = 1;
						len = _dl_strlcpy(
						    _dl_hint_store, lp,
						    MAXPATHLEN);
						if (lp[len-1] != '/') {
							_dl_hint_store[len] =
							    '/';
							len++;
						}
						_dl_strlcpy(
						    &_dl_hint_store[len],
						    dp->d_name,
						    MAXPATHLEN-len);
						if (tsod.sod_major == -1)
							break;
					}
				}
			}
			_dl_closedir(dd);
			if (match) {
				*sodp = bsod;
				return (_dl_hint_store);
			}
		}

		if (*pp)	/* Try curdir if ':' at end */
			pp++;
		else
			pp = 0;
	}
	return NULL;
}

/*
 *  Load a shared object. Search order is:
 *	If the name contains a '/' use the name exactly as is. (only)
 *	try the LD_LIBRARY_PATH specification (if present)
 *	   search hints for match in LD_LIBRARY_PATH dirs
 *           this will only match specific libary version.
 *	   search LD_LIBRARY_PATH dirs for match.
 *           this will find largest minor version in first dir found.
 *	check DT_RPATH paths, (if present)
 *	   search hints for match in DT_RPATH dirs
 *           this will only match specific libary version.
 *	   search DT_RPATH dirs for match.
 *           this will find largest minor version in first dir found.
 *	last look in default search directory, either as specified
 *      by ldconfig or default to '/usr/lib'
 */

elf_object_t *
_dl_load_shlib(const char *libname, elf_object_t *parent, int type, int flags)
{
	int try_any_minor, ignore_hints;
	struct sod sod, req_sod;
	elf_object_t *object;
	char *hint;

	try_any_minor = 0;
	ignore_hints = 0;

	if (_dl_strchr(libname, '/')) {
		object = _dl_tryload_shlib(libname, type);
		return(object);
	}

	_dl_build_sod(libname, &sod);
	req_sod = sod;

again:
	/*
	 *  No '/' in name. Scan the known places, LD_LIBRARY_PATH first.
	 */
	if (_dl_libpath != NULL) {
		hint = _dl_find_shlib(&req_sod, _dl_libpath, ignore_hints);
		if (hint != NULL) {
			if (req_sod.sod_minor < sod.sod_minor)
				_dl_printf("warning: lib%s.so.%d.%d: "
				    "minor version >= %d expected, "
				    "using it anyway\n",
				    sod.sod_name, sod.sod_major,
				    req_sod.sod_minor, sod.sod_minor);
			object = _dl_tryload_shlib(hint, type);
			if (object != NULL) {
				_dl_free((char *)sod.sod_name);
				object->obj_flags = flags;
				return (object);
			}
		}
	}

	/*
	 *  Check DT_RPATH.
	 */
	if (parent->dyn.rpath != NULL) {
		hint = _dl_find_shlib(&req_sod, parent->dyn.rpath,
		    ignore_hints);
		if (hint != NULL) {
			if (req_sod.sod_minor < sod.sod_minor)
				_dl_printf("warning: lib%s.so.%d.%d: "
				    "minor version >= %d expected, "
				    "using it anyway\n",
				    sod.sod_name, sod.sod_major,
				    req_sod.sod_minor, sod.sod_minor);
			object = _dl_tryload_shlib(hint, type);
			if (object != NULL) {
				_dl_free((char *)sod.sod_name);
				object->obj_flags = flags;
				return (object);
			}
		}
	}

	/* check 'standard' locations */
	hint = _dl_find_shlib(&req_sod, NULL, ignore_hints);
	if (hint != NULL) {
		if (req_sod.sod_minor < sod.sod_minor)
			_dl_printf("warning: lib%s.so.%d.%d: "
			    "minor version >= %d expected, "
			    "using it anyway\n",
			    sod.sod_name, sod.sod_major,
			    req_sod.sod_minor, sod.sod_minor);
		object = _dl_tryload_shlib(hint, type);
		if (object != NULL) {
			_dl_free((char *)sod.sod_name);
			object->obj_flags = flags;
			return(object);
		}
	}

	if (try_any_minor == 0) {
		try_any_minor = 1;
		ignore_hints = 1;
		req_sod.sod_minor = -1;
		goto again;
	}
	_dl_free((char *)sod.sod_name);
	_dl_errno = DL_NOT_FOUND;
	return(0);
}


void
_dl_link_sub(elf_object_t *dep, elf_object_t *p)
{
	struct dep_node *n;

	n = _dl_malloc(sizeof *n);
	if (n == NULL)
		_dl_exit(5);
	n->data = dep;
	n->next_sibling = NULL;
	if (p->first_child) {
		p->last_child->next_sibling = n;
		p->last_child = n;
	} else
		p->first_child = p->last_child = n;

	DL_DEB(("linking dep %s as child of %s\n", dep->load_name,
	    p->load_name));
}
