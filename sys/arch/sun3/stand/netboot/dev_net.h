/*	$OpenBSD: dev_net.h,v 1.2 2001/07/04 08:33:54 niklas Exp $	*/


int	net_open(struct open_file *, ...);
int	net_close(struct open_file *);
int	net_ioctl();
int	net_strategy();

