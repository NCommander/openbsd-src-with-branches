/*	$OpenBSD: dev_net.h,v 1.2 1998/08/22 08:37:57 smurph Exp $ */

int	net_open(struct open_file *, ...);
int	net_close(struct open_file *);
int	net_ioctl();
int	net_strategy();

