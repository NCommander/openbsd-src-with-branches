/*	$OpenBSD: crtbeginS.c,v 1.13 2012/09/08 20:08:33 matthew Exp $	*/
/*	$NetBSD: crtbegin.c,v 1.1 1996/09/12 16:59:03 cgd Exp $	*/

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Run-time module for GNU C++ compiled shared libraries.
 *
 * The linker constructs the following arrays of pointers to global
 * constructors and destructors. The first element contains the
 * number of pointers in each.
 * The tables are also null-terminated.

 */
#include <stdlib.h>

#include "md_init.h"
#include "os-note-elf.h"
#include "extern.h"

/*
 * java class registration hooks
 */

#if (__GNUC__ > 2)
static void *__JCR_LIST__[]
    __attribute__((section(".jcr"), aligned(sizeof(void*)))) = { };

extern void _Jv_RegisterClasses (void *)
    __attribute__((weak));
#endif

/*
 * Include support for the __cxa_atexit/__cxa_finalize C++ abi for
 * gcc > 2.x. __dso_handle is NULL in the main program and a unique
 * value for each C++ shared library. For more info on this API, see:
 *
 *     http://www.codesourcery.com/cxx-abi/abi.html#dso-dtor
 */

#if (__GNUC__ > 2)
void *__dso_handle = &__dso_handle;
__asm(".hidden  __dso_handle");

long __guard_local __dso_hidden __attribute__((section(".openbsd.randomdata")));

extern int __cxa_atexit(void (*)(void *), void *, void *) __attribute__((weak));
extern void __cxa_finalize(void *) __attribute__((weak));

int
atexit(void (*fn)(void))
{
	return (__cxa_atexit((void (*)(void *))fn, NULL, &__dso_handle));
}
asm(".hidden atexit");
#endif

static init_f __CTOR_LIST__[1]
    __attribute__((section(".ctors"))) = { (void *)-1 };	/* XXX */
static init_f __DTOR_LIST__[1]
    __attribute__((section(".dtors"))) = { (void *)-1 };	/* XXX */

static void	__dtors(void) __used;
static void	__ctors(void) __used;

void
__ctors(void)
{
	unsigned long i = (unsigned long) __CTOR_LIST__[0];
	init_f *p;

	if (i == -1)  {
		for (i = 1; __CTOR_LIST__[i] != NULL; i++)
			;
		i--;
	}
	p = __CTOR_LIST__ + i;
	while (i--) {
		(**p--)();
	}
}

static void
__dtors(void)
{
	init_f *p = __DTOR_LIST__ + 1;

	while (*p) {
		(**p++)();
	}
}
void _init(void);
void _fini(void);
static void _do_init(void) __used;
static void _do_fini(void) __used;

MD_SECTION_PROLOGUE(".init", _init);

MD_SECTION_PROLOGUE(".fini", _fini);

MD_SECT_CALL_FUNC(".init", _do_init);
MD_SECT_CALL_FUNC(".fini", _do_fini);

void
_do_init(void)
{
	static int initialized = 0;

	/*
	 * Call global constructors.
	 * Arrange to call global destructors at exit.
	 */
	if (!initialized) {
		initialized = 1;

#if (__GNUC__ > 2)
		if (__JCR_LIST__[0] && _Jv_RegisterClasses)
			_Jv_RegisterClasses(__JCR_LIST__);
#endif

		__ctors();
	}
}

void
_do_fini(void)
{
	static int finalized = 0;
	if (!finalized) {
		finalized = 1;

#if (__GNUC__ > 2)
		if (__cxa_finalize != NULL)
			__cxa_finalize(__dso_handle);
#endif

		/*
		 * since the _init() function sets up the destructors to 
		 * be called by atexit, do not call the destructors here.
		 */
		__dtors();
	}
}
