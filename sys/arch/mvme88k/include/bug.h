/*	$OpenBSD$ */

#ifndef _MACHINE_BUG_H_
#define _MACHINE_BUG_H_

struct bugenv {
	int	clun;
	int	dlun;
	int	ipl;
	int	ctlr;
	int	(*entry)(void);
	int	cfgblk;
	char	*argstart;
	char	*argend;
};

#endif	/* _MACHINE_BUG_H_ */
