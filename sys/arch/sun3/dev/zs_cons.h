/*	$OpenBSD: zs_cons.h,v 1.1 1997/01/16 04:04:01 kstailey Exp $	*/

extern void *zs_conschan;

extern void nullcnprobe(struct consdev *);

extern int  zs_getc(void *arg);
extern void zs_putc(void *arg, int c);

