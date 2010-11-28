/*	$OpenBSD: vmparam.h,v 1.1.1.1 2009/12/09 19:22:50 miod Exp $ */
/* public domain */
#ifndef _LOONGSON_VMPARAM_H_
#define _LOONGSON_VMPARAM_H_

#define	VM_PHYSSEG_MAX		2 /* Max number of physical memory segments */
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BIGFIRST

#include <mips64/vmparam.h>

#endif	/* _LOONGSON_VMPARAM_H_ */
