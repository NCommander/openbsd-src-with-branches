/*	$OpenBSD: dev_net.h,v 1.2 1996/04/28 10:49:20 deraadt Exp $ */

int	net_open(struct open_file *, ...);
int	net_close(struct open_file *);
int	net_ioctl();
int	net_strategy();

