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
	void *handle;
	handle = dlopen("libac.so.0.0", RTLD_LAZY);
	printf("handle %p\n", handle);
	return 0;
}
