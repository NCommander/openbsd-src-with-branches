/*	$OpenBSD: SYS.h,v 1.4 1999/09/16 19:19:46 mickey Exp $	*/

/*
 * Copyright (c) 1998-1999 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/syscall.h>
#include <machine/asm.h>
#include <machine/vmparam.h>
#undef _LOCORE
#define _LOCORE
#include <machine/frame.h>

#define	__ENTRY(p,x)	ENTRY(__CONCAT(p,x))
#define	__EXIT(p,x)	EXIT(__CONCAT(p,x))


#define	__SYSCALL(p,x)				!\
	.import	errno, data			!\
	stw	rp, HPPA_FRAME_ERP(sr0,sp)	!\
	ldil	L%SYSCALLGATE, r1		!\
	ble	4(sr7, r1)			!\
	ldi	__CONCAT(SYS_,x), t1		!\
	comb,=,n r0, t1, __CONCAT(x,$noerr)	!\
	ldil	L%errno, r1			!\
	stw	t1, R%errno(r1)			!\
	ldi	-1, ret0			!\
	ldi	-1, ret1			!\
	.label	__CONCAT(x,$noerr)		!\
	ldw HPPA_FRAME_ERP(sr0,sp), rp

#define	__RSYSCALL(p,x)			!\
__ENTRY(p,x)				!\
	__SYSCALL(p,x)			!\
	bv	r0(rp)			!\
	nop				!\
__EXIT(p,x)

#define	__PSEUDO(p,x,y)			!\
__ENTRY(p,x)				!\
	__SYSCALL(p,y)			!\
	bv	r0(rp)			!\
	nop				!\
__EXIT(p,x)

/*
 * Design note:
 *
 * When the syscalls need to be renamed so they can be handled
 * specially by the threaded library, these macros insert `_thread_sys_'
 * in front of their name. This avoids the need to #ifdef _THREAD_SAFE 
 * everywhere that the renamed function needs to be called.
 */
#ifdef _THREAD_SAFE
/*
 * For the thread_safe versions, we prepend _thread_sys_ to the function
 * name so that the 'C' wrapper can go around the real name.
 */
# define SYSCALL(x)	__SYSCALL(_thread_sys_,x)
# define RSYSCALL(x)	__RSYSCALL(_thread_sys_,x)
# define PSEUDO(x,y)	__PSEUDO(_thread_sys_,x,y)
/*# define SYSENTRY(x)	__ENTRY(_thread_sys_,x)*/
#else _THREAD_SAFE
/*
 * The non-threaded library defaults to traditional syscalls where
 * the function name matches the syscall name.
 */
# define SYSCALL(x)	__SYSCALL(,x)
# define RSYSCALL(x)	__RSYSCALL(,x)
# define PSEUDO(x,y)	__PSEUDO(,x,y)
/*# define SYSENTRY(x)	__ENTRY(,x)*/
#endif _THREAD_SAFE

