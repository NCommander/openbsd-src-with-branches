/*	$OpenBSD: pwrite.c,v 1.1 2002/02/08 20:58:02 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>

int
main(int argc, char *argv[])
{	
	char temp[] = "/tmp/pwriteXXXXXXXXX";
	const char magic[10] = "0123456789";
	const char zeroes[10] = "0000000000";
	char buf[10];
	char c;
	int fd;

	if ((fd = mkstemp(temp)) < 0)
		err(1, "mkstemp");
	remove(temp);

	if (write(fd, zeroes, sizeof(zeroes)) != sizeof(zeroes))
		err(1, "write");

	if (lseek(fd, 5, SEEK_SET) != 5)
		err(1, "lseek");

	if (pwrite(fd, &magic[1], 4, 4) != 4)
		err(1, "pwrite");

	if (read(fd, &c, 1) != 1)
		err(1, "read");

	if (c != '2')
		errx(1, "read %c != %c", c, '2');

	c = '5';
	if (write(fd, &c, 1) != 1)
		err(1, "write");

	if (pread(fd, buf, 10, 0) != 10)
		err(1, "pread");

	if (memcmp(buf, "0000125400", 10) != 0)
		errx(1, "data mismatch: %s != %s", buf, "0000125400");

	return 0;
}
