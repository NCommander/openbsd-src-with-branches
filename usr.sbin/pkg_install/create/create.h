/* $OpenBSD: create.h,v 1.3 1998/10/13 23:09:50 marc Exp $ */

/*
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
 * Include and define various things wanted by the create command.
 *
 */

#ifndef _INST_CREATE_H_INCLUDE
#define _INST_CREATE_H_INCLUDE

extern char	*Prefix;
extern char	*Comment;
extern char	*Desc;
extern char	*Display;
extern char	*Install;
extern char	*DeInstall;
extern char	*Contents;
extern char	*Require;
extern char	*SrcDir;
extern char	*ExcludeFrom;
extern char	*Mtree;
extern char	*Pkgdeps;
extern char	*Pkgcfl;
extern char	*BaseDir;
extern char	PlayPen[];
extern size_t	PlayPenSize;
extern int	Dereference;
extern int	PlistOnly;

void		check_list(char *, package_t *);
int		pkg_perform(char **);
void		copy_plist(char *, package_t *);

#endif	/* _INST_CREATE_H_INCLUDE */
