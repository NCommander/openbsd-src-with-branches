/*	$OpenBSD: system.c,v 1.1 2001/11/09 00:13:32 marc Exp $ */
/*
 *	Placed in the PUBLIC DOMAIN
 */ 

/*
 * system checks the threads system interface and that waitpid/wait4
 * works correctly.
 */

#include <stdlib.h>
#include "test.h"

int
main(int argc, char **argv)
{
    ASSERT(system("ls") == 0);
    SUCCEED;
}
