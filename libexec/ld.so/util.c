/*	$OpenBSD: util.c,v 1.27 2013/11/13 05:41:42 deraadt Exp $	*/

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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <string.h>
#include "archdep.h"

/*
 * Stack protector dummies.
 * Ideally, a scheme to compile these stubs from libc should be used, but
 * this would end up dragging too much code from libc here.
 */
long __guard_local __dso_hidden __attribute__((section(".openbsd.randomdata")));

void __stack_smash_handler(char [], int);

void
__stack_smash_handler(char func[], int damaged)
{
	_dl_exit(127);
}

/*
 * Static vars usable after bootstrapping.
 */
static char *_dl_malloc_pool;
static long *_dl_malloc_free;

char *
_dl_strdup(const char *orig)
{
	char *newstr;
	int len;

	len = _dl_strlen(orig)+1;
	newstr = _dl_malloc(len);
	_dl_strlcpy(newstr, orig, len);
	return (newstr);
}


/*
 * The following malloc/free code is a very simplified implementation
 * of a malloc function. However, we do not need to be very complex here
 * because we only free memory when 'dlclose()' is called and we can
 * reuse at least the memory allocated for the object descriptor. We have
 * one dynamic string allocated, the library name and it is likely that
 * we can reuse that one too without a lot of complex collapsing code.
 */
void *
_dl_malloc(size_t need)
{
	long *p, *t, *n, have;

	need = (need + 2*DL_MALLOC_ALIGN - 1) & ~(DL_MALLOC_ALIGN - 1);

	if ((t = _dl_malloc_free) != 0) {	/* Try free list first */
		n = (long *)&_dl_malloc_free;
		while (t && t[-1] < need) {
			n = t;
			t = (long *)*t;
		}
		if (t) {
			*n = *t;
			_dl_memset(t, 0, t[-1] - DL_MALLOC_ALIGN);
			return((void *)t);
		}
	}
	have = _dl_round_page((long)_dl_malloc_pool) - (long)_dl_malloc_pool;
	if (need > have) {
		if (have >= 8 + DL_MALLOC_ALIGN) {
			p = (void *)_dl_malloc_pool;
			p = (void *) ((long)p + DL_MALLOC_ALIGN);
			p[-1] = have;
			_dl_free((void *)p);		/* move to freelist */
		}
		_dl_malloc_pool = (void *)_dl_mmap((void *)0,
		    _dl_round_page(need), PROT_READ|PROT_WRITE,
		    MAP_ANON|MAP_PRIVATE, -1, 0);
		if (_dl_malloc_pool == 0 || _dl_mmap_error(_dl_malloc_pool)) {
			_dl_printf("Dynamic loader failure: malloc.\n");
			_dl_exit(7);
		}
	}
	p = (void *)_dl_malloc_pool;
	_dl_malloc_pool += need;
	_dl_memset(p, 0, need);
	p = (void *) ((long)p + DL_MALLOC_ALIGN);
	p[-1] = need;
	return (p);
}

void
_dl_free(void *p)
{
	long *t = (long *)p;

	*t = (long)_dl_malloc_free;
	_dl_malloc_free = p;
}


void
_dl_randombuf(void *buf, size_t buflen)
{
	const int mib[2] = { CTL_KERN, KERN_ARND };
	_dl_sysctl(mib, 2, buf, &buflen, NULL, 0);
}

unsigned int
_dl_random(void)
{
	unsigned int rnd;
	_dl_randombuf(&rnd, sizeof(rnd));
	return (rnd);
}
