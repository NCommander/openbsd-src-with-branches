/*	$OpenBSD$	*/

/*
 * Name:	MG 2a
 *		Directory management functions
 * Created:	Ron Flax (ron@vsedev.vse.com)
 *		Modified for MG 2a by Mic Kaczmarczik 03-Aug-1987
 */

#include "def.h"

#ifndef NO_DIR
char           *wdir;
static char     cwd[NFILEN];

/*
 * Initialize anything the directory management routines need
 */
VOID
dirinit()
{

	if (!(wdir = getcwd(cwd, sizeof(cwd))))
		panic("Can't get current directory!");
}

/*
 * Change current working directory
 */
/* ARGSUSED */
int
changedir(f, n)
	int    f, n;
{
	int    s;
	char   bufc[NPAT];

	if ((s = ereply("Change default directory: ", bufc, NPAT)) != TRUE)
		return (s);
	if (bufc[0] == '\0')
		(VOID) strcpy(bufc, wdir);
	if (chdir(bufc) == -1) {
		ewprintf("Can't change dir to %s", bufc);
		return (FALSE);
	} else {
		if (!(wdir = getcwd(cwd, sizeof(cwd))))
			panic("Can't get current directory!");
		ewprintf("Current directory is now %s", wdir);
		return (TRUE);
	}
}

/*
 * Show current directory
 */
/* ARGSUSED */
int
showcwdir(f, n)
{

	ewprintf("Current directory: %s", wdir);
	return (TRUE);
}
#endif
