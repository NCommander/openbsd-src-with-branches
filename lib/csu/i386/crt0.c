/*	$OpenBSD: crt0.c,v 1.9 2003/02/28 18:05:51 deraadt Exp $	*/
/*	$NetBSD: crt0.c,v 1.20 1995/06/03 13:16:08 pk Exp $	*/

/*
 * Copyright (c) 1995 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: crt0.c,v 1.9 2003/02/28 18:05:51 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <stdlib.h>

static char	*_strrchr(char *, char);

char	**environ;
char	*__progname = "";

char __progname_storage[NAME_MAX+1];

#ifdef MCRT0
extern void	monstartup(u_long, u_long);
extern void	_mcleanup(void);
extern unsigned char _etext, _eprol;
#endif /* MCRT0 */


__asm("
	.text
	.align  4
	.globl  __start
	.globl  _start
_start:
__start:
	pushl	%ebx			#ps_strings
	pushl   %ecx                    # obj
	pushl   %edx                    # cleanup
	movl    12(%esp),%eax
	leal    20(%esp,%eax,4),%ecx
	leal    16(%esp),%edx
	pushl   %ecx
	pushl   %edx
	pushl   %eax
	call    ___start
");

void
___start(argc, argv, envp, cleanup, obj, ps_strings)
	int argc;
	char **argv;
	char ** envp;
	void (*cleanup)(void);			/* from shared loader */
	const void *obj;			/* from shared loader */
	struct ps_strings *ps_strings;
{
	char *namep;
	register struct kframe *kfp;
	char *s;

        environ = envp;

	if ((namep = argv[0]) != NULL) {	/* NULL ptr if argc = 0 */
		if ((__progname = _strrchr(namep, '/')) == NULL)
			__progname = namep;
		else
			++__progname;
		for (s = __progname_storage; *__progname &&
		    s < &__progname_storage[sizeof __progname_storage - 1]; )
			*s++ = *__progname++;
		*s = '\0';
		__progname = __progname_storage;
	}

#ifdef MCRT0
	atexit(_mcleanup);
	monstartup((u_long)&_eprol, (u_long)&_etext);
#endif

	__init();

	exit(main(argc, argv, environ));
}


static char *
_strrchr(p, ch)
register char *p, ch;
{
	register char *save;

	for (save = NULL;; ++p) {
		if (*p == ch)
			save = (char *)p;
		if (!*p)
			return(save);
	}
/* NOTREACHED */
}

#ifdef MCRT0
asm ("  .text");
asm ("_eprol:");
#endif

