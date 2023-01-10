/*	$OpenBSD$	*/

/* public domain */

#include <lib/libsa/stand.h>

#define DEFAULT_KERNEL_ADDRESS 0

void devboot(dev_t, char *);
void machdep(void);
void run_loadfile(uint64_t *, int);
