/*	$OpenBSD: extern.h,v 1.3 2005/12/27 04:18:07 tedu Exp $ */

/*
 * Written by Raymond Lai <ray@cyth.net>.
 * Public domain.
 */

extern FILE		*outfile;	/* file to save changes to */
extern const char	*tmpdir;

int eparse(const char *, const char *, const char *);
