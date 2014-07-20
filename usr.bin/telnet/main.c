/*	$OpenBSD: main.c,v 1.24 2014/07/20 05:22:02 guenther Exp $	*/
/*	$NetBSD: main.c,v 1.5 1996/02/28 21:04:05 thorpej Exp $	*/

/*
 * Copyright (c) 1988, 1990, 1993
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

#include "telnet_locl.h"

#include <unistd.h>

int family = AF_UNSPEC;
int rtableid = -1;

/*
 * Initialize variables.
 */
    void
tninit()
{
    init_terminal();

    init_network();

    init_telnet();

    init_sys();
}

	void
usage()
{
	extern char *__progname;

	(void)fprintf(stderr,
	    "usage: %s [-4678acDdEKLr] [-b hostalias] [-e escapechar] "
	    "[-l user]\n"
	    "\t[-n tracefile] [-V rtable] [host [port]]\n",
	    __progname);

	exit(1);
}

/*
 * main.  Parse arguments, invoke the protocol or command parser.
 */

	int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	char *user, *alias;
	const char *errstr;

	tninit();		/* Clear out things */

	TerminalSaveState();

	if ((prompt = strrchr(argv[0], '/')))
		++prompt;
	else
		prompt = argv[0];

	user = alias = NULL;

	rlogin = (strncmp(prompt, "rlog", 4) == 0) ? '~' : _POSIX_VDISABLE;

	autologin = -1;

	while ((ch = getopt(argc, argv, "4678ab:cDdEe:KLl:n:rV:"))
	    != -1) {
		switch(ch) {
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case '7':
			eight = 0;
			break;
		case '8':
			eight = 3;	/* binary output and input */
			break;
		case 'a':
			autologin = 1;
			break;
		case 'b':
			alias = optarg;
			break;
		case 'c':
			skiprc = 1;
			break;
		case 'D': {
			/* sometimes we don't want a mangled display */
			char *p;
			if((p = getenv("DISPLAY")))
				env_define("DISPLAY", (unsigned char*)p);
			break;
		}
		case 'd':
			debug = 1;
			break;
		case 'E':
			rlogin = escape = _POSIX_VDISABLE;
			break;
		case 'e':
			set_escape_char(optarg);
			break;
		case 'K':
			autologin = 0;
			break;
		case 'L':
			eight |= 2;	/* binary output only */
			break;
		case 'l':
			autologin = -1;
			user = optarg;
			break;
		case 'n':
			SetNetTrace(optarg);
			break;
		case 'r':
			rlogin = '~';
			break;
		case 'V':
			rtableid = (int)strtonum(optarg, 0,
			    RT_TABLEID_MAX, &errstr);
			if (errstr) {
				fprintf(stderr, "%s: Warning: "
				    "-V ignored, rtable %s: %s\n",
				    prompt, errstr, optarg);
			}
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (autologin == -1)
		autologin = (rlogin == _POSIX_VDISABLE) ? 0 : 1;

	argc -= optind;
	argv += optind;

	if (argc) {
		char *args[7], **argp = args;

		if (argc > 2)
			usage();
		*argp++ = prompt;
		if (user) {
			*argp++ = "-l";
			*argp++ = user;
		}
		if (alias) {
			*argp++ = "-b";
			*argp++ = alias;
		}
		*argp++ = argv[0];		/* host */
		if (argc > 1)
			*argp++ = argv[1];	/* port */
		*argp = 0;

		if (setjmp(toplevel) != 0)
			Exit(0);
		if (tn(argp - args, args) == 1)
			return (0);
		else
			return (1);
	}
	(void)setjmp(toplevel);
	for (;;) {
		command(1, 0, 0);
	}
	return 0;
}
