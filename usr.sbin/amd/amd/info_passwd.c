/*
 * Copyright (c) 1990 Jan-Simon Pendry
 * Copyright (c) 1990 Imperial College of Science, Technology & Medicine
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry at Imperial College, London.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)info_passwd.c	8.1 (Berkeley) 6/6/93
 *	$Id: info_passwd.c,v 1.3 1994/06/13 20:47:29 mycroft Exp $
 */

/*
 * Get info from password "file"
 *
 * This is experimental and probably doesn't
 * do what you expect.
 */

#include "am.h"

#ifdef HAS_PASSWD_MAPS
#include <pwd.h>

#define	PASSWD_MAP	"/etc/passwd"

/*
 * Nothing to probe - check the map name is PASSWD_MAP.
 */
int passwd_init P((char *map, time_t *tp));
int passwd_init(map, tp)
char *map;
time_t *tp;
{
	*tp = 0;
	return strcmp(map, PASSWD_MAP) == 0 ? 0 : ENOENT;
}


/*
 * Grab the entry via the getpwname routine
 * Modify time is ignored by passwd - XXX
 */
int passwd_search P((mnt_map *m, char *map, char *key, char **pval, time_t *tp));
int passwd_search(m, map, key, pval, tp)
mnt_map *m;
char *map;
char *key;
char **pval;
time_t *tp;
{
	char *dir = 0;
	struct passwd *pw;
	if (strcmp(key, "/defaults") == 0) {
		*pval = strdup("type:=nfs");
		return 0;
	}

	pw = getpwnam(key);
	if (pw) {
		/*
		 * We chop the home directory up as follows:
		 * /anydir/dom1/dom2/dom3/user
		 *
		 * and return
		 * rfs:=/anydir/dom3;rhost:=dom3.dom2.dom1;sublink:=user
		 *
		 * This allows cross-domain entries in your passwd file.
		 * ... but forget about security!
		 */
		char *user;
		char *p, *q;
		char val[MAXPATHLEN];
		char rhost[MAXHOSTNAMELEN];
		dir = strdup(pw->pw_dir);
		/*
		 * Find user name.  If no / then Invalid...
		 */
		user = strrchr(dir, '/');
		if (!user)
			goto enoent;
		*user++ = '\0';
		/*
		 * Find start of host "path".  If no / then Invalid...
		 */ 
		p = strchr(dir+1, '/');
		if (!p)
			goto enoent;
		*p++ = '\0';
		/*
		 * At this point, p is dom1/dom2/dom3
		 * Copy, backwards, into rhost replacing
		 * / with .
		 */
		rhost[0] = '\0';
		do {
			q = strrchr(p, '/');
			if (q) {
				strcat(rhost, q + 1);
				strcat(rhost, ".");
				*q = '\0';
			} else {
				strcat(rhost, p);
			}
		} while (q);
		/*
		 * Sanity check
		 */
		if (*rhost == '\0' || *user == '\0' || *dir == '\0')
			goto enoent;
		/*
		 * Make up return string
		 */
		q = strchr(rhost, '.');
		if (q)
			*q = '\0';
		sprintf(val, "rfs:=%s/%s;rhost:=%s;sublink:=%s;fs:=${autodir}%s",
			dir, rhost, rhost, user, pw->pw_dir);
		if (q)
			*q = '.';
		*pval = strdup(val);
		return 0;
	}

enoent:
	if (dir)
		free(dir);

	return ENOENT;
}
#endif /* HAS_PASSWD_MAPS */
