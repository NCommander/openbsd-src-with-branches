/*	$OpenBSD: sysdef.h,v 1.9 2001/05/24 03:05:26 mickey Exp $	*/

/*
 *		POSIX system header file
 */
#include <sys/param.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define	KBLOCK	8192		/* Kill grow.			 */
#define	GOOD	0		/* Good exit status.		 */
#define SYMBLINK	1	/* Handle symbolic links	 */
#define	MAXPATH	PATH_MAX	/* Maximum length of path for chdir */

typedef int	RSIZE;		/* Type for file/region sizes	 */
typedef short	KCHAR;		/* Type for internal keystrokes	 */

#define MALLOCROUND(m)	(m+=7,m&=~7)	/* round up to 8 byte boundry	 */

#define	gettermtype()	getenv("TERM")	/* determine terminal type	 */

struct fileinfo {
	mode_t		fi_mode;
	uid_t		fi_uid;
	gid_t short	fi_gid;
};
