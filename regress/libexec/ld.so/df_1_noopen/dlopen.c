/*	$OpenBSD$	*/

#include <stdio.h>
#include <dlfcn.h>

int
main(int argc, char *argv[])
{
	int i;
	void *p;

	for (i = 1; i < argc; i++) {
		p = dlopen(argv[i] + 1, RTLD_LAZY|RTLD_LOCAL);
		if ((p != NULL) != (argv[i][0] == '+'))
			return (1);
	}

	return (0);
}
