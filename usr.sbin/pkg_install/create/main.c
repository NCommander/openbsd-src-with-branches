/*	$OpenBSD: main.c,v 1.9 2001/04/08 16:45:46 espie Exp $	*/

#ifndef lint
static const char *rcsid = "$OpenBSD: main.c,v 1.9 2001/04/08 16:45:46 espie Exp $";
#endif

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
 *
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * This is the create module.
 *
 */

#include <err.h>
#include "lib.h"
#include "create.h"

static char Options[] = "Ohvf:p:P:C:c:d:i:k:r:t:X:D:m:s:";

char	*Prefix		= NULL;
char	*Comment	= NULL;
char	*Desc		= NULL;
char	*SrcDir		= NULL;
char	*Display	= NULL;
char	*Install	= NULL;
char	*DeInstall	= NULL;
char	*Contents	= NULL;
char	*Require	= NULL;
char	*ExcludeFrom	= NULL;
char	*Mtree		= NULL;
char	*Pkgdeps	= NULL;
char	*Pkgcfl		= NULL;
char	PlayPen[FILENAME_MAX];
size_t	PlayPenSize	= sizeof(PlayPen);
int	Dereference	= 0;
int	PlistOnly	= 0;

static void usage(void);

int
main(int argc, char **argv)
{
    int ch;
    char **pkgs, **start;

    pkgs = start = argv;
    while ((ch = getopt(argc, argv, Options)) != -1)
	switch(ch) {
	case 'v':
	    Verbose = TRUE;
	    break;

	case 'O':
	    PlistOnly = YES;
	    break;

	case 'p':
	    Prefix = optarg;
	    break;

	case 's':
	    SrcDir = optarg;
	    break;

	case 'f':
	    Contents = optarg;
	    break;

	case 'c':
	    Comment = optarg;
	    break;

	case 'd':
	    Desc = optarg;
	    break;

	case 'i':
	    Install = optarg;
	    break;

	case 'k':
	    DeInstall = optarg;
	    break;

	case 'r':
	    Require = optarg;
	    break;

	case 't':
	    strcpy(PlayPen, optarg);
	    break;

	case 'X':
	  /* XXX this won't work until someone adds the gtar -X option
	     (--exclude-from-file) to paxtar - so long it is disabled
	     in perform.c */
	    printf("WARNING: the -X option is not supported in OpenBSD\n");
	    ExcludeFrom = optarg;
	    break;

	case 'h':
	    Dereference = 1;
	    break;

	case 'D':
	    Display = optarg;
	    break;

	case 'm':
	    Mtree = optarg;
	    break;

	case 'P':
	    Pkgdeps = optarg;
	    break;

	case 'C':
		Pkgcfl = optarg;
		break;

	case '?':
	default:
	    usage();
	    break;
	}

    argc -= optind;
    argv += optind;

    /* Get all the remaining package names, if any */
    while (*argv)
	*pkgs++ = *argv++;

    /* If no packages, yelp */
    if (pkgs == start)
	pwarnx("missing package name"), usage();
    *pkgs = NULL;
    if (start[1])
	pwarnx("only one package name allowed ('%s' extraneous)", start[1]),
	usage();
    if (!pkg_perform(start)) {
	if (Verbose)
	    pwarnx("package creation failed");
	return 1;
    }
    else
	return 0;
}

static void
usage()
{
    fprintf(stderr, "%s\n%s\n%s\n%s\n",
"usage: pkg_create [-Ohv] [-P dpkgs] [-C cpkgs] [-p prefix] [-f contents]",
"                  [-i iscript] [-k dscript] [-r rscript] [-t template]",
"                  [-X excludefile] [-D displayfile] [-m mtreefile]",
"                  -c comment -d description -f packlist pkg-name");
    exit(1);
}
