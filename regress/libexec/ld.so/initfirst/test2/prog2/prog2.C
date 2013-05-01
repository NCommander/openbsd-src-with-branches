/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD$
 */

#include <iostream>
#include <dlfcn.h>
#include <string.h>

int
main()
{
	void *handle1;

	handle1 = dlopen("libaa.so", DL_LAZY);
	if (handle1 == NULL) {
		std::cout << "handle1 open libaa failed\n";
		return (1);
	}
	dlclose(handle1);

	return 0;
}
