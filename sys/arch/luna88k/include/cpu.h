/* $OpenBSD: cpu.h,v 1.7 2014/06/03 22:43:51 aoyama Exp $ */
/* public domain */
#ifndef	_MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <m88k/cpu.h>

#ifdef _KERNEL

/*
 * 88110 systems only have cpudep6..7 available so far.
 * By the time Luna2001/2010 are supported, we can grow ci_cpudep to a
 * couple more fields to unbreak this.
 */
#ifndef M88110
#define	ci_curspl	ci_cpudep4
#define	ci_swireg	ci_cpudep5
#endif
#define	ci_intr_mask	ci_cpudep6
#define	ci_clock_ack	ci_cpudep7

void luna88k_ext_int(struct trapframe *eframe);
#define	md_interrupt_func	luna88k_ext_int
#endif	/* _KERNEL */

#endif	/* _MACHINE_CPU_H_ */
