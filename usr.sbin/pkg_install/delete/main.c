/*	$OpenBSD: main.c,v 1.5 1998/04/04 22:44:18 deraadt Exp $	*/

#ifndef lint
static char *rcsid = "$OpenBSD: main.c,v 1.5 1998/04/04 22:44:18 deraadt Exp $";
#endif

/*
 *
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the delete module.
 *
 */

#include "lib.h"
#include "delete.h"

static char Options[] = "hvDdnfp:";

char	*Prefix		= NULL;
Boolean	NoDeInstall	= FALSE;
Boolean	CleanDirs	= FALSE;
Boolean	Force		= FALSE;

int
main(int argc, char **argv)
{
    int ch, error;
    char **pkgs, **start;
    char *prog_name = argv[0];

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1)
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'f':
	    Force = TRUE;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 'D':
	    NoDeInstall = TRUE;
	    break;

	case 'd':
	    CleanDirs = TRUE;
	    break;

	case 'n':
	    Fake = TRUE;
	    Verbose = TRUE;
	    break;

	case 'h':
	case '?':
	default:
	    usage(prog_name, NULL);
	    break;
	}

    argc -= optind;
    argv += optind;

    /* Get all the remaining package names, if any */
    /* Get all the remaining package names, if any */
    while (*argv)
	*pkgs++ = *argv++;

    /* If no packages, yelp */
    if (pkgs == start)
	usage(prog_name, "Missing package name(s)");
    *pkgs = NULL;
    if (!Fake && getuid() != 0)
	errx(1, "You must be root to delete packages.");
    if ((error = pkg_perform(start)) != 0) {
	if (Verbose)
	    fprintf(stderr, "%d package deletion(s) failed.\n", error);
	return error;
    }
    else
	return 0;
}

void
usage(const char *name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (fmt) {
	fprintf(stderr, "%s: ", name);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
    }
    va_end(args);
    fprintf(stderr, "usage: %s [-vDdnf] [-p prefix] pkg ...\n", name);
    exit(1);
}
