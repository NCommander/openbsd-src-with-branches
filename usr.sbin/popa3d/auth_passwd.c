/* $OpenBSD: auth_passwd.c,v 1.2 2001/08/13 20:19:33 camield Exp $ */

/*
 * The /etc/passwd authentication routine.
 */

#include "params.h"

#if AUTH_PASSWD && !VIRTUAL_ONLY

#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED
#define _XOPEN_VERSION 4
#define _XPG4_2
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>

struct passwd *auth_userpass(char *user, char *pass, char **mailbox)
{
	struct passwd *pw, *result;

	if ((pw = getpwnam(user))) *mailbox = user;
	endpwent();
	result = NULL;

	if (!pw || !*pw->pw_passwd ||
	    *pw->pw_passwd == '*' || *pw->pw_passwd == '!')
		crypt(pass, AUTH_DUMMY_SALT);
	else
	if (!strcmp(crypt(pass, pw->pw_passwd), pw->pw_passwd))
		result = pw;

	if (pw)
		memset(pw->pw_passwd, 0, strlen(pw->pw_passwd));

	return result;
}

#endif
