/* fsusage.c -- return space usage of mounted filesystems
   Copyright (C) 1991, 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   This file was modified slightly by Ian Lance Taylor, December 1992,
   and again July 1995, for use with Taylor UUCP.  */

#include "uucp.h"
#include "uudefs.h"
#include "sysdep.h"
#include "fsusg.h"

int statfs ();

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#if HAVE_SYS_FILSYS_H
#include <sys/filsys.h>		/* SVR2.  */
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#if HAVE_SYS_DUSTAT_H		/* AIX PS/2.  */
#include <sys/dustat.h>
#endif

#if HAVE_SYS_STATVFS_H		/* SVR4.  */
#include <sys/statvfs.h>
int statvfs ();
#endif

#if HAVE_USTAT_H		/* SVR2 and others.  */
#include <ustat.h>
#endif

#if STAT_DISK_SPACE		/* QNX.  */
#include <sys/disk.h>
#include <errno.h>
#endif

#define STAT_NONE 0

#if ! STAT_STATFS3_OSF1
#if ! STAT_STATFS2_FS_DATA
#if ! STAT_STATFS2_BSIZE
#if ! STAT_STATFS2_FSIZE
#if ! STAT_STATFS4
#if ! STAT_STATVFS
#if ! STAT_DISK_SPACE
#if ! STAT_USTAT
#undef STAT_NONE
#define STAT_NONE 1
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#if ! STAT_NONE

static long adjust_blocks P((long blocks, int fromsize, int tosize));

/* Return the number of TOSIZE-byte blocks used by
   BLOCKS FROMSIZE-byte blocks, rounding away from zero.
   TOSIZE must be positive.  Return -1 if FROMSIZE is not positive.  */

static long
adjust_blocks (blocks, fromsize, tosize)
     long blocks;
     int fromsize, tosize;
{
  if (tosize <= 0)
    abort ();
  if (fromsize <= 0)
    return -1;
								    
  if (fromsize == tosize)	/* E.g., from 512 to 512.  */
    return blocks;
  else if (fromsize > tosize)	/* E.g., from 2048 to 512.  */
    return blocks * (fromsize / tosize);
  else				/* E.g., from 256 to 512.  */
    return (blocks + (blocks < 0 ? -1 : 1)) / (tosize / fromsize);
}

#endif

/* Fill in the fields of FSP with information about space usage for
   the filesystem on which PATH resides.
   DISK is the device on which PATH is mounted, for space-getting
   methods that need to know it.
   Return 0 if successful, -1 if not. */

int
get_fs_usage (path, disk, fsp)
     char *path, *disk;
     struct fs_usage *fsp;
{
#if STAT_NONE
  return -1;
#endif

#if STAT_STATFS3_OSF1
  struct statfs fsd;

  if (statfs (path, &fsd, sizeof (struct statfs)) != 0)
    return -1;
#define CONVERT_BLOCKS(b) adjust_blocks ((b), fsd.f_fsize, 512)
#endif /* STAT_STATFS3_OSF1 */

#if STAT_STATFS2_FS_DATA	/* Ultrix.  */
  struct fs_data fsd;

  if (statfs (path, &fsd) != 1)
    return -1;
#define CONVERT_BLOCKS(b) adjust_blocks ((long) (b), 1024, 512)
  fsp->fsu_blocks = CONVERT_BLOCKS (fsd.fd_req.btot);
  fsp->fsu_bfree = CONVERT_BLOCKS (fsd.fd_req.bfree);
  fsp->fsu_bavail = CONVERT_BLOCKS (fsd.fd_req.bfreen);
  fsp->fsu_files = fsd.fd_req.gtot;
  fsp->fsu_ffree = fsd.fd_req.gfree;
#endif

#if STAT_STATFS2_BSIZE		/* 4.3BSD, SunOS 4, HP-UX, AIX.  */
  struct statfs fsd;

  if (statfs (path, &fsd) < 0)
    return -1;
#define CONVERT_BLOCKS(b) adjust_blocks ((b), fsd.f_bsize, 512)
#endif

#if STAT_STATFS2_FSIZE		/* 4.4BSD.  */
  struct statfs fsd;

  if (statfs (path, &fsd) < 0)
    return -1;
#define CONVERT_BLOCKS(b) adjust_blocks ((b), fsd.f_fsize, 512)
#endif

#if STAT_STATFS4		/* SVR3, Dynix, Irix.  */
  struct statfs fsd;

  if (statfs (path, &fsd, sizeof fsd, 0) < 0)
    return -1;
  /* Empirically, the block counts on most SVR3 and SVR3-derived
     systems seem to always be in terms of 512-byte blocks,
     no matter what value f_bsize has.  */
# if _AIX
#  define CONVERT_BLOCKS(b) adjust_blocks ((b), fsd.f_bsize, 512)
# else
#  define CONVERT_BLOCKS(b) (b)
#  ifndef _SEQUENT_		/* _SEQUENT_ is DYNIX/ptx.  */
#   ifndef DOLPHIN		/* DOLPHIN 3.8.alfa/7.18 has f_bavail */
#    define f_bavail f_bfree
#   endif
#  endif
# endif
#endif

#if STAT_STATVFS		/* SVR4.  */
  struct statvfs fsd;

  if (statvfs (path, &fsd) < 0)
    return -1;
  /* f_frsize isn't guaranteed to be supported.  */
#define CONVERT_BLOCKS(b) \
  adjust_blocks ((b), fsd.f_frsize ? fsd.f_frsize : fsd.f_bsize, 512)
#endif

#if STAT_DISK_SPACE		/* QNX.  */
  int o;
  int iret;
  long cfree_blocks, ctotal_blocks;
  char *zpath;
  char *zslash;
    
  zpath = zbufcpy (path);
  while ((o = open (zpath, O_RDONLY, 0)) == -1
	 && errno == ENOENT)
    {
      /* The named file doesn't exist, so we can't open it.  Try the
	 directory containing it. */
      if ((strcmp ("/", zpath) == 0)
	  || (strcmp (zpath, ".") == 0)
	  || (strcmp (zpath, "") == 0)
	  /* QNX peculiarity: "//2" means root on node 2 */
	  || ((strncmp (zpath, "//", 2) == 0)
	      && (strchr (zpath + 2, '/') == NULL)))
	{
	  /* We can't shorten this! */
	  break;
	}

      /* Shorten the pathname by one component and try again. */
      zslash = strrchr (zpath, '/');
      if (zslash == NULL)
	{
	  /* Try the current directory.  We can open directories. */
	  zpath[0] = '.';
	  zpath[1] = '\0';
	}
      else if (zslash == zpath)
	{
	  /* Try the root directory. */
	  zpath[0] = '/';
	  zpath[1] = '\0';
	}
      else
	{
	  /* Chop off last path component. */
	  zslash[0] = '\0';
	}
    }
  if (o == -1)
    {
      ulog (LOG_ERROR, "get_fs_usage: open (%s) failed: %s", zpath,
	    strerror (errno));
      ubuffree (zpath);
      return -1;
    }
  ubuffree (zpath);

  iret = disk_space (o, &cfree_blocks, &ctotal_blocks);
  (void) close (o);
  if (iret == -1)
    {
      ulog (LOG_ERROR, "get_fs_usage: disk_space failed: %s",
	    strerror (errno));
      return -1;
    }

  fsp->fsu_blocks = ctotal_blocks;
  fsp->fsu_bfree = cfree_blocks;
  fsp->fsu_bavail = cfree_blocks;
    
  /* QNX has no limit on the number of inodes.  Most inodes are stored
     directly in the directory entry. */
  fsp->fsu_files = -1;
  fsp->fsu_ffree = -1;
#endif /* STAT_DISK_SPACE */

#if STAT_USTAT
  struct stat sstat;
  struct ustat s;

  if (stat (path, &sstat) < 0
      || ustat (sstat.st_dev, &s) < 0)
    return -1;
  fsp->fsu_blocks = -1;
  fsp->fsu_bfree = s.f_tfree;
  fsp->fsu_bavail = s.f_tfree;
  fsp->fsu_files = -1;
  fsp->fsu_ffree = -1;
#endif

#if ! STAT_STATFS2_FS_DATA /* ! Ultrix */
#if ! STAT_DISK_SPACE
#if ! STAT_USTAT
#if ! STAT_NONE
  fsp->fsu_blocks = CONVERT_BLOCKS (fsd.f_blocks);
  fsp->fsu_bfree = CONVERT_BLOCKS (fsd.f_bfree);
  fsp->fsu_bavail = CONVERT_BLOCKS (fsd.f_bavail);
  fsp->fsu_files = fsd.f_files;
  fsp->fsu_ffree = fsd.f_ffree;
#endif
#endif
#endif
#endif

  return 0;
}

#ifdef _AIX
#ifdef _I386
/* AIX PS/2 does not supply statfs.  */

int
statfs (path, fsb)
     char *path;
     struct statfs *fsb;
{
  struct stat stats;
  struct dustat fsd;

  if (stat (path, &stats))
    return -1;
  if (dustat (stats.st_dev, 0, &fsd, sizeof (fsd)))
    return -1;
  fsb->f_type   = 0;
  fsb->f_bsize  = fsd.du_bsize;
  fsb->f_blocks = fsd.du_fsize - fsd.du_isize;
  fsb->f_bfree  = fsd.du_tfree;
  fsb->f_bavail = fsd.du_tfree;
  fsb->f_files  = (fsd.du_isize - 2) * fsd.du_inopb;
  fsb->f_ffree  = fsd.du_tinode;
  fsb->f_fsid.val[0] = fsd.du_site;
  fsb->f_fsid.val[1] = fsd.du_pckno;
  return 0;
}
#endif /* _I386 */
#endif /* _AIX */
