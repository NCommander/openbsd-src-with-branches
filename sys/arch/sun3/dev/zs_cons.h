/*	$OpenBSD$	*/

extern void *zs_conschan;

extern void nullcnprobe(struct consdev *);

extern int  zs_getc(void *arg);
extern void zs_putc(void *arg, int c);

