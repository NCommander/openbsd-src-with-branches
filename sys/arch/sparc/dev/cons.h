/*	$OpenBSD$	*/

struct consdev;
struct zs_chanstate;

extern void *zs_conschan;

extern void nullcnprobe(struct consdev *);

extern int  zs_getc(void *arg);
extern void zs_putc(void *arg, int c);

struct zschan *zs_get_chan_addr(int zsc_unit, int channel);

#ifdef	KGDB
void zs_kgdb_init(void);
void zskgdb(struct zs_chanstate *);
#endif
