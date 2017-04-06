/* $OpenBSD: asm.h,v 1.1.1.1 2009/07/31 09:26:25 miod Exp $ */
/* public domain */

#ifdef MULTIPROCESSOR
#define HW_GET_CPU_INFO(ci, tmp)	\
	dmfc0	ci, COP_0_ERROR_PC
#endif

#include <mips64/asm.h>
