/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 */

#include "cvs.h"

/* Determine the name of the RCS repository for directory DIR in the
   current working directory, or for the current working directory
   itself if DIR is NULL.  Returns the name in a newly-malloc'd
   string.  On error, gives a fatal error and does not return.
   UPDATE_DIR is the path from where cvs was invoked (for use in error
   messages), and should contain DIR as its last component.
   UPDATE_DIR can be NULL to signify the directory in which cvs was
   invoked.  */

char *
Name_Repository (dir, update_dir)
    char *dir;
    char *update_dir;
{
    FILE *fpin;
    char *ret, *xupdate_dir;
    char repos[PATH_MAX];
    char path[PATH_MAX];
    char tmp[PATH_MAX];
    char *cp;

    if (update_dir && *update_dir)
	xupdate_dir = update_dir;
    else
	xupdate_dir = ".";

    if (dir != NULL)
	(void) sprintf (tmp, "%s/%s", dir, CVSADM_REP);
    else
	(void) strcpy (tmp, CVSADM_REP);

    /*
     * The assumption here is that the repository is always contained in the
     * first line of the "Repository" file.
     */
    fpin = CVS_FOPEN (tmp, "r");

    if (fpin == NULL)
    {
	int save_errno = errno;
	char cvsadm[PATH_MAX];

	if (dir != NULL)
	    (void) sprintf (cvsadm, "%s/%s", dir, CVSADM);
	else
	    (void) strcpy (cvsadm, CVSADM);

	if (!isdir (cvsadm))
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (1, 0, "there is no version here; do '%s checkout' first",
		   program_name);
	}

	if (existence_error (save_errno))
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (1, 0, "*PANIC* administration files missing");
	}

	error (1, save_errno, "cannot open %s", tmp);
    }

    if (fgets (repos, PATH_MAX, fpin) == NULL)
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (1, errno, "cannot read %s", CVSADM_REP);
    }
    (void) fclose (fpin);
    if ((cp = strrchr (repos, '\n')) != NULL)
	*cp = '\0';			/* strip the newline */

    /*
     * If this is a relative repository pathname, turn it into an absolute
     * one by tacking on the CVSROOT environment variable. If the CVSROOT
     * environment variable is not set, die now.
     */
    if (strcmp (repos, "..") == 0 || strncmp (repos, "../", 3) == 0)
    {
	error (0, 0, "in directory %s:", xupdate_dir);
	error (0, 0, "`..'-relative repositories are not supported.");
	error (1, 0, "illegal source repository");
    }
    if (! isabsolute(repos))
    {
	if (CVSroot_original == NULL)
	{
	    error (0, 0, "in directory %s:", xupdate_dir);
	    error (0, 0, "must set the CVSROOT environment variable\n");
	    error (0, 0, "or specify the '-d' option to %s.", program_name);
	    error (1, 0, "illegal repository setting");
	}
	(void) strcpy (path, repos);
	(void) sprintf (repos, "%s/%s", CVSroot_directory, path);
    }

    /* allocate space to return and fill it in */
    strip_path (repos);
    ret = xstrdup (repos);
    return (ret);
}

/*
 * Return a pointer to the repository name relative to CVSROOT from a
 * possibly fully qualified repository
 */
char *
Short_Repository (repository)
    char *repository;
{
    if (repository == NULL)
	return (NULL);

    /* If repository matches CVSroot at the beginning, strip off CVSroot */
    /* And skip leading '/' in rep, in case CVSroot ended with '/'. */
    if (strncmp (CVSroot_directory, repository,
		 strlen (CVSroot_directory)) == 0)
    {
	char *rep = repository + strlen (CVSroot_directory);
	return (*rep == '/') ? rep+1 : rep;
    }
    else
	return (repository);
}
