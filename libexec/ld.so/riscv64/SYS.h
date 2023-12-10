/*	$OpenBSD: SYS.h,v 1.1 2021/04/28 15:16:26 drahn Exp $ */

/*
 * Copyright (c) 2016 Dale Rahn <drahn@openbsd.org>
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

#include <machine/asm.h>
#include <sys/syscall.h>

#define PINSYSCALL(sysno, label)				\
	.pushsection .openbsd.syscalls,"",@progbits		;\
	.long label						;\
	.long sysno						;\
	.popsection

#define SYSTRAP(x)						\
	li	t0, SYS_ ## x					;\
99:	ecall							;\
	PINSYSCALL(SYS_ ## x, 99b)
	/* XXX fence */

#define DL_SYSCALL(n)						\
	.global		__CONCAT(_dl_,n)			;\
	.type		__CONCAT(_dl_,n)%function		;\
__CONCAT(_dl_,n):						;\
	RETGUARD_SETUP(__CONCAT(_dl_,n), x15)			;\
	SYSTRAP(n)						;\
	beqz	t0, 1f						;\
	sub	a0, zero, a0	/* r0 = -errno */		;\
1:								;\
	RETGUARD_CHECK(__CONCAT(_dl_,n), x15)	 		;\
	ret
