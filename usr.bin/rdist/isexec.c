/*	$OpenBSD: isexec.c,v 1.11 2017/10/27 16:47:08 mpi Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <elf.h>
#include <fcntl.h>
#include <unistd.h>

#include "client.h"

/*
 * Determine whether 'file' is a binary executable or not.
 */
int
isexec(char *file, struct stat *statp)
{
	Elf32_Ehdr hdr;
	int fd, r;

	/*
	 * Must be a regular file that has some executable mode bit on
	 */
	if (!S_ISREG(statp->st_mode) ||
	    !(statp->st_mode & (S_IXUSR|S_IXGRP|S_IXOTH)))
		return(FALSE);

	if ((fd = open(file, O_RDONLY, 0)) == -1)
		return(FALSE);

	r = read(fd, &hdr, sizeof(hdr)) == sizeof(hdr) &&
	    IS_ELF(hdr) && hdr.e_type == ET_EXEC;
	close(fd);

	return (r);
}
