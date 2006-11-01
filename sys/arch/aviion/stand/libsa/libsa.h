/*	$OpenBSD: libsa.h,v 1.3 2002/03/14 01:26:40 millert Exp $	*/

/*
 * libsa prototypes 
 */

#include <machine/prom.h>

void	exec_aout(char *, const char *, int, int, int);
int	parse_args(char *, char **);
