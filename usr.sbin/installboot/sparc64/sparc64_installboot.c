/*	$OpenBSD$	*/

/*
 * Copyright (c) 2012, 2013 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ufs/ffs/fs.h>

#include "installboot.h"

char	*blkstore;
size_t	blksize;

void
md_init(void)
{
	stages = 2;
	stage1 = "/usr/mdec/bootblk";
	stage2 = "/boot";
}

void
md_loadboot(void)
{
	struct stat sb;
	size_t blocks;
	int fd;

	if ((fd = open(stage1, O_RDONLY)) < 0)
		err(1, "open");
	if (fstat(fd, &sb) == -1)
		err(1, "fstat");

	blocks = howmany((size_t)sb.st_size, DEV_BSIZE);
	blksize = blocks * DEV_BSIZE;
	if (verbose)
		fprintf(stderr, "boot block is %zu bytes "
                    "(%zu blocks @ %u bytes = %zu bytes)\n",
                    (ssize_t)sb.st_size, blocks, DEV_BSIZE, blksize);
	if (blksize > SBSIZE - DEV_BSIZE)
		errx(1, "boot blocks too big (%zu > %d)",
		    blksize, SBSIZE - DEV_BSIZE);

	blkstore = malloc(blksize);
	if (blkstore == NULL)
		err(1, "malloc");
	memset(blkstore, 0, blksize);
	if (read(fd, blkstore, sb.st_size) != (ssize_t)sb.st_size)
		err(1, "read");

	close(fd);
}

void
md_installboot(int devfd, char *dev)
{
	/* XXX - is this necessary? */
	sync();

	/* Write bootblock into the superblock. */
	if (lseek(devfd, DEV_BSIZE, SEEK_SET) != DEV_BSIZE)
		err(1, "lseek");
	if (verbose)
		fprintf(stderr, "%s boot block to disk %s\n",
		    (nowrite ? "would write" : "writing"), dev);
	if (nowrite)
		return;
	if (write(devfd, blkstore, blksize) != (ssize_t)blksize)
		err(1, "write");
}
