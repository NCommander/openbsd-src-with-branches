/*	$OpenBSD: disk.h,v 1.3 1996/10/30 22:40:42 niklas Exp $	*/
/*	$NetBSD: disk.h,v 1.1 1995/11/23 02:39:42 cgd Exp $	*/

int	diskstrategy(void *, int, daddr_t, size_t, void *, size_t *);
/* int     diskopen(struct open_file *, int, int, int); */
int     diskclose(struct open_file *);

#define	diskioctl	noioctl
