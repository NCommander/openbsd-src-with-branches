/*	$OpenBSD: libsa.h,v 1.1 2006/05/16 22:48:18 miod Exp $	*/

/*
 * libsa prototypes 
 */

#include <machine/prom.h>

extern int boothowto;

#define	BOOT_ETHERNET_ZERO	0x0001

void	exec_aout(char *, const char *, int, int, int);
int	parse_args(char *, char **, int);
