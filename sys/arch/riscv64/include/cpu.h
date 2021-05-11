/*
 * Copyright (c) 2019 Mike Larkin <mlarkin@openbsd.org>
 * Copyright (c) 2016 Dale Rahn <drahn@dalerahn.com>
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

/*  CTL_MACHDEP definitions. */
#define	CPU_COMPATIBLE		1	/* compatible property */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "compatible", CTLTYPE_STRING }, \
}

#ifdef _KERNEL

/*
 * Kernel-only definitions
 */
#include <machine/intr.h>
#include <machine/frame.h>
#include <machine/riscvreg.h>

/* All the CLKF_* macros take a struct clockframe * as an argument. */

#define clockframe trapframe
/*
 * CLKF_USERMODE: Return TRUE/FALSE (1/0) depending on whether the
 * frame came from USR mode or not.
 */
#define CLKF_USERMODE(frame)	((frame->tf_sstatus & SSTATUS_SPP) == 0)

/*
 * CLKF_INTR: True if we took the interrupt from inside another
 * interrupt handler.
 */
#define CLKF_INTR(frame)	(curcpu()->ci_idepth > 1)

/*
 * CLKF_PC: Extract the program counter from a clockframe
 */
#define CLKF_PC(frame)		(frame->tf_sepc)

/*
 * PROC_PC: Find out the program counter for the given process.
 */
#define PROC_PC(p)	((p)->p_addr->u_pcb.pcb_tf->tf_sepc)
#define PROC_STACK(p)	((p)->p_addr->u_pcb.pcb_tf->tf_sp)

#include <sys/device.h>
#include <sys/sched.h>
#include <sys/srp.h>

struct cpu_info {
	struct device		*ci_dev; /* Device corresponding to this CPU */
	struct cpu_info		*ci_next;
	struct schedstate_percpu ci_schedstate; /* scheduler state */

	u_int32_t		ci_cpuid;
#if 0
	uint64_t		ci_mpidr;
	u_int			ci_acpi_proc_id;
#endif
	int			ci_node;
	struct cpu_info		*ci_self;

	struct proc		*ci_curproc;
	struct pmap		*ci_curpm;
#if 0
	struct proc		*ci_fpuproc;
#endif
	u_int32_t		ci_randseed;

	struct pcb		*ci_curpcb;
	struct pcb		*ci_idle_pcb;

	u_int32_t		ci_ctrl; /* The CPU control register */

	uint64_t		ci_lasttb;
	uint64_t		ci_nexttimerevent;
	uint64_t		ci_nextstatevent;
	int			ci_statspending;

	uint32_t		ci_cpl;
	uint32_t		ci_ipending;
	uint32_t		ci_idepth;
#ifdef DIAGNOSTIC
	int			ci_mutex_level;
#endif
	int			ci_want_resched;

	/* currently loaded fpu proc ctx */
	struct proc		*ci_fpuproc;

#if 0
	void			(*ci_flush_bp)(void);

	struct opp_table	*ci_opp_table;
	volatile int		ci_opp_idx;
	volatile int		ci_opp_max;
	uint32_t		ci_cpu_supply;
#endif

#ifdef MULTIPROCESSOR
	struct srp_hazard	ci_srp_hazards[SRP_HAZARD_NUM];
	volatile int		ci_flags;
#if 0
	uint64_t		ci_ttbr1;
	vaddr_t			ci_el1_stkend;
#endif

	volatile int		ci_ddb_paused;
#define CI_DDB_RUNNING		0
#define CI_DDB_SHOULDSTOP	1
#define CI_DDB_STOPPED		2
#define CI_DDB_ENTERDDB		3
#define CI_DDB_INDDB		4

#endif

#ifdef GPROF
	struct gmonparam	*ci_gmon;
#endif
};

#define CPUF_PRIMARY		(1<<0)
#define CPUF_AP			(1<<1)
#define CPUF_IDENTIFY		(1<<2)
#define CPUF_IDENTIFIED		(1<<3)
#define CPUF_PRESENT		(1<<4)
#define CPUF_GO			(1<<5)
#define CPUF_RUNNING		(1<<6)

static inline struct cpu_info *
curcpu(void)
{
	struct cpu_info *__ci = NULL;
	__asm __volatile("mv %0, tp" : "=&r"(__ci));
	return (__ci);
}

extern uint32_t boot_hart;	/* The hart we booted on. */
extern struct cpu_info cpu_info_primary;
extern struct cpu_info *cpu_info_list;

#ifndef MULTIPROCESSOR

#define cpu_number()	0
#define CPU_IS_PRIMARY(ci)	1
#define CPU_INFO_ITERATOR	int
#define CPU_INFO_FOREACH(cii, ci) \
	for (cii = 0, ci = curcpu(); ci != NULL; ci = NULL)
#define CPU_INFO_UNIT(ci)	0
#define MAXCPUS	1
#define cpu_unidle(ci)

#else

#define cpu_number()		(curcpu()->ci_cpuid)
#define CPU_IS_PRIMARY(ci)	((ci) == &cpu_info_primary)
#define CPU_INFO_ITERATOR		int
#define CPU_INFO_FOREACH(cii, ci)	for (cii = 0, ci = cpu_info_list; \
					    ci != NULL; ci = ci->ci_next)
#define CPU_INFO_UNIT(ci)	((ci)->ci_dev ? (ci)->ci_dev->dv_unit : 0)
#define MAXCPUS	32

extern struct cpu_info *cpu_info[MAXCPUS];

void cpu_boot_secondary_processors(void);
#endif /* !MULTIPROCESSOR */

#define CPU_BUSY_CYCLE()	do {} while (0)

#define curpcb		curcpu()->ci_curpcb

static inline unsigned int
cpu_rnd_messybits(void)
{
	// Should do bit reversal ^ with csr_read(time);
	return csr_read(time);
}

/*
 * Scheduling glue
 */
#define aston(p)	((p)->p_md.md_astpending = 1)
#define	setsoftast()	aston(curcpu()->ci_curproc)

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */

#ifdef MULTIPROCESSOR
void cpu_unidle(struct cpu_info *ci);
#define signotify(p)	(aston(p), cpu_unidle((p)->p_cpu))
void cpu_kick(struct cpu_info *);
#else
#define cpu_kick(ci)
#define cpu_unidle(ci)
#define signotify(p)	setsoftast()
#endif

/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */
void need_resched(struct cpu_info *);
#define clear_resched(ci)	((ci)->ci_want_resched = 0)

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the i386, request an ast to send us
 * through trap(), marking the proc as needing a profiling tick.
 */
#define	need_proftick(p)	aston(p)

// asm code to start new kernel contexts.
void	proc_trampoline(void);
void	child_trampoline(void);

/*
 * Random cruft
 */
void	dumpconf(void);

/* cpuswitch.S */
struct pcb;
void	savectx		(struct pcb *pcb);

static inline void
intr_enable(void)
{
	__asm __volatile("csrsi sstatus, %0" :: "i" (SSTATUS_SIE));
}

static inline u_long
intr_disable(void)
{
	uint64_t ret;

	__asm __volatile(
	    "csrrci %0, sstatus, %1"
	    : "=&r" (ret) : "i" (SSTATUS_SIE)
	);

	return (ret & (SSTATUS_SIE));
}

static inline void
intr_restore(u_long s)
{
	__asm __volatile("csrs sstatus, %0" :: "r" (s));
}

void	delay (unsigned);
#define	DELAY(x)	delay(x)

void fpu_save(struct proc *, struct trapframe *);
void fpu_load(struct proc *);
void fpu_discard(struct proc *p);

#endif /* _KERNEL */

#ifdef MULTIPROCESSOR
#include <sys/mplock.h>
#endif /* MULTIPROCESSOR */

#endif /* !_MACHINE_CPU_H_ */
