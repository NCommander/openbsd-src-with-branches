/*	$OpenBSD: cpu.h,v 1.19 2020/07/11 11:43:57 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_

/*
 * User-visible definitions
 */

/* 
 * CTL_MACHDEP definitions.
 */
#define CPU_ALTIVEC		1	/* altivec is present */
#define CPU_MAXID		2	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "altivec", CTLTYPE_INT }, \
}

#ifdef _KERNEL

/*
 * Kernel-only definitions
 */

#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/psl.h>
#include <machine/pte.h>

#include <sys/device.h>
#include <sys/sched.h>
#include <sys/srp.h>

struct cpu_info {
	struct device	*ci_dev;
	struct cpu_info	*ci_next;
	struct schedstate_percpu ci_schedstate;

	uint32_t	ci_cpuid;
	uint32_t	ci_pir;

	struct proc	*ci_curproc;
	struct pcb	*ci_curpcb;

	struct slb	ci_kernel_slb[32];
	paddr_t		ci_user_slb_pa;

#define CPUSAVE_LEN	9
	register_t	ci_tempsave[CPUSAVE_LEN];

	uint64_t	ci_lasttb;
	uint64_t	ci_nexttimerevent;
	uint64_t	ci_nextstatevent;
	int		ci_statspending;
	
	volatile int 	ci_cpl;
	uint32_t	ci_ipending;
	uint32_t	ci_idepth;
#ifdef DIAGNOSTIC
	int		ci_mutex_level;
#endif
	int		ci_want_resched;

	uint32_t	ci_randseed;

#ifdef MULTIPROCESSOR
	struct srp_hazard ci_srp_hazards[SRP_HAZARD_NUM];
	void		*ci_initstack_end;
	volatile int		ci_flags;
#endif
};

#define CPUF_PRIMARY 		(1 << 0)
#define CPUF_AP	 		(1 << 1)
#define CPUF_IDENTIFY		(1 << 2)
#define CPUF_IDENTIFIED		(1 << 3)
#define CPUF_PRESENT		(1 << 4)
#define CPUF_GO			(1 << 5)
#define CPUF_RUNNING		(1 << 6)

extern struct cpu_info cpu_info[];
extern struct cpu_info *cpu_info_primary;

register struct cpu_info *__curcpu asm("r13");
#define curcpu()	__curcpu

#define CPU_INFO_ITERATOR	int

#ifndef MULTIPROCESSOR

#define MAXCPUS			1
#define CPU_IS_PRIMARY(ci)	1
#define cpu_number()		0

#define CPU_INFO_UNIT(ci)	0
#define CPU_INFO_FOREACH(cii, ci) \
	for (cii = 0, ci = curcpu(); ci != NULL; ci = NULL)

#else

#define MAXCPUS			16
#define CPU_IS_PRIMARY(ci)	((ci) == cpu_info_primary)
#define cpu_number()		(curcpu()->ci_cpuid)

#define CPU_INFO_UNIT(ci)	((ci)->ci_dev ? (ci)->ci_dev->dv_unit : 0)
#define CPU_INFO_FOREACH(cii, ci) \
	for (cii = 0, ci = &cpu_info[0]; cii < ncpus; cii++, ci++)

void	cpu_boot_secondary_processors(void);
void	cpu_startclock(void);

#endif

#define clockframe trapframe

#define CLKF_INTR(frame)	(curcpu()->ci_idepth > 1)
#define CLKF_USERMODE(frame)	(frame->srr1 & PSL_PR)
#define CLKF_PC(frame)		(frame->srr0)

#define aston(p)		((p)->p_md.md_astpending = 1)
#define need_proftick(p)	aston(p)

#define cpu_kick(ci)
#define cpu_unidle(ci)
#define CPU_BUSY_CYCLE()	do {} while (0)
#define signotify(p)		setsoftast()

#define curpcb			curcpu()->ci_curpcb

static inline unsigned int
cpu_rnd_messybits(void)
{
	uint64_t tb;

	__asm volatile("mftb %0" : "=r" (tb));
	return ((tb >> 32) ^ tb);
}

void need_resched(struct cpu_info *);
#define clear_resched(ci)	((ci)->ci_want_resched = 0)

void delay(u_int);
#define DELAY(x)	delay(x)

#define setsoftast()		aston(curcpu()->ci_curproc)

#define PROC_STACK(p)		((p)->p_md.md_regs->fixreg[1])
#define PROC_PC(p)		((p)->p_md.md_regs->srr0)

void	proc_trampoline(void);

static inline void
intr_enable(void)
{
	mtmsr(mfmsr() | PSL_EE);
}

static inline u_long
intr_disable(void)
{
	u_long msr;

	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);
	return msr;
}

static inline void
intr_restore(u_long msr)
{
	mtmsr(msr);
}

#endif /* _KERNEL */

#ifdef MULTIPROCESSOR
#include <sys/mplock.h>
#endif /* MULTIPROCESSOR */

#endif /* _MACHINE_CPU_H_ */
