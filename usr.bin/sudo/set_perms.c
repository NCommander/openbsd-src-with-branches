/*
 * Copyright (c) 1994-1996,1998-2001 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <pwd.h>
#include <errno.h>
#include <grp.h>
#ifdef HAVE_LOGIN_CAP_H
# include <login_cap.h>
#endif

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: set_perms.c,v 1.9 2002/01/13 18:28:09 millert Exp $";
#endif /* lint */

/*
 * Prototypes
 */
static void runas_setup		__P((void));
static void fatal		__P((char *));

#if defined(_SC_SAVED_IDS) && defined(_SC_VERSION)
/*
 * Set real and effective uids and gids based on perm.
 * Since we have POSIX saved IDs we can get away with just
 * toggling the effective uid/gid unless we are headed for an exec().
 */
void
set_perms_posix(perm, sudo_mode)
    int perm;
    int sudo_mode;
{
    int error;

    switch (perm) {
	case PERM_ROOT:
				if (seteuid(0))
				    fatal("seteuid(0)");
			      	break;

	case PERM_FULL_ROOT:
				/* headed for exec() */
				(void) seteuid(0);
				if (setuid(0))
				    fatal("setuid(0)");
			      	break;

	case PERM_USER:
    	    	    	        (void) setegid(user_gid);
				if (seteuid(user_uid))
				    fatal("seteuid(user_uid)");
			      	break;
				
	case PERM_RUNAS:
				/* headed for exec(), assume euid == 0 */
				runas_setup();
				if (def_flag(I_STAY_SETUID))
				    error = seteuid(runas_pw->pw_uid);
				else
				    error = setuid(runas_pw->pw_uid);
				if (error)
				    fatal("unable to change to runas uid");
				break;

	case PERM_SUDOERS:
				/* assume euid == 0, ruid == user */
				if (setegid(SUDOERS_GID))
				    fatal("unable to change to sudoers gid");

				/*
				 * If SUDOERS_UID == 0 and SUDOERS_MODE
				 * is group readable we use a non-zero
				 * uid in order to avoid NFS lossage.
				 * Using uid 1 is a bit bogus but should
				 * work on all OS's.
				 */
				if (SUDOERS_UID == 0) {
				    if ((SUDOERS_MODE & 040) && seteuid(1))
					fatal("seteuid(1)");
				} else {
				    if (seteuid(SUDOERS_UID))
					fatal("seteuid(SUDOERS_UID)");
				}
			      	break;
    }
}
#endif /* _SC_SAVED_IDS && _SC_VERSION */

#ifdef HAVE_SETREUID
/*
 * Set real and effective uids and gids based on perm.
 * We always retain a real or effective uid of 0 unless
 * we are headed for an exec().
 */
void
set_perms_fallback(perm, sudo_mode)
    int perm;
    int sudo_mode;
{
    int error;

    switch (perm) {
	case PERM_FULL_ROOT:
	case PERM_ROOT:
				if (setuid(0))
				    fatal("setuid(0)");
			      	break;

	case PERM_USER:
    	    	    	        (void) setegid(user_gid);
				if (setreuid(0, user_uid))
				    fatal("setreuid(0, user_uid)");
			      	break;
				
	case PERM_RUNAS:
				/* headed for exec(), assume euid == 0 */
				runas_setup();
				if (def_flag(I_STAY_SETUID))
				    error = setreuid(user_uid, runas_pw->pw_uid);
				else
				    error = setuid(runas_pw->pw_uid);
				if (error)
				    fatal("unable to change to runas uid");
				break;

	case PERM_SUDOERS:
				/* assume euid == 0, ruid == user */
				if (setegid(SUDOERS_GID))
				    fatal("unable to change to sudoers gid");

				/*
				 * If SUDOERS_UID == 0 and SUDOERS_MODE
				 * is group readable we use a non-zero
				 * uid in order to avoid NFS lossage.
				 * Using uid 1 is a bit bogus but should
				 * work on all OS's.
				 */
				if (SUDOERS_UID == 0) {
				    if ((SUDOERS_MODE & 040) && setreuid(0, 1))
					fatal("setreuid(0, 1)");
				} else {
				    if (setreuid(0, SUDOERS_UID))
					fatal("setreuid(0, SUDOERS_UID)");
				}
			      	break;
    }
}

#else

/*
 * Set real and effective uids and gids based on perm.
 * NOTE: does not support the "stay_setuid" option.
 */
void
set_perms_fallback(perm, sudo_mode)
    int perm;
    int sudo_mode;
{

    /*
     * Since we only have setuid() and seteuid() we have to set
     * real and effective uidss to 0 initially.
     */
    if (setuid(0))
	fatal("setuid(0)");

    switch (perm) {
	case PERM_USER:
    	    	    	        (void) setegid(user_gid);
				if (seteuid(user_uid))
				    fatal("seteuid(user_uid)");
			      	break;
				
	case PERM_RUNAS:
				/* headed for exec(), assume euid == 0 */
				runas_setup();
				if (setuid(runas_pw->pw_uid))
				    fatal("unable to change to runas uid");
				break;

	case PERM_SUDOERS:
				/* assume euid == 0, ruid == user */
				if (setegid(SUDOERS_GID))
				    fatal("unable to change to sudoers gid");

				/*
				 * If SUDOERS_UID == 0 and SUDOERS_MODE
				 * is group readable we use a non-zero
				 * uid in order to avoid NFS lossage.
				 * Using uid 1 is a bit bogus but should
				 * work on all OS's.
				 */
				if (SUDOERS_UID == 0) {
				    if ((SUDOERS_MODE & 040) && seteuid(1))
					fatal("seteuid(1)");
				} else {
				    if (seteuid(SUDOERS_UID))
					fatal("seteuid(SUDOERS_UID)");
				}
			      	break;
    }
}
#endif /* HAVE_SETREUID */

static void
runas_setup()
{
#ifdef HAVE_LOGIN_CAP_H
    int error, flags;
    extern login_cap_t *lc;
#endif

    if (runas_pw->pw_name != NULL) {
#ifdef HAVE_PAM
	pam_prep_user(runas_pw);
#endif /* HAVE_PAM */

#ifdef HAVE_LOGIN_CAP_H
	if (def_flag(I_USE_LOGINCLASS)) {
	    /*
             * We don't have setusercontext() set the user since we
             * may only want to set the effective uid.  Depending on
             * sudoers and/or command line arguments we may not want
             * setusercontext() to call initgroups().
	     */
	    flags = LOGIN_SETRESOURCES|LOGIN_SETPRIORITY;
	    if (!def_flag(I_PRESERVE_GROUPS))
		flags |= LOGIN_SETGROUP;
	    else if (setgid(runas_pw->pw_gid))
		perror("cannot set gid to runas gid");
	    error = setusercontext(lc, runas_pw,
		runas_pw->pw_uid, flags);
	    if (error)
		perror("unable to set user context");
	} else
#endif /* HAVE_LOGIN_CAP_H */
	{
	    if (setgid(runas_pw->pw_gid))
		perror("cannot set gid to runas gid");
#ifdef HAVE_INITGROUPS
	    /*
	     * Initialize group vector unless asked not to.
	     */
	    if (!def_flag(I_PRESERVE_GROUPS) &&
		initgroups(*user_runas, runas_pw->pw_gid) < 0)
		perror("cannot set group vector");
#endif /* HAVE_INITGROUPS */
	}
    }
}

static void
fatal(str)
    char *str;
{

    if (str)
	perror(str);
    exit(1);
}
