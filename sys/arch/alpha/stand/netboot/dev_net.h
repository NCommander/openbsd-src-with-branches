/*	$OpenBSD: dev_net.h,v 1.1 1996/10/30 22:40:53 niklas Exp $	*/


int	net_open(struct open_file *, ...);
int	net_close(struct open_file *);
int	net_ioctl();
int	net_strategy();

