/*	$OpenBSD$	*/

/*
 * Public domain - no warranty.
 */

#include <sys/cdefs.h>

int ls_main __P((int argc, char **argv));

int
main(argc, argv)
	int argc;
	char **argv;
{
	return ls_main(argc, argv);
}
