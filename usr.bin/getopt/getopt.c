/*	$OpenBSD: getopt.c,v 1.3 1997/01/15 23:42:32 millert Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: getopt.c,v 1.3 1997/01/15 23:42:32 millert Exp $";
#endif /* not lint */

#include <stdio.h>
#include <unistd.h>

int
main(argc, argv)
int argc;
char *argv[];
{
	extern int optind;
	extern char *optarg;
	int c;
	int status = 0;

	optind = 2;	/* Past the program name and the option letters. */
	while ((c = getopt(argc, argv, argv[1])) != -1)
		switch (c) {
		case '?':
			status = 1;	/* getopt routine gave message */
			break;
		default:
			if (optarg != NULL)
				printf(" -%c %s", c, optarg);
			else
				printf(" -%c", c);
			break;
		}
	printf(" --");
	for (; optind < argc; optind++)
		printf(" %s", argv[optind]);
	printf("\n");
	exit(status);
}
