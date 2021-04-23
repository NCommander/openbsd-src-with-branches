/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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

#ifndef	_MACHINE_INTR_H_
#define	_MACHINE_INTR_H_

/*
 * The interrupt level ipl is a logical level; per-platform interrupt
 * code will turn it into the appropriate hardware interrupt masks
 * values.
 *
 * Interrupt sources on the CPU are kept enabled regardless of the
 * current ipl value; individual hardware sources interrupting while
 * logically masked are masked on the fly, remembered as pending, and
 * unmasked at the first splx() opportunity.
 */
#ifdef _KERNEL

/* Interrupt priority `levels'; not mutually exclusive. */
#define	IPL_NONE	0	/* nothing */
#define	IPL_SOFT	1	/* soft interrupts */
#define	IPL_SOFTCLOCK	2	/* soft clock interrupts */
#define	IPL_SOFTNET	3	/* soft network interrupts */
#define	IPL_SOFTTTY	4	/* soft terminal interrupts */
#define	IPL_BIO		5	/* block I/O */
#define	IPL_NET		6	/* network */
#define	IPL_TTY		7	/* terminal */
#define	IPL_VM		8	/* memory allocation */
#define	IPL_AUDIO	9	/* audio */
#define	IPL_CLOCK	10	/* clock */
#define	IPL_SCHED	IPL_CLOCK
#define	IPL_STATCLOCK	IPL_CLOCK
#define	IPL_HIGH	11	/* everything */
#define	IPL_IPI		12	/* interprocessor interrupt */
#define	NIPL		13	/* number of levels */

#define	IPL_MPFLOOR	IPL_TTY
/* Interrupt priority 'flags'. */
#define	IPL_IRQMASK	0xf	/* priority only */
#define	IPL_FLAGMASK	0xf00	/* flags only*/
#define	IPL_MPSAFE	0x100	/* 'mpsafe' interrupt, no kernel lock */

/* Interrupt sharing types. */
#define	IST_NONE	0	/* none */
#define	IST_PULSE	1	/* pulsed */
#define	IST_EDGE	2	/* edge-triggered */
#define	IST_LEVEL	3	/* level-triggered */

#define	IST_LEVEL_LOW		IST_LEVEL
#define	IST_LEVEL_HIGH		4
#define	IST_EDGE_FALLING	IST_EDGE
#define	IST_EDGE_RISING		5
#define	IST_EDGE_BOTH		6

/* RISCV interrupt mcause, from freebsd */
#define	RISCV_NIRQ		1024

#ifndef	NIRQ
#define	NIRQ			RISCV_NIRQ
#endif

enum {
	IRQ_SOFTWARE_USER,
	IRQ_SOFTWARE_SUPERVISOR,
	IRQ_SOFTWARE_HYPERVISOR,
	IRQ_SOFTWARE_MACHINE,
	IRQ_TIMER_USER,
	IRQ_TIMER_SUPERVISOR,
	IRQ_TIMER_HYPERVISOR,
	IRQ_TIMER_MACHINE,
	IRQ_EXTERNAL_USER,
	IRQ_EXTERNAL_SUPERVISOR,
	IRQ_EXTERNAL_HYPERVISOR,
	IRQ_EXTERNAL_MACHINE,
	INTC_NIRQS
};

#ifndef _LOCORE
#include <sys/device.h>
#include <sys/queue.h>

#include <machine/frame.h>

int	 splraise(int);
int	 spllower(int);
void	 splx(int);

void	 riscv_cpu_intr(void *);
void	 riscv_do_pending_intr(int);
void	 riscv_set_intr_func(int (*raise)(int), int (*lower)(int),
    void (*x)(int), void (*setipl)(int));
void	 riscv_set_intr_handler(void (*intr_handle)(void *));

struct riscv_intr_func {
	int (*raise)(int);
	int (*lower)(int);
	void (*x)(int);
	void (*setipl)(int);
};

extern struct riscv_intr_func riscv_intr_func;

#define	splraise(cpl)		(riscv_intr_func.raise(cpl))
#define	_splraise(cpl)		(riscv_intr_func.raise(cpl))
#define	spllower(cpl)		(riscv_intr_func.lower(cpl))
#define	splx(cpl)		(riscv_intr_func.x(cpl))

#define	splsoft()	splraise(IPL_SOFT)
#define	splsoftclock()	splraise(IPL_SOFTCLOCK)
#define	splsoftnet()	splraise(IPL_SOFTNET)
#define	splsofttty()	splraise(IPL_SOFTTTY)
#define	splbio()	splraise(IPL_BIO)
#define	splnet()	splraise(IPL_NET)
#define	spltty()	splraise(IPL_TTY)
#define	splvm()		splraise(IPL_VM)
#define	splaudio()	splraise(IPL_AUDIO)
#define	splclock()	splraise(IPL_CLOCK)
#define	splsched()	splraise(IPL_SCHED)
#define	splstatclock()	splraise(IPL_STATCLOCK)
#define	splhigh()	splraise(IPL_HIGH)

#define	spl0()		spllower(IPL_NONE)

#include <machine/riscvreg.h>

void	 intr_barrier(void *);

static inline void
enable_interrupts(void)
{
	__asm volatile(
		"csrsi sstatus, %0"
		:: "i" (SSTATUS_SIE)
	);
}

static inline uint64_t
disable_interrupts(void)
{
	uint64_t ret;

	__asm volatile(
		"csrrci %0, sstatus, %1"
		: "=&r" (ret) : "i" (SSTATUS_SIE)
	);

	return (ret & (SSTATUS_SIE));
}

static inline void
restore_interrupts(uint64_t s)
{
	__asm volatile(
		"csrs sstatus, %0"
		:: "r" (s & (SSTATUS_SIE))
	);
}

void	 riscv_init_smask(void); /* XXX */
extern uint32_t riscv_smask[NIPL];

#include <machine/softintr.h>

void 	riscv_clock_register(void (*)(void), void (*)(u_int), void (*)(int),
    void (*)(void));

/*
 **** interrupt controller structure and routines ****
 */
struct cpu_info;
struct interrupt_controller {
	int	ic_node;
	void	*ic_cookie;
	void	*(*ic_establish)(void *, int *, int, int (*)(void *),
		    void *, char *);
	void	 (*ic_disestablish)(void *);
	void	 (*ic_enable)(void *);
	void	 (*ic_disable)(void *);
	void	 (*ic_route)(void *, int, struct cpu_info *);
	void	 (*ic_cpu_enable)(void);

	LIST_ENTRY(interrupt_controller) ic_list;
	uint32_t ic_phandle;
	uint32_t ic_cells;
};

void	 riscv_intr_init_fdt(void);
void	 riscv_intr_register_fdt(struct interrupt_controller *);
void	*riscv_intr_establish_fdt(int, int, int (*)(void *),
	    void *, char *);
void	*riscv_intr_establish_fdt_idx(int, int, int, int (*)(void *),
	    void *, char *);
void	 riscv_intr_disestablish_fdt(void *);
void	 riscv_intr_enable(void *);
void	 riscv_intr_disable(void *);
void	 riscv_intr_route(void *, int, struct cpu_info *);
void	 riscv_intr_cpu_enable(void);

void	 riscv_send_ipi(struct cpu_info *, int);
extern void (*intr_send_ipi_func)(struct cpu_info *, int);

#define riscv_IPI_NOP	0
#define riscv_IPI_DDB	1

#ifdef DIAGNOSTIC
/*
 * Although this function is implemented in MI code, it must be in this MD
 * header because we don't want this header to include MI includes.
 */
void splassert_fail(int, int, const char *);
extern int splassert_ctl;
void riscv_splassert_check(int, const char *);
#define splassert(__wantipl) do {				\
	if (splassert_ctl > 0) {				\
		riscv_splassert_check(__wantipl, __func__);	\
	}							\
} while (0)
#define	splsoftassert(wantipl)	splassert(wantipl)
#else
#define	splassert(wantipl)	do { /* nothing */ } while (0)
#define	splsoftassert(wantipl)	do { /* nothing */ } while (0)
#endif

#endif /* ! _LOCORE */

#endif /* _KERNEL */

#endif	/* _MACHINE_INTR_H_ */

