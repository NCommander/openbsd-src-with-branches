/*	$OpenBSD$	*/
/*
 * Copyright (c) 2004, Mark Kettenis.
 * Copyright (c) 2004, Miodrag Vallat.
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

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include <machine/reg.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * This tests checks whether ptrace will correctly cope with unaligned pc.
 *
 * Platforms known to fail at the moment are: sparc, m68060.
 */
int
main(void)
{
	pid_t pid;
	int status;

	pid = fork();
	if (pid == 0) {
		ptrace(PT_TRACE_ME, 0, 0, 0);
		raise(SIGTRAP);
		exit(EXIT_FAILURE);
	} else {
		struct reg regs;

		waitpid(pid, &status, 0);
		ptrace(PT_GETREGS, pid, (caddr_t)&regs, 0);

		/*
		 * Make sure amd64 is tested before i386,
		 * and sparc64 before sparc.
		 */

#if defined(__x86_64__)
		regs.regs[tRIP] |= 0x07;
#elif defined(__m68k__)
		regs.r_pc |= 0x03;
#elif defined(__hppa__)
		regs.r_pc |= 0x03;
		regs.r_npc |= 0x03;
#elif defined(__i386__)
		regs.r_eip |= 0x03;
#elif defined(__powerpc__)
		regs.pc |= 0x03;
#elif defined( __sparcv9__)
		regs.r_pc |= 0x07;
		regs.r_npc |= 0x07;
#elif defined(__sparc__)
		regs.r_pc |= 0x03;
		regs.r_npc |= 0x03;
#elif defined( __m88k__)
		/*
		 * The following code is for 88100 only, but should work with
		 * 88110 too, as it does not set the DELAY bit in exip.
		 * Though we might want to test the behaviour with delay set
		 * in exip too...
		 */
		regs.sxip |= 0x03;
		regs.snip |= 0x03;
		regs.sfip |= 0x03;
#elif defined(__vax__)
		regs.pc |= 0x03;
#endif
		ptrace(PT_SETREGS, pid, (caddr_t)&regs, 0);
		ptrace(PT_CONTINUE, pid, (caddr_t)1, 0);
	}
	exit(EXIT_SUCCESS);
}
