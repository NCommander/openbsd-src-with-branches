/*	$OpenBSD: bug.h,v 1.1 1998/12/15 04:45:50 smurph Exp $ */
#include <machine/bugio.h>

struct bugenv {
	int	clun;
	int	dlun;
	int	ipl;
	int	ctlr;
	int	(*entry)();
	int	cfgblk;
	char	*argstart;
	char	*argend;
};
