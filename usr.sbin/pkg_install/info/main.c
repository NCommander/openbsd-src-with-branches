/*	$OpenBSD: main.c,v 1.7 1997/12/14 10:04:01 gene Exp $	*/

#ifndef lint
static char *rcsid = "$OpenBSD: main.c,v 1.7 1997/12/14 10:04:01 gene Exp $";
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
 * This is the add module.
 *
 */

#include "lib.h"
#include "info.h"

static char Options[] = "acdDe:fikrRpLqImvhl:";

int	Flags		= 0;
Boolean AllInstalled	= FALSE;
Boolean Quiet		= FALSE;
char	*InfoPrefix	= "";
char 	PlayPen[FILENAME_MAX];
char 	*CheckPkg	= NULL;

int
main(int argc, char **argv)
{
    int ch;
    char **pkgs, **start;
    char *prog_name = argv[0];

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1)
	switch(ch) {
	case 'a':
	    AllInstalled = TRUE;
	    break;

	case 'v':
	    Verbose = TRUE;
	    /* Reasonable definition of 'everything' */
	    Flags = SHOW_COMMENT | SHOW_DESC | SHOW_PLIST | SHOW_INSTALL |
		SHOW_DEINSTALL | SHOW_REQUIRE | SHOW_DISPLAY | SHOW_MTREE;
	    break;

	case 'I':
	    Flags |= SHOW_INDEX;
	    break;

	case 'p':
	    Flags |= SHOW_PREFIX;
	    break;

	case 'c':
	    Flags |= SHOW_COMMENT;
	    break;

	case 'd':
	    Flags |= SHOW_DESC;
	    break;

	case 'D':
	    Flags |= SHOW_DISPLAY;
	    break;

	case 'f':
	    Flags |= SHOW_PLIST;
	    break;

	case 'i':
	    Flags |= SHOW_INSTALL;
	    break;

	case 'k':
	    Flags |= SHOW_DEINSTALL;
	    break;

	case 'r':
	    Flags |= SHOW_REQUIRE;
	    break;

	case 'R':
	    Flags |= SHOW_REQBY;
	    break;

	case 'L':
	    Flags |= SHOW_FILES;
	    break;

	case 'm':
	    Flags |= SHOW_MTREE;
	    break;

	case 'l':
	    InfoPrefix = optarg;
	    break;

	case 'q':
	    Quiet = TRUE;
	    break;

	case 't':
	    strncpy(PlayPen, optarg, FILENAME_MAX);
	    break;

	case 'e':
	    CheckPkg = optarg;
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
    while (*argv)
	*pkgs++ = *argv++;

    /* Set some reasonable defaults */
    if (!Flags) {
	Flags |= SHOW_INDEX;
	if (pkgs == start)
	    AllInstalled = TRUE;
    }
    *pkgs = NULL;
    return pkg_perform(start);
}

void
usage(const char *name, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (fmt) {
	fprintf(stderr, "%s: ", name);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n\n");
    }
    va_end(args);
    fprintf(stderr, "Usage: %s [args] pkg [ .. pkg ]\n", name);
    fprintf(stderr, "\t-a         show all installed packages (if any)\n");
    fprintf(stderr, "\t-I         print 'index' of packages\n");
    fprintf(stderr, "\t-c         print `one line comment'\n");
    fprintf(stderr, "\t-d         print description\n");
    fprintf(stderr, "\t-D         print install notice\n");
    fprintf(stderr, "\t-f         show packing list\n");
    fprintf(stderr, "\t-i         show install script\n");
    fprintf(stderr, "\t-k         show deinstall script\n");
    fprintf(stderr, "\t-r         show requirements script\n");
    fprintf(stderr, "\t-R         show packages depending on this package\n");
    fprintf(stderr, "\t-p         show prefix\n");
    fprintf(stderr, "\t-l <str>   Prefix each info catagory with <str>\n");
    fprintf(stderr, "\t-L         show installed files\n");
    fprintf(stderr, "\t-q         minimal output (``quiet'' mode)\n");
    fprintf(stderr, "\t-v         show all information\n");
    fprintf(stderr, "\t-t temp    use temp as template for mktemp()\n");
    fprintf(stderr, "\t-e pkg     returns 0 if pkg is installed, 1 otherwise\n");
    fprintf(stderr, "\t[default: args = -a -I]\n");
    exit(1);
}
