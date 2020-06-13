/*	$OpenBSD$	*/

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

#ifndef _MACHINE_INTR_H_
#define _MACHINE_INTR_H_

#define IPL_NONE	0
#define IPL_SOFTCLOCK	1
#define IPL_SOFTNET	2
#define IPL_SOFTTTY	3
#define IPL_BIO		4
#define IPL_NET		5
#define IPL_TTY		6
#define IPL_VM		IPL_TTY
#define IPL_AUDIO	7
#define IPL_CLOCK	8
#define IPL_STATCLOCK	IPL_CLOCK
#define IPL_SCHED	IPL_CLOCK
#define IPL_HIGH	IPL_CLOCK
#define IPL_IPI		9

#define	IPL_MPFLOOR	IPL_TTY
/* Interrupt priority 'flags'. */
#define	IPL_IRQMASK	0xf	/* priority only */
#define	IPL_FLAGMASK	0xf00	/* flags only*/
#define	IPL_MPSAFE	0x100	/* 'mpsafe' interrupt, no kernel lock */

int	splraise(int);
int	spllower(int);
void	splx(int);

#define spl0()		spllower(IPL_NONE)
#define splsoftclock()	splraise(IPL_SOFTCLOCK)
#define splsoftnet()	splraise(IPL_SOFTNET)
#define splsofttty()	splraise(IPL_SOFTTTY)
#define splbio()	splraise(IPL_BIO)
#define splnet()	splraise(IPL_NET)
#define spltty()	splraise(IPL_TTY)
#define splvm()		splraise(IPL_VM)
#define splclock()	splraise(IPL_CLOCK)
#define splstatclock()	splraise(IPL_STATCLOCK)
#define splsched()	splraise(IPL_SCHED)
#define splhigh()	splraise(IPL_HIGH)

#ifdef DIAGNOSTIC
/*
 * Although this function is implemented in MI code, it must be in this MD
 * header because we don't want this header to include MI includes.
 */
void splassert_fail(int, int, const char *);
extern int splassert_ctl;
void splassert_check(int, const char *);
#define splassert(__wantipl) do {			\
	if (splassert_ctl > 0) {			\
		splassert_check(__wantipl, __func__);	\
	}						\
} while (0)
#define	splsoftassert(wantipl)	splassert(wantipl)
#else
#define	splassert(wantipl)	do { /* nothing */ } while (0)
#define	splsoftassert(wantipl)	do { /* nothing */ } while (0)
#endif

#define intr_barrier(x)

#define IST_EDGE	0
#define IST_LEVEL	1

void	*intr_establish(uint32_t, int, int,
	    int (*)(void *), void *, const char *);

extern void (*_hvi)(struct trapframe *);
extern void *(*_intr_establish)(uint32_t, int, int,
	    int (*)(void *), void *, const char *);
extern void (*_setipl)(int);

#include <machine/softintr.h>

#endif /* _MACHINE_INTR_H_ */
