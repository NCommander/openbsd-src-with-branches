/*
 * Copyright (c) 2000 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#include <ctype.h>
#include <sys/param.h>
#include <sys/types.h>
#include <pwd.h>

#include <login_cap.h>
#include <bsd_auth.h>

#include "sudo.h"
#include "sudo_auth.h"

#ifndef lint
static const char rcsid[] = "$Sudo: bsdauth.c,v 1.3 2000/10/30 03:45:11 millert Exp $";
#endif /* lint */

extern char *login_style;		/* from sudo.c */

int
bsdauth_init(pw, promptp, auth)
    struct passwd *pw;
    char **promptp;
    sudo_auth *auth;
{
    static auth_session_t *as;
    extern login_cap_t *lc;			/* from sudo.c */

    if ((as = auth_open()) == NULL) {
	log_error(USE_ERRNO|NO_EXIT|NO_MAIL, 
	    "unable to begin bsd authentication");
	return(AUTH_FATAL);
    }

    /* XXX - maybe sanity check the auth style earlier? */
    login_style = login_getstyle(lc, login_style, "auth-sudo");
    if (login_style == NULL) {
	log_error(NO_EXIT|NO_MAIL, "invalid authentication type");
	auth_close(as);
	return(AUTH_FATAL);
    }

     if (auth_setitem(as, AUTHV_STYLE, login_style) < 0 ||
	auth_setitem(as, AUTHV_NAME, pw->pw_name) < 0 ||
	auth_setitem(as, AUTHV_CLASS, login_class) < 0) {
	log_error(NO_EXIT|NO_MAIL, "unable to setup authentication");
	auth_close(as);
	return(AUTH_FATAL);
    }

    auth->data = (VOID *) as;
    return(AUTH_SUCCESS);
}

int
bsdauth_verify(pw, prompt, auth)
    struct passwd *pw;
    char *prompt;
    sudo_auth *auth;
{
    char *s, *pass;
    size_t len;
    int authok;
    sig_t childkiller;
    auth_session_t *as = (auth_session_t *) auth->data;
    extern int nil_pw;

    /* save old signal handler */
    childkiller = signal(SIGCHLD, SIG_DFL);

    /*
     * If there is a challenge then print that instead of the normal
     * prompt.  If the user just hits return we prompt again with echo
     * turned on, which is useful for challenge/response things like
     * S/Key.
     */
    if ((s = auth_challenge(as)) == NULL) {
	pass = tgetpass(prompt, def_ival(I_PW_TIMEOUT) * 60, tgetpass_flags);
    } else {
	pass = tgetpass(s, def_ival(I_PW_TIMEOUT) * 60, tgetpass_flags);
	if (!pass || *pass == '\0') {
	    if ((prompt = strrchr(s, '\n')))
		prompt++;
	    else
		prompt = s;

	    /*
	     * Append '[echo on]' to the last line of the challenge and
	     * reprompt with echo turned on.
	     */
	    len = strlen(prompt) - 1;
	    while (isspace(prompt[len]) || prompt[len] == ':')
		prompt[len--] = '\0';
	    easprintf(&s, "%s [echo on]: ", prompt);
	    pass = tgetpass(s, def_ival(I_PW_TIMEOUT) * 60,
		tgetpass_flags | TGP_ECHO);
	    free(s);
	}
    }

    if (!pass || *pass == '\0')
	nil_pw = 1;			/* empty password */

    authok = auth_userresponse(as, pass, 1);

    /* restore old signal handler */
    (void)signal(SIGCHLD, childkiller);

    if (authok)
	return(AUTH_SUCCESS);

    if ((s = auth_getvalue(as, "errormsg")) != NULL)
	log_error(NO_EXIT|NO_MAIL, "%s", s);
    return(AUTH_FAILURE);
}

int
bsdauth_cleanup(pw, auth)
    struct passwd *pw;
    sudo_auth *auth;
{
    auth_session_t *as = (auth_session_t *) auth->data;

    auth_close(as);

    return(AUTH_SUCCESS);
}
