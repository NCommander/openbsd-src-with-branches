/*	$OpenBSD: read_bsd_terminfo.c,v 1.5 1999/03/02 06:23:29 millert Exp $	*/

/*
 * Copyright (c) 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
 * Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by SigmaSoft, Th.  Lockert.
 * 4. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: read_bsd_terminfo.c,v 1.5 1999/03/02 06:23:29 millert Exp $";
#endif

#include <curses.priv.h>
#include <tic.h>
#include <term.h>	/* lines, columns, cur_term */
#include <term_entry.h>

#define	_PATH_TERMINFO	"/usr/share/misc/terminfo"

/* Function prototypes for private functions, */
static int _nc_lookup_bsd_terminfo_entry __P((const char *const, const char *const, TERMTYPE *));

/*
 * Look up ``tn'' in the BSD terminfo.db file and fill in ``tp''
 * with the info we find there.
 * Returns 1 on success, 0 on failure.
 */
int
_nc_read_bsd_terminfo_entry(tn, filename, tp)
    const char *const tn;
    char *const filename;
    TERMTYPE *const tp;
{
    char **fname, *p;
    char   envterm[PATH_MAX];		/* local copy of $TERMINFO */
    char   hometerm[PATH_MAX];		/* local copy of $HOME/.terminfo */
    char  *pathvec[4];			/* list of possible terminfo files */
    size_t len;

    fname = pathvec;
    /* $TERMINFO may hold a path to a terminfo file */
    if (!issetugid() && (p = getenv("TERMINFO")) != NULL) {
	len = strlcpy(envterm, p, sizeof(envterm));
	if (len < sizeof(envterm))
	    *fname++ = envterm;
    }

    /* Also check $HOME/.terminfo if it exists */
    if (!issetugid() && (p = getenv("HOME")) != NULL) {
	len = snprintf(hometerm, sizeof(hometerm), "%s/.terminfo", p);
	if (len < sizeof(hometerm))
	    *fname++ = hometerm;
    }

    /* Finally we check the system terminfo file */
    *fname++ = _PATH_TERMINFO;
    *fname = NULL;

    /*
     * Lookup ``tn'' in each possible terminfo file until
     * we find it or reach the end.
     */
    for (fname = pathvec; *fname; fname++) {
	if (_nc_lookup_bsd_terminfo_entry(tn, *fname, tp) == 1) {
	    /* Set copyout parameter and return */
	    (void)strlcpy(filename, *fname, PATH_MAX);
	    return (1);
	}
    }
    return (0);
}

/*
 * Given a path /path/to/terminfo/X/termname, look up termname
 * /path/to/terminfo.db and fill in ``tp'' with the info we find there.
 * Returns 1 on success, 0 on failure.
 */
int
_nc_read_bsd_terminfo_file(filename, tp)
    const char *const filename;
    TERMTYPE *const tp;
{
    char path[PATH_MAX];		/* path to terminfo.db */
    char *tname;			/* name of terminal to look up */
    char *p;

    (void)strlcpy(path, filename, sizeof(path));

    /* Split filename into path and term name components. */
    if ((tname = strrchr(path, '/')) == NULL)
	return (0);
    *tname++ = '\0';
    if ((p = strrchr(path, '/')) == NULL)
	return (0);
    *p = '\0';

    return (_nc_lookup_bsd_terminfo_entry(tname, path, tp));
}

/*
 * Look up ``tn'' in the BSD terminfo file ``filename'' and fill in
 * ``tp'' with the info we find there.
 * Returns 1 on success, 0 on failure.
 */
static int
_nc_lookup_bsd_terminfo_entry(tn, filename, tp)
    const char *const tn;
    const char *const filename;
    TERMTYPE *const tp;
{
    char  *pathvec[2];
    char  *capbuf, *p;
    char   namecpy[MAX_NAME_SIZE+1];
    long   num;
    int    i;

    capbuf = NULL;
    pathvec[0] = (char *)filename;
    pathvec[1] = NULL;

    /* Don't prepent any hardcoded entries. */
    (void) cgetset(NULL);

    /* Lookup tn in filename */
    i = cgetent(&capbuf, pathvec, (char *)tn);      
    if (i == 0) {
	_nc_init_entry(tp);

	/* Set terminal name(s) */
	if ((p = strchr(capbuf, ':')) != NULL)
	    *p = '\0';
	if ((tp->str_table = tp->term_names = strdup(capbuf)) == NULL) {
	    if (capbuf)
		free(capbuf);
	    return (0);
	}
	_nc_set_type(_nc_first_name(tp->term_names));
	if (p)
	    *p = ':';

	/* Check for overly-long names and aliases */
	(void)strlcpy(namecpy, tp->term_names, sizeof(namecpy));
	if ((p = strrchr(namecpy, '|')) != (char *)NULL)
	    *p = '\0';
	p = strtok(namecpy, "|");
	if (strlen(p) > MAX_ALIAS)
	    _nc_warning("primary name may be too long");
	while ((p = strtok((char *)NULL, "|")) != (char *)NULL)
	    if (strlen(p) > MAX_ALIAS)
		_nc_warning("alias `%s' may be too long", p);

	/* Copy existing capabilities */
	for_each_boolean(i, tp)
	    if (cgetcap(capbuf, (char *)boolnames[i], ':') != NULL)
		tp->Booleans[i] = TRUE;
	for_each_number(i, tp)
	    if (cgetnum(capbuf, (char *)numnames[i], &num) == 0)
		tp->Numbers[i] = (short)num;
	for_each_string(i, tp)
	    if (cgetstr(capbuf, (char *)strnames[i], &p) >= 0)
		tp->Strings[i] = p;
	i = 0;
    }

    /* We are done with the returned getcap buffer now; free it */
    cgetclose();
    if (capbuf)
	free(capbuf);

    return ((i == 0));
}
