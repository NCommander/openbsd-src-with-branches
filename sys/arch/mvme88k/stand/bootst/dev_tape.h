/*	$OpenBSD: dev_tape.h,v 1.2 1997/04/22 16:02:20 gvf Exp $ */

int	tape_open __P((struct open_file *, ...));
int	tape_close __P((struct open_file *));
int	tape_strategy __P((void *, int, daddr_t, size_t, void *, size_t *));
int	tape_ioctl();

