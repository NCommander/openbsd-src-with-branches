/*	$NetBSD: time.c,v 1.7 1995/06/27 00:34:00 jtc Exp $	*/

/*
 * Copyright (c) 1987, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1987, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)time.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$NetBSD: time.c,v 1.7 1995/06/27 00:34:00 jtc Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int lflag;
int portableflag;

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	register int pid;
	int ch, status;
	struct timeval before, after;
	struct rusage ru;

	lflag = 0;
	while ((ch = getopt(argc, argv, "lp")) != EOF)
		switch((char)ch) {
		case 'p':
			portableflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case '?':
		default:
			fprintf(stderr, "usage: time [-lp] command.\n");
			exit(1);
		}

	if (!(argc -= optind))
		exit(0);
	argv += optind;

	gettimeofday(&before, (struct timezone *)NULL);
	switch(pid = vfork()) {
	case -1:			/* error */
		perror("time");
		exit(1);
		/* NOTREACHED */
	case 0:				/* child */
		execvp(*argv, argv);
		perror(*argv);
		_exit((errno == ENOENT) ? 127 : 126);
		/* NOTREACHED */
	}

	/* parent */
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	while (wait3(&status, 0, &ru) != pid);
	gettimeofday(&after, (struct timezone *)NULL);
	if (!WIFEXITED(status))
		fprintf(stderr, "Command terminated abnormally.\n");
	timersub(&after, &before, &after);

	if (portableflag) {
		fprintf (stderr, "real %9ld.%02ld\n", 
			after.tv_sec, after.tv_usec/10000);
		fprintf (stderr, "user %9ld.%02ld\n",
			ru.ru_utime.tv_sec, ru.ru_utime.tv_usec/10000);
		fprintf (stderr, "sys  %9ld.%02ld\n",
			ru.ru_stime.tv_sec, ru.ru_stime.tv_usec/10000);
	} else {

		fprintf(stderr, "%9ld.%02ld real ", 
			after.tv_sec, after.tv_usec/10000);
		fprintf(stderr, "%9ld.%02ld user ",
			ru.ru_utime.tv_sec, ru.ru_utime.tv_usec/10000);
		fprintf(stderr, "%9ld.%02ld sys\n",
			ru.ru_stime.tv_sec, ru.ru_stime.tv_usec/10000);
	}

	if (lflag) {
		int hz = 100;			/* XXX */
		long ticks;

		ticks = hz * (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec) +
		     hz * (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec) / 1000000;

		fprintf(stderr, "%10ld  %s\n",
			ru.ru_maxrss, "maximum resident set size");
		fprintf(stderr, "%10ld  %s\n", ticks ? ru.ru_ixrss / ticks : 0,
			"average shared memory size");
		fprintf(stderr, "%10ld  %s\n", ticks ? ru.ru_idrss / ticks : 0,
			"average unshared data size");
		fprintf(stderr, "%10ld  %s\n", ticks ? ru.ru_isrss / ticks : 0,
			"average unshared stack size");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_minflt, "page reclaims");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_majflt, "page faults");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_nswap, "swaps");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_inblock, "block input operations");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_oublock, "block output operations");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_msgsnd, "messages sent");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_msgrcv, "messages received");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_nsignals, "signals received");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_nvcsw, "voluntary context switches");
		fprintf(stderr, "%10ld  %s\n",
			ru.ru_nivcsw, "involuntary context switches");
	}

	exit (WEXITSTATUS(status));
}
