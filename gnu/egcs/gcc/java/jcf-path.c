/* Handle CLASSPATH, -classpath, and path searching.

   Copyright (C) 1998, 1999  Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  

Java and all Java-based marks are trademarks or registered trademarks
of Sun Microsystems, Inc. in the United States and other countries.
The Free Software Foundation is independent of Sun Microsystems, Inc.  */

/* Written by Tom Tromey <tromey@cygnus.com>, October 1998.  */

#include "config.h"
#include "system.h"

#include "jcf.h"

/* Some boilerplate that really belongs in a header.  */

#ifndef GET_ENV_PATH_LIST
#define GET_ENV_PATH_LIST(VAR,NAME)	do { (VAR) = getenv (NAME); } while (0)
#endif

/* By default, colon separates directories in a path.  */
#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR ':'
#endif

#ifndef DIR_SEPARATOR
#define DIR_SEPARATOR '/'
#endif



/* Possible flag values.  */
#define FLAG_SYSTEM 1
#define FLAG_ZIP    2

/* We keep linked lists of directory names.  A ``directory'' can be
   either an ordinary directory or a .zip file.  */
struct entry
{
  char *name;
  int flags;
  struct entry *next;
};

/* We support several different ways to set the class path.

   built-in system directory (only libgcj.zip)
   CLASSPATH environment variable
   -CLASSPATH overrides CLASSPATH
   -classpath option - overrides CLASSPATH, -CLASSPATH, and built-in
   -I prepends path to list

   We implement this by keeping several path lists, and then simply
   ignoring the ones which are not relevant.  */

/* This holds all the -I directories.  */
static struct entry *include_dirs;

/* This holds the CLASSPATH environment variable.  */
static struct entry *classpath_env;

/* This holds the -CLASSPATH command-line option.  */
static struct entry *classpath_u;

/* This holds the -classpath command-line option.  */
static struct entry *classpath_l;

/* This holds the default directories.  Some of these will have the
   "system" flag set.  */
static struct entry *sys_dirs;

/* This is the sealed list.  It is just a combination of other lists.  */
static struct entry *sealed;

/* We keep track of the longest path we've seen.  */
static int longest_path = 0;



static void
free_entry (entp)
     struct entry **entp;
{
  struct entry *e, *n;

  for (e = *entp; e; e = n)
    {
      n = e->next;
      free (e->name);
      free (e);
    }
  *entp = NULL;
}

static void
append_entry (entp, ent)
     struct entry **entp;
     struct entry *ent;
{
  /* It doesn't matter if this is slow, since it is run only at
     startup, and then infrequently.  */
  struct entry *e;

  /* Find end of list.  */
  for (e = *entp; e && e->next; e = e->next)
    ;

  if (e)
    e->next = ent;
  else
    *entp = ent;
}

static void
add_entry (entp, filename, is_system)
     struct entry **entp;
     char *filename;
     int is_system;
{
  int len;
  struct entry *n;

  n = (struct entry *) ALLOC (sizeof (struct entry));
  n->flags = is_system ? FLAG_SYSTEM : 0;
  n->next = NULL;

  len = strlen (filename);
  if (len > 4 && (strcmp (filename + len - 4, ".zip") == 0
		  || strcmp (filename + len - 4, ".jar") == 0))
    {
      n->flags |= FLAG_ZIP;
      /* If the user uses -classpath then he'll have to include
	 libgcj.zip in the value.  We check for this in a simplistic
	 way.  Symlinks will fool this test.  This is only used for
	 -MM and -MMD, so it probably isn't terribly important.  */
      if (! strcmp (filename, LIBGCJ_ZIP_FILE))
	n->flags |= FLAG_SYSTEM;
    }

  /* Note that we add a trailing separator to `.zip' names as well.
     This is a little hack that lets the searching code in jcf-io.c
     work more easily.  Eww.  */
  if (filename[len - 1] != '/' && filename[len - 1] != DIR_SEPARATOR)
    {
      char *f2 = (char *) alloca (len + 2);
      strcpy (f2, filename);
      f2[len] = DIR_SEPARATOR;
      f2[len + 1] = '\0';
      n->name = strdup (f2);
      ++len;
    }
  else
    n->name = strdup (filename);

  if (len > longest_path)
    longest_path = len;

  append_entry (entp, n);
}

static void
add_path (entp, cp, is_system)
     struct entry **entp;
     char *cp;
     int is_system;
{
  char *startp, *endp;

  if (cp)
    {
      char *buf = (char *) alloca (strlen (cp) + 3);
      startp = endp = cp;
      while (1)
	{
	  if (! *endp || *endp == PATH_SEPARATOR)
	    {
	      if (endp == startp)
		{
		  buf[0] = '.';
		  buf[1] = DIR_SEPARATOR;
		  buf[2] = '\0';
		}
	      else
		{
		  strncpy (buf, startp, endp - startp);
		  buf[endp - startp] = '\0';
		}
	      add_entry (entp, buf, is_system);
	      if (! *endp)
		break;
	      ++endp;
	      startp = endp;
	    }
	  else
	    ++endp;
	}
    }
}

/* Initialize the path module.  */
void
jcf_path_init ()
{
  char *cp;

  add_entry (&sys_dirs, ".", 0);
  add_entry (&sys_dirs, LIBGCJ_ZIP_FILE, 1);

  GET_ENV_PATH_LIST (cp, "CLASSPATH");
  add_path (&classpath_env, cp, 0);
}

/* Call this when -classpath is seen on the command line.  */
void
jcf_path_classpath_arg (path)
     char *path;
{
  free_entry (&classpath_l);
  add_path (&classpath_l, path, 0);
}

/* Call this when -CLASSPATH is seen on the command line.  */
void
jcf_path_CLASSPATH_arg (path)
     char *path;
{
  free_entry (&classpath_u);
  add_path (&classpath_u, path, 0);
}

/* Call this when -I is seen on the command line.  */
void
jcf_path_include_arg (path)
     char *path;
{
  add_entry (&include_dirs, path, 0);
}

/* We `seal' the path by linking everything into one big list.  Then
   we provide a way to iterate through the sealed list.  */
void
jcf_path_seal ()
{
  int do_system = 1;
  struct entry *secondary;

  sealed = include_dirs;
  include_dirs = NULL;

  if (classpath_l)
    {
      secondary = classpath_l;
      classpath_l = NULL;
      do_system = 0;
    }
  else if (classpath_u)
    {
      secondary = classpath_u;
      classpath_u = NULL;
    }
  else
    {
      secondary = classpath_env;
      classpath_env = NULL;
    }

  free_entry (&classpath_l);
  free_entry (&classpath_u);
  free_entry (&classpath_env);

  append_entry (&sealed, secondary);

  if (do_system)
    {
      append_entry (&sealed, sys_dirs);
      sys_dirs = NULL;
    }
  else
    free_entry (&sys_dirs);
}

void *
jcf_path_start ()
{
  return (void *) sealed;
}

void *
jcf_path_next (x)
     void *x;
{
  struct entry *ent = (struct entry *) x;
  return (void *) ent->next;
}

/* We guarantee that the return path will either be a zip file, or it
   will end with a directory separator.  */
char *
jcf_path_name (x)
     void *x;
{
  struct entry *ent = (struct entry *) x;
  return ent->name;
}

int
jcf_path_is_zipfile (x)
     void *x;
{
  struct entry *ent = (struct entry *) x;
  return (ent->flags & FLAG_ZIP);
}

int
jcf_path_is_system (x)
     void *x;
{
  struct entry *ent = (struct entry *) x;
  return (ent->flags & FLAG_SYSTEM);
}

int
jcf_path_max_len ()
{
  return longest_path;
}
