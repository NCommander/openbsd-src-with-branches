/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: prog2.C,v 1.1.1.1 2005/09/13 20:51:39 drahn Exp $
 */
#include <stdio.h>
#include <dlfcn.h>

int
main()
{
	int ret = 0;
	void *handle;

	handle = dlopen("libac.so.0.0", RTLD_LAZY);
	if (handle != NULL) {
		printf("found libaa, dependancy of libac, not expected\n");
		ret = 1;
	}

	return ret;
}
