/*	$OpenBSD: main.c,v 1.11 2018/06/17 08:30:20 anton Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "main.h"

int
main(int argc, char **argv)
{
	extern char *__progname;
	int ret, c;

	ret = 0;
	while ((c = getopt(argc, argv, "fFiIlpPrRstT")) != -1) {
		switch (c) {
		case 'f':
			ret |= check_inheritance();
			break;
		case 'F':
			ret |= do_fdpass();
			break;
		case 'i':
			ret |= do_timer();
			break;
		case 'I':
			ret |= do_invalid_timer();
			break;
		case 'l':
			ret |= do_flock();
			break;
		case 'p':
			ret |= do_pipe();
			break;
		case 'P':
			ret |= do_process();
			break;
		case 'r':
			ret |= do_random();
			break;
		case 'R':
			ret |= do_regress();
			break;
		case 's':
			ret |= do_signal();
			break;
		case 't':
			ret |= do_tun();
			break;
		case 'T':
			ret |= do_pty();
			break;
		default:
			fprintf(stderr, "usage: %s -[fFiIlpPrRstT]\n",
			    __progname);
			exit(1);
		}
	}

	return (ret);
}
