/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $OpenBSD: SYS.h,v 1.8 2002/02/19 19:39:36 millert Exp $ 
 */

#include <sys/syscall.h>
#include <machine/asm.h>

#ifdef __STDC__
# define __ENTRY(p,x)	ENTRY(p ## x)
# define __DO_SYSCALL(x)	\
			li	v0,SYS_ ## x;	\
			syscall
# define __LEAF2(p,x)	LEAF(p ## x)
# define __END2(p,x)	END(p ## x)
# define __CLABEL2(p,x)	_C_LABEL(p ## x)
#else
# define __ENTRY(p,x)	ENTRY(p/**/x)
# define __DO_SYSCALL(x)	\
			li	v0,SYS_/**/x;	\
			syscall
# define __LEAF2(p,x)	LEAF(p/**/x)
# define __END2(p,x)	END(p/**/x)
# define __CLABEL2(p,x)	_C_LABEL(p/**/x)
#endif

#define __PSEUDO_NOERROR(p,x,y)				\
		__LEAF2(p,x);				\
			__DO_SYSCALL(y);		\
			j	ra;			\
		__END2(p,x)

#define __PSEUDO(p,x,y)   				\
		__LEAF2(p,x);				\
			__DO_SYSCALL(y);		\
			bne	a3,zero,err;		\
			j	ra;			\
		err:	la	t9,_C_LABEL(cerror);	\
			jr	t9;			\
		__END2(p,x)

#define __RSYSCALL(p,x)   __PSEUDO(p,x,x)

#ifdef _THREAD_SAFE
# define RSYSCALL(x)	__RSYSCALL(_thread_sys_,x)
# define PSEUDO(x,y)	__PSEUDO(_thread_sys_,x,y)
# define SYSLEAF(x)	__LEAF2(_thread_sys_,x)
# define SYSEND(x)	__END2(_thread_sys_,x)
#else /* _THREAD_SAFE */
# define RSYSCALL(x)	__RSYSCALL(,x)
# define PSEUDO(x,y)	__PSEUDO(,x,y)
# define SYSLEAF(x)	__LEAF2(,x)
# define SYSEND(x)	__END2(,x)
#endif /* _THREAD_SAFE */

