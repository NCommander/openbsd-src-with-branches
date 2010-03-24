/*	$OpenBSD: asm.h,v 1.1 2004/08/06 21:12:18 pefo Exp $ */

/* Use Mips generic include file */

#ifdef MULTIPROCESSOR
#define HW_CPU_NUMBER(reg)			\
	LA reg, HW_CPU_NUMBER_REG;		\
	PTR_L reg, 0(reg)
#else  /* MULTIPROCESSOR */
#define HW_CPU_NUMBER(reg)			\
	LI reg, 0
#endif /* MULTIPROCESSOR */

#include <mips64/asm.h>
