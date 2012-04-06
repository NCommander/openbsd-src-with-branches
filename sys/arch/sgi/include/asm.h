/*	$OpenBSD: asm.h,v 1.3 2010/04/28 16:20:28 syuu Exp $ */

#ifdef MULTIPROCESSOR
#define HW_GET_CPU_INFO(ci, tmp)	\
	LOAD_XKPHYS(ci, CCA_CACHED);	\
	mfc0	tmp, COP_0_LLADDR;	\
	or	ci, ci, tmp
#endif

#include <mips64/asm.h>
