/*	$OpenBSD$	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2005 Public Domain.
 */

#include <sys/types.h>
#include <sys/mman.h>

int
main()
{
	void *foo;

	foo = mmap(0, (size_t)-1, 0, 0, 0, 0);

	if (foo == MAP_FAILED)
		return (0);

	return (1);
}
