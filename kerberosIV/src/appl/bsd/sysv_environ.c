/* Author: Wietse Venema <wietse@wzv.win.tue.nl> */

#include "bsd_locl.h"

RCSID("$KTH: sysv_environ.c,v 1.23 1997/12/14 23:50:44 assar Exp $");

#ifdef HAVE_ULIMIT_H
#include <ulimit.h>
#endif

#ifndef UL_SETFSIZE
#define UL_SETFSIZE 2
#endif

#include "sysv_default.h"

/*
 * Set 
 */

static void
read_etc_environment (void)
{
    FILE *f;
    char buf[BUFSIZ];

    f = fopen(_PATH_ETC_ENVIRONMENT, "r");
    if (f) {
	char *val;

	while (fgets (buf, sizeof(buf), f) != NULL) {
	    if (buf[0] == '\n' || buf[0] == '#')
		continue;
	    buf[strlen(buf) - 1] = '\0';
	    val = strchr (buf, '=');
	    if (val == NULL)
		continue;
	    *val = '\0';
	    if(setenv(buf, val + 1, 1) != 0)
		errx(1, "cannot set %s", buf);
	}
	fclose (f);
    }
}

 /*
  * Environment variables that are preserved (but may still be overruled by
  * other means). Only TERM and TZ appear to survive (SunOS 5.1). These are
  * typically inherited from the ttymon process.
  */

static struct preserved {
    char   *name;
    char   *value;
} preserved[] = {
    {"TZ", 0},
    {"TERM", 0},
    {0},
};

 /*
  * Environment variables that are not preserved and that cannot be specified
  * via commandline or stdin. Except for the LD_xxx (runtime linker) stuff,
  * the list applies to most SYSV systems. The manpage mentions only that
  * SHELL and PATH are censored. HOME, LOGNAME and MAIL are always
  * overwritten; they are in the list to make the censoring explicit.
  */

static struct censored {
    char   *prefix;
    int     length;
} censored[] = {
  {"SHELL=",	sizeof("SHELL=") - 1},
     {"HOME=",	sizeof("HOME=") - 1},
     {"LOGNAME=",	sizeof("LOGNAME=") - 1},
     {"MAIL=",	sizeof("MAIL=") - 1},
     {"CDPATH=",	sizeof("CDPATH=") - 1},
     {"IFS=",	sizeof("IFS=") - 1},
     {"PATH=",	sizeof("PATH=") - 1},
    {"LD_",	sizeof("LD_") - 1},
    {0},
};

/* sysv_newenv - set up final environment after logging in */

void sysv_newenv(int argc, char **argv, struct passwd *pwd,
		 char *term, int pflag)
{
    unsigned umask_val;
    char    buf[BUFSIZ];
    int     count = 0;
    struct censored *cp;
    struct preserved *pp;

    /* Preserve a selection of the environment. */

    for (pp = preserved; pp->name; pp++)
	pp->value = getenv(pp->name);

    /*
     * Note: it is a bad idea to assign a static array to the global environ
     * variable. Reason is that putenv() can run into problems when it tries
     * to realloc() the environment table. Instead, we just clear environ[0]
     * and let putenv() work things out.
     */

    if (!pflag && environ)
	environ[0] = 0;

    /* Restore preserved environment variables. */

    for (pp = preserved; pp->name; pp++)
	if (pp->value)
	    if(setenv(pp->name, pp->value, 1) != 0)
		errx(1, "cannot set %s", pp->name);

    /* The TERM definition from e.g. rlogind can override an existing one. */

    if (term[0])
	if(setenv("TERM", term, 1) != 0)
	    errx(1, "cannot set TERM");

    /*
     * Environment definitions from the command line overrule existing ones,
     * but can be overruled by definitions from stdin. Some variables are
     * censored.
     * 
     * Omission: we do not support environment definitions from stdin.
     */

#define STREQN(x,y,l) (x[0] == y[0] && strncmp(x,y,l) == 0)

    while (argc && *argv) {
	if (strchr(*argv, '=') == 0) {
	    snprintf(buf, sizeof(buf), "L%d", count++);
	    if(setenv(buf, *argv, 1) != 0)
		errx(1, "cannot set %s", buf);
	} else {
	    for (cp = censored; cp->prefix; cp++)
		if (STREQN(*argv, cp->prefix, cp->length))
		    break;
	    if (cp->prefix == 0)
		putenv(*argv);
	}
	argc--, argv++;
    }

    /* PATH is always reset. */

    if(setenv("PATH", pwd->pw_uid ? default_path : default_supath, 1) != 0)
	errx(1, "cannot set PATH");

    /* Undocumented: HOME, MAIL and LOGNAME are always reset (SunOS 5.1). */

    if(setenv("HOME", pwd->pw_dir, 1) != 0)
	errx(1, "cannot set HOME");
    {
	char *sep = "/";
	if(KRB4_MAILDIR[strlen(KRB4_MAILDIR) - 1] == '/')
	    sep = "";
	roken_concat(buf, sizeof(buf), KRB4_MAILDIR, sep, pwd->pw_name, NULL);
    }
    if(setenv("MAIL", buf, 1) != 0)
	errx(1, "cannot set MAIL");
    if(setenv("LOGNAME", pwd->pw_name, 1) != 0)
	errx(1, "cannot set LOGNAME");
    if(setenv("USER", pwd->pw_name, 1) != 0)
	errx(1, "cannot set USER");

    /*
     * Variables that may be set according to specifications in the defaults
     * file. HZ and TZ are set only if they are still uninitialized.
     * 
     * Extension: when ALTSHELL=YES, we set the SHELL variable even if it is
     * /bin/sh.
     */

    if (strcasecmp(default_altsh, "YES") == 0)
	if(setenv("SHELL", pwd->pw_shell, 1) != 0)
	    errx(1, "cannot set SHELL");
    if (default_hz)
	if(setenv("HZ", default_hz, 0) != 0)
	    errx(1, "cannot set HZ");
    if (default_timezone)
	if(setenv("TZ", default_timezone, 0) != 0)
	    errx(1, "cannot set TZ");

    /* Non-environment stuff. */

    if (default_umask) {
	if (sscanf(default_umask, "%o", &umask_val) == 1 && umask_val)
	    umask(umask_val);
    }
#ifdef HAVE_ULIMIT
    if (default_ulimit) {
	long    limit_val;

	if (sscanf(default_ulimit, "%ld", &limit_val) == 1 && limit_val)
	    if (ulimit(UL_SETFSIZE, limit_val) < 0)
	        warn ("ulimit(UL_SETFSIZE, %ld)", limit_val);
    }
#endif
    read_etc_environment();
}

