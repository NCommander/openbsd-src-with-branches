/*	$OpenBSD: bug.h,v 1.4.6.1 2001/04/18 16:11:14 niklas Exp $ */
#ifndef __MACHINE_BUG_H__
#define __MACHINE_BUG_H__
#include <machine/bugio.h>

struct bugenv {
	int	clun;
	int	dlun;
	int	ipl;
	int	ctlr;
	int	(*entry) __P((void));
	int	cfgblk;
	char	*argstart;
	char	*argend;
};
#endif /* __MACHINE_BUG_H__ */
