/*
 * Copyright (c) 1994 Winning Strategies, Inc.
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
 *      This product includes software developed by Winning Strategies, Inc.
 * 4. The name of Winning Strategies, Inc. may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
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

#ifndef lint
static char rcsid[] = "$Id: uname.c,v 1.7 1994/12/20 01:28:57 jtc Exp $";
#endif /* not lint */

#include <stdio.h>
#include <locale.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <err.h>

static void usage();

#define	PRINT_SYSNAME	0x01
#define	PRINT_NODENAME	0x02
#define	PRINT_RELEASE	0x04
#define	PRINT_VERSION	0x08
#define	PRINT_MACHINE	0x10
#define	PRINT_ALL	0x1f

int
main(argc, argv) 
	int argc;
	char **argv;
{
	struct utsname u;
	int c;
	int space = 0;
	int print_mask = 0;

	setlocale(LC_ALL, "");

	while ((c = getopt(argc,argv,"amnrsv")) != -1 ) {
		switch ( c ) {
		case 'a':
			print_mask |= PRINT_ALL;
			break;
		case 'm':
			print_mask |= PRINT_MACHINE;
			break;
		case 'n':
			print_mask |= PRINT_NODENAME;
			break;
		case 'r': 
			print_mask |= PRINT_RELEASE;
			break;
		case 's': 
			print_mask |= PRINT_SYSNAME;
			break;
		case 'v':
			print_mask |= PRINT_VERSION;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	
	if (optind != argc) {
		usage();
		/* NOTREACHED */
	}

	if (!print_mask) {
		print_mask = PRINT_SYSNAME;
	}

	if (uname(&u)) {
		err(1, NULL);
		/* NOTREACHED */
	}

	if (print_mask & PRINT_SYSNAME) {
		space++;
		fputs(u.sysname, stdout);
	}
	if (print_mask & PRINT_NODENAME) {
		if (space++) putchar(' ');
		fputs(u.nodename, stdout);
	}
	if (print_mask & PRINT_RELEASE) {
		if (space++) putchar(' ');
		fputs(u.release, stdout);
	}
	if (print_mask & PRINT_VERSION) {
		if (space++) putchar(' ');
		fputs(u.version, stdout);
	}
	if (print_mask & PRINT_MACHINE) {
		if (space++) putchar(' ');
		fputs(u.machine, stdout);
	}
	putchar('\n');

	exit(0);
	/* NOTREACHED */
}

static void
usage()
{
	fprintf(stderr, "usage: uname [-amnrsv]\n");
	exit(1);
}
