/*
 * Copyright (c) 1994, 1995 Christopher G. Demetriou
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(SYSLIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: semctl.c,v 1.4 1997/07/25 20:30:14 mickey Exp $";
#endif /* SYSLIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdlib.h>

#if __STDC__
int semctl(int semid, int semnum, int cmd, ...)
#else
int semctl(semid, semnum, cmd, va_alist)
	int semid, semnum;
	int cmd;
	va_dcl
#endif
{
	va_list ap;
	union semun semun;
	union semun *semun_ptr = NULL;
#if __STDC__
	va_start(ap, cmd);
#else
	va_start(ap);
#endif
	if (cmd == IPC_SET || cmd == IPC_STAT || cmd == GETALL ||
	    cmd == SETVAL || cmd == SETALL) {
		semun = va_arg(ap, union semun);
		semun_ptr = &semun;
	}
	va_end(ap);

	return (__semctl(semid, semnum, cmd, semun_ptr));
}
