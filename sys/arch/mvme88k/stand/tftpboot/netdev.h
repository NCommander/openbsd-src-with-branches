/*	$OpenBSD: libsa.h,v 1.3 2002/03/14 01:26:41 millert Exp $	*/

int net_open(struct open_file *, ...);
int net_close(struct open_file *);
int net_ioctl(struct open_file *, u_long, void *);
int net_strategy(void *, int, daddr_t, size_t, void *, size_t *);
