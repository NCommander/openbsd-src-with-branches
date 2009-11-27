/*	$OpenBSD: atomic.h,v 1.4 2009/04/12 17:52:17 miod Exp $	*/

/* Public Domain */

#ifndef __MIPS64_ATOMIC_H__
#define __MIPS64_ATOMIC_H__

#if defined(_KERNEL)

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm__ __volatile__ (
	"1:	ll	%0,	0(%1)\n"
	"	or	%0,	%2,	%0\n"
	"	sc	%0,	0(%1)\n"
	"	beqz	%0,	1b\n"
	"	 nop\n" :
		"=&r"(tmp) :
		"r"(uip), "r"(v) : "memory");
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm__ __volatile__ (
	"1:	ll	%0,	0(%1)\n"
	"	and	%0,	%2,	%0\n"
	"	sc	%0,	0(%1)\n"
	"	beqz	%0,	1b\n"
	"	 nop\n" :
		"=&r"(tmp) :
		"r"(uip), "r"(~v) : "memory");
}

static __inline void
atomic_add_int(__volatile unsigned int *uip, unsigned int v)
{
	unsigned int tmp;

	__asm__ __volatile__ (
	"1:	ll	%0,	0(%1)\n"
	"	addu	%0,	%2,	%0\n"
	"	sc	%0,	0(%1)\n"
	"	beqz	%0,	1b\n"
	"	 nop\n" :
		"=&r"(tmp) :
		"r"(uip), "r"(v) : "memory");
}
static __inline void
atomic_add_uint64(__volatile uint64_t *uip, uint64_t v)
{
	uint64_t tmp;

	__asm__ __volatile__ (
	"1:	lld	%0,	0(%1)\n"
	"	daddu	%0,	%2,	%0\n"
	"	scd	%0,	0(%1)\n"
	"	beqz	%0,	1b\n"
	"	 nop\n" :
		"=&r"(tmp) :
		"r"(uip), "r"(v) : "memory");
}
#endif /* defined(_KERNEL) */
#endif /* __MIPS64_ATOMIC_H__ */
