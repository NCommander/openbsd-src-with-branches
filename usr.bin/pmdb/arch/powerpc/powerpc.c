/*      $OpenBSD: powerpc.c,v 1.1 2002/03/19 23:15:31 drahn Exp $       */
/*
 * Copyright (c) 2002 Dale Rahn <drahn@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 

#include <stddef.h>
#include <sys/param.h>
#include <sys/ptrace.h>
#include <machine/reg.h>
#include <machine/frame.h>
#include "pmdb.h"

static const char *md_reg_names[] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
	"r8", "r9", "r10","r11","r12","r13","r14","r15",
	"r16","r17","r18","r19","r20","r21","r22","r23",
	"r24","r25","r26","r27","r28","r29","r30","r31",
	"pc", "ps", "cnd", "lr", "cnt", "xer", "mq"
};

struct md_def md_def = { md_reg_names, 39, 32};

void
md_def_init(void)
{
	/* no need to do anything */
}

int
md_getframe(struct pstate *ps, int frame, struct md_frame *fram)
{
	struct reg r;
	reg fp, pc;
	int i;

	if (ptrace(PT_GETREGS, ps->ps_pid, (caddr_t)&r, 0) != 0)
		return (-1);
	fp = r.gpr[1];
	if (frame == 0) {
		pc = r.pc;
		goto out;
	}
	pc = r.lr;
	for (i = 1; i < frame; i++) {
		if (fp != (fp & ~7)) { /* should be 0xf */
			return -1;
		}
		if (process_read(ps, fp, &fp, sizeof(fp)) < 0)
			return -1;
		if (process_read(ps, fp+4, &pc, sizeof(pc)) < 0)
			return -1;
		if (fp == 0) {
			return -1;
		}
	}
out:
	fram->pc = pc;
	fram->fp = fp;
	fram->nargs = 0;
	return 0;
}

int md_getregs(struct pstate *ps, reg *regs)
{
	struct reg md_reg;
	if (ptrace(PT_GETREGS, ps->ps_pid, (caddr_t)&md_reg, 0) != 0)
		return -1;
	memcpy(regs, &md_reg, 32 * sizeof(long));
	memcpy(&regs[32], &md_reg.pc, 7 * sizeof(long));
	return 0;
}
