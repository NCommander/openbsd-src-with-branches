/*	$OpenBSD$	*/

/*
 * Copyright (c) 1996, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <sys/types.h>
#include <err.h>
#include <string.h>
#include <unistd.h>

/*
 * Very simple little program, for encrypting passwords from the command
 * line.  Useful for scripts and such.
 */

extern char *__progname;
extern char *optarg;
extern int optind;

void usage()
{
    errx(1, "usage: %s [-m] [-s salt] [string]", __progname);
}

int main(argc, argv)
    int argc;
    char *argv[];
{
    int opt;
    int do_md5 = 0;
    char *salt = (char *)NULL;

    while ((opt = getopt(argc, argv, "ms:")) != -1) {
    	switch (opt) {
	case 'm':
	    do_md5 = 1;
	    break;
	case 's':
	    salt = optarg;
	    break;
	default:
	    usage();
	}
    }

    if (do_md5 && (salt != (char *)NULL))
	usage();

    if (!do_md5 && (salt == (char *)NULL))
	usage();

    if ((argc - optind) < 1) {
    	char line[BUFSIZ];

    	/* Encrypt stdin to stdout. */
	while (!feof(stdin) && (fgets(line, sizeof(line), stdin) != NULL)) {
	    if ((line[0] == '\0') || (line[0] == '\n'))
	    	continue;

	    /* Kill the newline. */
	    if (line[strlen(line)] == '\n')
	    	line[strlen(line)] = '\0';

	    fputs(crypt(line, (do_md5 ? "$1$" : salt)), stdout);
	    fputc('\n', stdout);
	}
    } else {
    	char *string;

    	/* Perhaps it isn't worth worrying about, but... */
    	string = strdup(argv[optind]);
    	if (string == (char *)NULL)
    	    err(1, NULL);
    	/* Wipe the argument. */
    	bzero(argv[optind], strlen(argv[optind]));

    	fputs(crypt(string, (do_md5 ? "$1$" : salt)), stdout);
    	fputc('\n', stdout);

    	/* Wipe our copy, before we free it. */
    	bzero(string, strlen(string));
    	free(string);
    }
    exit(0);
}
