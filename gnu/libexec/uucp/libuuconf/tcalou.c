/* tcalou.c
   Find callout login name and password from Taylor UUCP configuration files.

   Copyright (C) 1992, 1993 Ian Lance Taylor

   This file is part of the Taylor UUCP uuconf library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License
   as published by the Free Software Foundation; either version 2 of
   the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucnfi.h"

#if USE_RCS_ID
const char _uuconf_tcalou_rcsid[] = "$Id: tcalou.c,v 1.3 1995/08/24 05:22:02 jtc Exp $";
#endif

#include <errno.h>

static int icsys P((pointer pglobal, int argc, char **argv, pointer pvar,
		    pointer pinfo));

/* Find the callout login name and password for a system from the
   Taylor UUCP configuration files.  */

int
uuconf_taylor_callout (pglobal, qsys, pzlog, pzpass)
     pointer pglobal;
     const struct uuconf_system *qsys;
     char **pzlog;
     char **pzpass;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  boolean flookup;
  struct uuconf_cmdtab as[2];
  char **pz;
  int iret;
  pointer pinfo;

  *pzlog = NULL;
  *pzpass = NULL;

  flookup = FALSE;

  if (qsys->uuconf_zcall_login != NULL)
    {
      if (strcmp (qsys->uuconf_zcall_login, "*") == 0)
	flookup = TRUE;
      else
	{
	  *pzlog = strdup (qsys->uuconf_zcall_login);
	  if (*pzlog == NULL)
	    {
	      qglobal->ierrno = errno;
	      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	    }
	}
    }

  if (qsys->uuconf_zcall_password != NULL)
    {
      if (strcmp (qsys->uuconf_zcall_password, "*") == 0)
	flookup = TRUE;
      else
	{
	  *pzpass = strdup (qsys->uuconf_zcall_password);
	  if (*pzpass == NULL)
	    {
	      qglobal->ierrno = errno;
	      if (*pzlog != NULL)
		{
		  free ((pointer) *pzlog);
		  *pzlog = NULL;
		}
	      return UUCONF_MALLOC_FAILED | UUCONF_ERROR_ERRNO;
	    }
	}
    }

  if (! flookup)
    {
      if (*pzlog == NULL && *pzpass == NULL)
	return UUCONF_NOT_FOUND;
      return UUCONF_SUCCESS;
    }

  as[0].uuconf_zcmd = qsys->uuconf_zname;
  as[0].uuconf_itype = UUCONF_CMDTABTYPE_FN | 0;
  if (*pzlog == NULL)
    as[0].uuconf_pvar = (pointer) pzlog;
  else
    as[0].uuconf_pvar = NULL;
  as[0].uuconf_pifn = icsys;

  as[1].uuconf_zcmd = NULL;

  if (*pzpass == NULL)
    pinfo = (pointer) pzpass;
  else
    pinfo = NULL;

  iret = UUCONF_SUCCESS;

  for (pz = qglobal->qprocess->pzcallfiles; *pz != NULL; pz++)
    {
      FILE *e;

      e = fopen (*pz, "r");
      if (e == NULL)
	{
	  if (FNO_SUCH_FILE ())
	    continue;
	  qglobal->ierrno = errno;
	  iret = UUCONF_FOPEN_FAILED | UUCONF_ERROR_ERRNO;
	  break;
	}

      iret = uuconf_cmd_file (pglobal, e, as, pinfo,
			      (uuconf_cmdtabfn) NULL, 0,
			      qsys->uuconf_palloc);
      (void) fclose (e);

      if (iret != UUCONF_SUCCESS)
	break;
      if (*pzlog != NULL)
	break;
    }

  if (iret != UUCONF_SUCCESS)
    {
      qglobal->zfilename = *pz;
      return iret | UUCONF_ERROR_FILENAME;
    }

  if (*pzlog == NULL && *pzpass == NULL)
    return UUCONF_NOT_FOUND;

  return UUCONF_SUCCESS;
}

/* Copy the login name and password onto the heap and set the
   pointers.  The pzlog argument is passed in pvar, and the pzpass
   argument is passed in pinfo.  */

static int
icsys (pglobal, argc, argv, pvar, pinfo)
     pointer pglobal;
     int argc;
     char **argv;
     pointer pvar;
     pointer pinfo;
{
  struct sglobal *qglobal = (struct sglobal *) pglobal;
  char **pzlog = (char **) pvar;
  char **pzpass = (char **) pinfo;

  if (argc < 2 || argc > 3)
    return UUCONF_SYNTAX_ERROR | UUCONF_CMDTABRET_EXIT;

  if (pzlog != NULL)
    {
      *pzlog = strdup (argv[1]);
      if (*pzlog == NULL)
	{
	  qglobal->ierrno = errno;
	  return (UUCONF_MALLOC_FAILED
		  | UUCONF_ERROR_ERRNO
		  | UUCONF_CMDTABRET_EXIT);
	}
    }

  if (pzpass != NULL)
    {
      if (argc < 3)
	*pzpass = strdup ("");
      else
	*pzpass = strdup (argv[2]);
      if (*pzpass == NULL)
	{
	  qglobal->ierrno = errno;
	  if (pzlog != NULL)
	    {
	      free ((pointer) *pzlog);
	      *pzlog = NULL;
	    }
	  return (UUCONF_MALLOC_FAILED
		  | UUCONF_ERROR_ERRNO
		  | UUCONF_CMDTABRET_EXIT);
	}
    }

  return UUCONF_CMDTABRET_EXIT;
}
