/* $OpenBSD: user.c,v 1.19 2001/02/13 09:18:33 deraadt Exp $ */
/* $NetBSD: user.c,v 1.17 2000/04/14 06:26:55 simonb Exp $ */

/*
 * Copyright (c) 1999 Alistair G. Crooks.  All rights reserved.
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
 *	This product includes software developed by Alistair G. Crooks.
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <util.h>

#include "defs.h"
#include "usermgmt.h"

/* this struct describes a uid range */
typedef struct range_t {
	int	r_from;		/* low uid */
	int	r_to;		/* high uid */
} range_t;

/* this struct encapsulates the user information */
typedef struct user_t {
	int		u_uid;			/* uid of user */
	char		*u_password;		/* encrypted password */
	char		*u_comment;		/* comment field */
	int		u_homeset;		/* home dir has been set */
	char		*u_home;		/* home directory */
	char		*u_primgrp;		/* primary group */
	int		u_groupc;		/* # of secondary groups */
	char		*u_groupv[NGROUPS_MAX];	/* secondary groups */
	char		*u_shell;		/* user's shell */
	char		*u_basedir;		/* base directory for home */
	char		*u_expire;		/* when password will expire */
	int		u_inactive;		/* inactive */
	int		u_mkdir;		/* make the home directory */
	int		u_dupuid;		/* duplicate uids are allowed */
	char		*u_skeldir;		/* directory for startup files */
	unsigned	u_rsize;		/* size of range array */
	unsigned	u_rc;			/* # of ranges */
	range_t		*u_rv;			/* the ranges */
	unsigned	u_defrc;		/* # of ranges in defaults */
	int		u_preserve;		/* preserve uids on deletion */
} user_t;

#define CONFFILE	"/etc/usermgmt.conf"

#ifndef DEF_GROUP
#define DEF_GROUP	"users"
#endif

#ifndef DEF_BASEDIR
#define DEF_BASEDIR	"/home"
#endif

#ifndef DEF_SKELDIR
#define DEF_SKELDIR	"/etc/skel"
#endif

#ifndef DEF_SHELL
#define DEF_SHELL	"/bin/csh"
#endif

#ifndef DEF_COMMENT
#define DEF_COMMENT	""
#endif

#ifndef DEF_LOWUID
#define DEF_LOWUID	1000
#endif

#ifndef DEF_HIGHUID
#define DEF_HIGHUID	60000
#endif

#ifndef DEF_INACTIVE
#define DEF_INACTIVE	0
#endif

#ifndef DEF_EXPIRE
#define DEF_EXPIRE	(char *) NULL
#endif

#ifndef WAITSECS
#define WAITSECS	10
#endif

#ifndef NOBODY_UID
#define NOBODY_UID	32767
#endif

/* some useful constants */
enum {
	MaxShellNameLen = 256,
	MaxFileNameLen = MAXPATHLEN,
	MaxUserNameLen = MAXLOGNAME,
	MaxCommandLen = 2048,
	MaxEntryLen = 2048,
	MaxPasswordEntryLen = 1024,
	PasswordLength = _PASSWORD_LEN,

	LowGid = DEF_LOWUID,
	HighGid = DEF_HIGHUID
};

/* Full paths of programs used here */
#define CHOWN		"/sbin/chown"
#define MKDIR		"/bin/mkdir"
#define MV		"/bin/mv"
#define NOLOGIN		"/sbin/nologin"
#define PAX		"/bin/pax"
#define RM		"/bin/rm"

#define UNSET_EXPIRY	"Null (unset)"

static int	verbose = 0;

/* if *cpp is non-null, free it, then assign `n' chars of `s' to it */
static void
memsave(char **cpp, char *s, size_t n)
{
	if (*cpp != (char *) NULL) {
		FREE(*cpp);
	}
	NEWARRAY(char, *cpp, n + 1, exit(1));
	(void) memcpy(*cpp, s, n);
	(*cpp)[n] = '\0';
}

/* a replacement for system(3) */
static int
asystem(char *fmt, ...)
{
	va_list	vp;
	char	buf[MaxCommandLen];
	int	ret;

	va_start(vp, fmt);
	(void) vsnprintf(buf, sizeof(buf), fmt, vp);
	va_end(vp);
	if (verbose) {
		(void) printf("Command: %s\n", buf);
	}
	if ((ret = system(buf)) != 0) {
		warnx("[Warning] can't system `%s'", buf);
	}
	return ret;
}


/* return 1 if all of `s' is numeric */
static int
is_number(char *s)
{
	for ( ; *s ; s++) {
		if (!isdigit(*s)) {
			return 0;
		}
	}
	return 1;
}

/*
 * check that the effective uid is 0 - called from funcs which will
 * modify data and config files.
 */
static void
checkeuid(void)
{
	if (geteuid() != 0) {
		errx(EXIT_FAILURE, "Program must be run as root");
	}
}

/* copy any dot files into the user's home directory */
static int
copydotfiles(char *skeldir, int uid, int gid, char *dir)
{
	struct dirent	*dp;
	DIR		*dirp;
	int		n;

	if ((dirp = opendir(skeldir)) == (DIR *) NULL) {
		warn("can't open source . files dir `%s'", skeldir);
		return 0;
	}
	for (n = 0; (dp = readdir(dirp)) != (struct dirent *) NULL && n == 0 ; ) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0) {
			continue;
		}
		n = 1;
	}
	(void) closedir(dirp);
	if (n == 0) {
		warnx("No \"dot\" initialisation files found");
	} else {
		(void) asystem("cd %s; %s -rw -pe %s . %s", 
				skeldir, PAX, (verbose) ? "-v" : "", dir);
	}
	(void) asystem("%s -R -P %d:%d %s", CHOWN, uid, gid, dir);
	return n;
}

/* create a group entry with gid `gid' */
static int
creategid(char *group, int gid)
{
	struct stat	st;
	FILE		*from;
	FILE		*to;
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	int		fd;
	int		cc;

	if (getgrnam(group) != (struct group *) NULL) {
		warnx("group `%s' already exists", group);
		return 0;
	}
	if ((from = fopen(_PATH_GROUP, "r")) == (FILE *) NULL) {
		warn("can't create gid for %s: can't open %s", group, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("can't lock `%s'", _PATH_GROUP);
	}
	(void) fstat(fileno(from), &st);
	(void) snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		(void) fclose(from);
		warn("can't create gid: mkstemp failed");
		return 0;
	}
	if ((to = fdopen(fd, "w")) == (FILE *) NULL) {
		(void) fclose(from);
		(void) close(fd);
		(void) unlink(f);
		warn("can't create gid: fdopen `%s' failed", f);
		return 0;
	}
	while ((cc = fread(buf, sizeof(char), sizeof(buf), from)) > 0) {
		if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			(void) fclose(from);
			(void) close(fd);
			(void) unlink(f);
			warn("can't create gid: short write to `%s'", f);
			return 0;
		}
	}
	(void) fprintf(to, "%s:*:%d:\n", group, gid);
	(void) fclose(from);
	(void) fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		warn("can't create gid: can't rename `%s' to `%s'", f, _PATH_GROUP);
		return 0;
	}
	(void) chmod(_PATH_GROUP, st.st_mode & 07777);
	return 1;
}

/* modify the group entry with name `group' to be newent */
static int
modify_gid(char *group, char *newent)
{
	struct stat	st;
	FILE		*from;
	FILE		*to;
	char		buf[MaxEntryLen];
	char		f[MaxFileNameLen];
	char		*colon;
	int		groupc;
	int		entc;
	int		fd;
	int		cc;

	if ((from = fopen(_PATH_GROUP, "r")) == (FILE *) NULL) {
		warn("can't create gid for %s: can't open %s", group, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("can't lock `%s'", _PATH_GROUP);
	}
	(void) fstat(fileno(from), &st);
	(void) snprintf(f, sizeof(f), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp(f)) < 0) {
		(void) fclose(from);
		warn("can't create gid: mkstemp failed");
		return 0;
	}
	if ((to = fdopen(fd, "w")) == (FILE *) NULL) {
		(void) fclose(from);
		(void) close(fd);
		(void) unlink(f);
		warn("can't create gid: fdopen `%s' failed", f);
		return 0;
	}
	groupc = strlen(group);
	while (fgets(buf, sizeof(buf), from) != NULL) {
		cc = strlen(buf);
		if ((colon = strchr(buf, ':')) == NULL) {
			warn("badly formed entry `%s'", buf);
			continue;
		}
		entc = (int)(colon - buf);
		if (entc == groupc && strncmp(group, buf, (unsigned) entc) == 0) {
			if (newent == NULL) {
				continue;
			} else {
				cc = strlen(newent);
				(void) strlcpy(buf, newent, sizeof(buf));
			}
		}
		if (fwrite(buf, sizeof(char), (unsigned) cc, to) != cc) {
			(void) fclose(from);
			(void) close(fd);
			(void) unlink(f);
			warn("can't create gid: short write to `%s'", f);
			return 0;
		}
	}
	(void) fclose(from);
	(void) fclose(to);
	if (rename(f, _PATH_GROUP) < 0) {
		warn("can't create gid: can't rename `%s' to `%s'", f, _PATH_GROUP);
		return 0;
	}
	(void) chmod(_PATH_GROUP, st.st_mode & 07777);
	return 1;
}

/* (Re)write secondary groups */
static int
modify_groups (char *login, char *newlogin, user_t *up, char *primgrp)
{
	struct stat    st;
	FILE          *from, *to;
	char          *colon, *p;
	char           buf[MaxEntryLen], tmpf[MaxFileNameLen];
	int            fd, cc, g;

	/* XXX This function, and modify_gid() above could possibly be */
	/* combined into one function. */

	if ((from = fopen(_PATH_GROUP, "r")) == (FILE *) NULL) {
		warn("can't create/modify groups for login %s: can't open %s",
		     login, _PATH_GROUP);
		return 0;
	}
	if (flock(fileno(from), LOCK_EX | LOCK_NB) < 0) {
		warn("can't lock `%s'", _PATH_GROUP);
	}
	(void) fstat(fileno(from), &st);
	(void) snprintf(tmpf, sizeof (tmpf), "%s.XXXXXX", _PATH_GROUP);
	if ((fd = mkstemp (tmpf)) < 0) {
		(void) fclose(from);
		warn("can't modify secondary groups: mkstemp failed");
		return 0;
	}
	if ((to = fdopen(fd, "w")) == (FILE *) NULL) {
		(void) fclose(from);
		(void) close(fd);
		(void) unlink(tmpf);
		warn("can't create gid: fdopen `%s' failed", tmpf);
		return 0;
	}

	while (fgets (buf, sizeof(buf), from) != NULL) {
	  	if ((colon = strchr (buf, ':')) == NULL) {
		  	warn ("badly formed entry `%s'", buf);
			continue;
		}

		/* Does the user exist in this group? */
		cc = strlen (buf);
		g  = strlen (login);
		p  = colon; 
		while (p < (buf + cc) && (p = strstr (p, login)) != NULL &&
		       *(p + g) != ',' && *(p + g) != '\n')
		  	p += g;
		if (p && p < (buf + cc)) {
		  	/* Remove. XXX Un-obfuscate. */
		  	bcopy (*(p + g) == ',' ? (p + g + 1) : (p + g), 
			       *(p - 1) == ',' && *(p + g) != ',' ? p - 1 : p, 
			       strlen (p + g) + 1);
		}
		
		/* Should the user exist in this group? */
		cc = strlen (primgrp);
		for (g = 0; g < up->u_groupc; g++) {
		        if (!up->u_groupv[g] ||
			    !strncmp (up->u_groupv[g], primgrp,
				      MAX (strlen (up->u_groupv[g]), cc)))
			  	continue;
		        if ((int)(colon - buf) == strlen (up->u_groupv[g]) &&
			    !strncmp (up->u_groupv[g], buf, 
				      (int)(colon - buf))) {
			  	/* Add user. */
				p = buf + strlen(buf) - 1; /* No '\n' */
				snprintf (p, sizeof (buf), "%s%s\n", 
					  (*(p-1) == ':' ? "" : ",") ,
					  (newlogin ? newlogin : login));
				up->u_groupv[g] = '\0'; /* Mark used. */
				g = up->u_groupc;       /* Break loop. */
			}
		}

		cc = strlen (buf);
		if (fwrite (buf, sizeof (char), cc, to) != cc) {
		  	(void) fclose (from);
			(void) close (fd);
			(void) unlink (tmpf);
			warn ("can't update secondary groups: "
			      "short write to `%s'", tmpf);
			return 0;
		}
	}
	(void) fclose (from);
	(void) fclose (to);
	if (rename (tmpf, _PATH_GROUP) < 0) {
	  	warn("can't create gid: can't rename `%s' to `%s'", tmpf, 
		     _PATH_GROUP);
		return 0;
	}
	(void) chmod(_PATH_GROUP, st.st_mode & 07777);
	/* Warn about unselected groups. */
	for (g = 0; g < up->u_groupc; g++)
	  	if (up->u_groupv[g])
		  	warnx("extraneous group `%s' not added",
			      up->u_groupv[g]);
	return 1;
}

/* return 1 if `login' is a valid login name */
static int
valid_login(char *login)
{
	char	*cp;

	for (cp = login ; *cp ; cp++) {
		if (!isalnum(*cp) && *cp != '.' && *cp != '_' && *cp != '-') {
			return 0;
		}
	}
	return 1;
}

/* return 1 if `group' is a valid group name */
static int
valid_group(char *group)
{
	char	*cp;

	for (cp = group ; *cp ; cp++) {
		if (!isalnum(*cp)) {
			return 0;
		}
	}
	return 1;
}

/* find the next gid in the range lo .. hi */
static int
getnextgid(int *gidp, int lo, int hi)
{
	for (*gidp = lo ; *gidp < hi ; *gidp += 1) {
		if (getgrgid((gid_t)*gidp) == (struct group *) NULL) {
			return 1;
		}
	}
	return 0;
}

#ifdef EXTENSIONS
/* save a range of uids */
static int
save_range(user_t *up, char *cp)
{
	int	from;
	int	to;
	int	i;

	if (up->u_rsize == 0) {
		up->u_rsize = 32;
		NEWARRAY(range_t, up->u_rv, up->u_rsize, return(0));
	} else if (up->u_rc == up->u_rsize) {
		up->u_rsize *= 2;
		RENEW(range_t, up->u_rv, up->u_rsize, return(0));
	}
	if (up->u_rv && sscanf(cp, "%d..%d", &from, &to) == 2) {
		for (i = 0 ; i < up->u_rc ; i++) {
			if (up->u_rv[i].r_from == from && up->u_rv[i].r_to == to) {
				break;
			}
		}
		if (i == up->u_rc) {
			up->u_rv[up->u_rc].r_from = from;
			up->u_rv[up->u_rc].r_to = to;
			up->u_rc += 1;
		}
	} else {
		warnx("Bad range `%s'", cp);
		return 0;
	}
	return 1;
}
#endif

/* set the defaults in the defaults file */
static int
setdefaults(user_t *up)
{
	char	template[MaxFileNameLen];
	FILE	*fp;
	int	ret;
	int	fd;
	int	i;

	(void) snprintf(template, sizeof(template), "%s.XXXXXX", CONFFILE);
	if ((fd = mkstemp(template)) < 0) {
		warnx("can't mkstemp `%s' for writing", CONFFILE);
		return 0;
	}
	if ((fp = fdopen(fd, "w")) == (FILE *) NULL) {
		warn("can't fdopen `%s' for writing", CONFFILE);
		return 0;
	}
	ret = 1;
	if (fprintf(fp, "group\t\t%s\n", up->u_primgrp) <= 0 ||
	    fprintf(fp, "base_dir\t%s\n", up->u_basedir) <= 0 ||
	    fprintf(fp, "skel_dir\t%s\n", up->u_skeldir) <= 0 ||
	    fprintf(fp, "shell\t\t%s\n", up->u_shell) <= 0 ||
	    fprintf(fp, "inactive\t%d\n", up->u_inactive) <= 0 ||
	    fprintf(fp, "expire\t\t%s\n", (up->u_expire == NULL) ? UNSET_EXPIRY : up->u_expire) <= 0) {
		warn("can't write to `%s'", CONFFILE);
		ret = 0;
	}
#ifdef EXTENSIONS
	for (i = (up->u_defrc != up->u_rc) ? up->u_defrc : 0 ; i < up->u_rc ; i++) {
		if (fprintf(fp, "range\t\t%d..%d\n", up->u_rv[i].r_from, up->u_rv[i].r_to) <= 0) {
			warn("can't write to `%s'", CONFFILE);
			ret = 0;
		}
	}
#endif
	(void) fclose(fp);
	if (ret) {
		ret = ((rename(template, CONFFILE) == 0) && (chmod(CONFFILE, 0644) == 0));
	}
	return ret;
}

/* read the defaults file */
static void
read_defaults(user_t *up)
{
	struct stat	st;
	size_t		lineno;
	size_t		len;
	FILE		*fp;
	char		*cp;
	char		*s;

	memsave(&up->u_primgrp, DEF_GROUP, strlen(DEF_GROUP));
	memsave(&up->u_basedir, DEF_BASEDIR, strlen(DEF_BASEDIR));
	memsave(&up->u_skeldir, DEF_SKELDIR, strlen(DEF_SKELDIR));
	memsave(&up->u_shell, DEF_SHELL, strlen(DEF_SHELL));
	memsave(&up->u_comment, DEF_COMMENT, strlen(DEF_COMMENT));
	up->u_rsize = 16;
	NEWARRAY(range_t, up->u_rv, up->u_rsize, exit(1));
	up->u_inactive = DEF_INACTIVE;
	up->u_expire = DEF_EXPIRE;
	if ((fp = fopen(CONFFILE, "r")) == (FILE *) NULL) {
		if (stat(CONFFILE, &st) < 0 && !setdefaults(up)) {
			warn("can't create `%s' defaults file", CONFFILE);
		}
		fp = fopen(CONFFILE, "r");
	}
	if (fp != (FILE *) NULL) {
		while ((s = fparseln(fp, &len, &lineno, NULL, 0)) != NULL) {
			if (strncmp(s, "group", 5) == 0) {
				for (cp = s + 5 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_primgrp, cp, strlen(cp));
			} else if (strncmp(s, "base_dir", 8) == 0) {
				for (cp = s + 8 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_basedir, cp, strlen(cp));
			} else if (strncmp(s, "skel_dir", 8) == 0) {
				for (cp = s + 8 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_skeldir, cp, strlen(cp));
			} else if (strncmp(s, "shell", 5) == 0) {
				for (cp = s + 5 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_shell, cp, strlen(cp));
			} else if (strncmp(s, "password", 8) == 0) {
				for (cp = s + 8 ; *cp && isspace(*cp) ; cp++) {
				}
				memsave(&up->u_password, cp, strlen(cp));
			} else if (strncmp(s, "inactive", 8) == 0) {
				for (cp = s + 8 ; *cp && isspace(*cp) ; cp++) {
				}
				up->u_inactive = atoi(cp);
#ifdef EXTENSIONS
			} else if (strncmp(s, "range", 5) == 0) {
				for (cp = s + 5 ; *cp && isspace(*cp) ; cp++) {
				}
				(void) save_range(up, cp);
#endif
#ifdef EXTENSIONS
			} else if (strncmp(s, "preserve", 8) == 0) {
				for (cp = s + 8 ; *cp && isspace(*cp) ; cp++) {
				}
				up->u_preserve = (strncmp(cp, "true", 4) == 0) ? 1 :
						  (strncmp(cp, "yes", 3) == 0) ? 1 :
						   atoi(cp);
#endif
			} else if (strncmp(s, "expire", 6) == 0) {
				for (cp = s + 6 ; *cp && isspace(*cp) ; cp++) {
				}
				if (strcmp(cp, UNSET_EXPIRY) == 0) {
					if (up->u_expire) {
						FREE(up->u_expire);
					}
					up->u_expire = NULL;
				} else {
					memsave(&up->u_expire, cp, strlen(cp));
				}
			}
			(void) free(s);
		}
		(void) fclose(fp);
	}
	if (up->u_rc == 0) {
		up->u_rv[up->u_rc].r_from = DEF_LOWUID;
		up->u_rv[up->u_rc].r_to = DEF_HIGHUID;
		up->u_rc += 1;
	}
	up->u_defrc = up->u_rc;
}

/* return the next valid unused uid */
static int
getnextuid(int sync_uid_gid, int *uid, int low_uid, int high_uid)
{
	for (*uid = low_uid ; *uid <= high_uid ; (*uid)++) {
		if (getpwuid((uid_t)(*uid)) == (struct passwd *) NULL && *uid != NOBODY_UID) {
			if (sync_uid_gid) {
				if (getgrgid((gid_t)(*uid)) == (struct group *) NULL) {
					return 1;
				}
			} else {
				return 1;
			}
		}
	}
	return 0;
}

/* add a user */
static int
adduser(char *login, user_t *up)
{
	struct group	*grp;
	struct stat	st;
	struct tm	tm;
	time_t		expire;
	char		password[PasswordLength + 1];
	char		home[MaxFileNameLen];
	char		buf[MaxFileNameLen];
	int		sync_uid_gid;
	int		masterfd;
	int		ptmpfd;
	int		gid;
	int		cc;
	int		i;

	if (!valid_login(login)) {
		errx(EXIT_FAILURE, "`%s' is not a valid login name", login);
	}
	if ((masterfd = open(_PATH_MASTERPASSWD, O_RDONLY)) < 0) {
		err(EXIT_FAILURE, "can't open `%s'", _PATH_MASTERPASSWD);
	}
	if (flock(masterfd, LOCK_EX | LOCK_NB) < 0) {
		err(EXIT_FAILURE, "can't lock `%s'", _PATH_MASTERPASSWD);
	}
	pw_init();
	if ((ptmpfd = pw_lock(WAITSECS)) < 0) {
		(void) close(masterfd);
		err(EXIT_FAILURE, "can't obtain pw_lock");
	}
	while ((cc = read(masterfd, buf, sizeof(buf))) > 0) {
		if (write(ptmpfd, buf, (size_t)(cc)) != cc) {
			(void) close(masterfd);
			(void) close(ptmpfd);
			(void) pw_abort();
			err(EXIT_FAILURE, "short write to /etc/ptmp (not %d chars)", cc);
		}
	}
	/* if no uid was specified, get next one in [low_uid..high_uid] range */
	sync_uid_gid = (strcmp(up->u_primgrp, "=uid") == 0);
	if (up->u_uid == -1) {
		for (i = 0 ; i < up->u_rc ; i++) {
			if (getnextuid(sync_uid_gid, &up->u_uid, up->u_rv[i].r_from, up->u_rv[i].r_to)) {
				break;
			}
		}
		if (i == up->u_rc) {
			(void) close(ptmpfd);
			(void) pw_abort();
			errx(EXIT_FAILURE, "can't get next uid for %d", up->u_uid);
		}
	}
	/* check uid isn't already allocated */
	if (!up->u_dupuid && getpwuid((uid_t)(up->u_uid)) != (struct passwd *) NULL) {
		(void) close(ptmpfd);
		(void) pw_abort();
		errx(EXIT_FAILURE, "uid %d is already in use", up->u_uid);
	}
	/* if -g=uid was specified, check gid is unused */
	if (sync_uid_gid) {
		if (getgrgid((gid_t)(up->u_uid)) != (struct group *) NULL) {
			(void) close(ptmpfd);
			(void) pw_abort();
			errx(EXIT_FAILURE, "gid %d is already in use", up->u_uid);
		}
		gid = up->u_uid;
	} else if ((grp = getgrnam(up->u_primgrp)) != (struct group *) NULL) {
		gid = grp->gr_gid;
	} else if (is_number(up->u_primgrp) &&
		   (grp = getgrgid((gid_t)atoi(up->u_primgrp))) != (struct group *) NULL) {
		gid = grp->gr_gid;
	} else {
		(void) close(ptmpfd);
		(void) pw_abort();
		errx(EXIT_FAILURE, "group %s not found", up->u_primgrp);
	}
	/* check name isn't already in use */
	if (!up->u_dupuid && getpwnam(login) != (struct passwd *) NULL) {
		(void) close(ptmpfd);
		(void) pw_abort();
		errx(EXIT_FAILURE, "already a `%s' user", login);
	}
	if (up->u_homeset) {
		(void) strlcpy(home, up->u_home, sizeof(home));
	} else {
		/* if home directory hasn't been given, make it up */
		(void) snprintf(home, sizeof(home), "%s/%s", up->u_basedir, login);
	}
	expire = 0;
	if (up->u_expire != NULL) {
		(void) memset(&tm, 0, sizeof(tm));
		if (strptime(up->u_expire, "%c", &tm) == NULL) {
			warnx("invalid time format `%s'", optarg);
		} else {
			expire = mktime(&tm);
		}
	}
	if (up->u_password != NULL &&
	    strlen(up->u_password) <= PasswordLength) {
		(void) strlcpy(password, up->u_password, sizeof(password));
	} else {
		(void) strlcpy(password, "*", sizeof(password));
		if (up->u_password != NULL) {
			warnx("Password `%s' is invalid: setting it to `%s'",
				up->u_password, password);
		}
	}
	cc = snprintf(buf, sizeof(buf), "%s:%s:%d:%d::%d:%ld:%s:%s:%s\n",
			login,
			password,
			up->u_uid,
			gid,
			up->u_inactive,
			(long) expire,
			up->u_comment,
			home,
			up->u_shell);
	if (cc >= MaxPasswordEntryLen ||
	    (strchr(up->u_comment, '&') != NULL &&
	     cc + strlen(login) >= MaxPasswordEntryLen)) {
		(void) close(ptmpfd);
		(void) pw_abort();
		errx(EXIT_FAILURE, "can't add `%s', line too long", buf);
	}
	if (write(ptmpfd, buf, (size_t) cc) != cc) {
		(void) close(ptmpfd);
		(void) pw_abort();
		err(EXIT_FAILURE, "can't add `%s'", buf);
	}
	if (up->u_mkdir) {
		if (lstat(home, &st) < 0 && asystem("%s -p %s", MKDIR, home) != 0) {
			(void) close(ptmpfd);
			(void) pw_abort();
			err(EXIT_FAILURE, "can't mkdir `%s'", home);
		}
		(void) copydotfiles(up->u_skeldir, up->u_uid, gid, home);
	}
	if (sync_uid_gid &&
	    getgrnam(login) == (struct group *) NULL &&
	    !creategid(login, gid)) {
		(void) close(ptmpfd);
		(void) pw_abort();
		err(EXIT_FAILURE, "can't create gid %d for login name %s", gid, login);
	}
	/* Modify secondary groups */
	if (up->u_groupc != 0 && 
	    !modify_groups (login, NULL, up,
			    (sync_uid_gid ? login : up->u_primgrp))) {
	  	(void) close(ptmpfd);
	  	(void) pw_abort();
		err(EXIT_FAILURE, 
		    "failed to modify secondary groups for login %s", login);
	}
	(void) close(ptmpfd);
	if (pw_mkdb(login) < 0) {
		err(EXIT_FAILURE, "pw_mkdb failed");
	}
	return 1;
}

/* modify a user */
static int
moduser(char *login, char *newlogin, user_t *up)
{
	struct passwd	*pwp;
	struct group	*grp;
	struct tm	tm;
	time_t		expire;
	size_t		loginc;
	size_t		colonc;
	FILE		*master;
	char		password[PasswordLength + 1];
	char		oldhome[MaxFileNameLen];
	char		home[MaxFileNameLen];
	char		buf[MaxFileNameLen];
	char		*colon;
	int		masterfd;
	int		ptmpfd;
	int		gid;
	int		cc;

	if (!valid_login(newlogin)) {
		errx(EXIT_FAILURE, "`%s' is not a valid login name", login);
	}
	if ((pwp = getpwnam(login)) == (struct passwd *) NULL) {
		errx(EXIT_FAILURE, "No such user `%s'", login);
	}
	if ((masterfd = open(_PATH_MASTERPASSWD, O_RDONLY)) < 0) {
		err(EXIT_FAILURE, "can't open `%s'", _PATH_MASTERPASSWD);
	}
	if (flock(masterfd, LOCK_EX | LOCK_NB) < 0) {
		err(EXIT_FAILURE, "can't lock `%s'", _PATH_MASTERPASSWD);
	}
	pw_init();
	if ((ptmpfd = pw_lock(WAITSECS)) < 0) {
		(void) close(masterfd);
		err(EXIT_FAILURE, "can't obtain pw_lock");
	}
	if ((master = fdopen(masterfd, "r")) == (FILE *) NULL) {
		(void) close(masterfd);
		(void) close(ptmpfd);
		(void) pw_abort();
		err(EXIT_FAILURE, "can't fdopen fd for %s", _PATH_MASTERPASSWD);
	}
	if (up != (user_t *) NULL) {
		if (up->u_mkdir) {
			(void) strlcpy(oldhome, pwp->pw_dir, sizeof(oldhome));
		}
		if (up->u_uid == -1) {
			up->u_uid = pwp->pw_uid;
		}
		if (up->u_primgrp == NULL) {
			gid = pwp->pw_gid;
		} else if (strcmp(up->u_primgrp, "=uid") == 0) {
		  	/* if -g=uid was specified, check gid is unused */
			if (getgrgid((gid_t)(up->u_uid)) != (struct group *) NULL) {
				(void) close(ptmpfd);
				(void) pw_abort();
				errx(EXIT_FAILURE, "gid %d is already in use", up->u_uid);
			}
			gid = up->u_uid;
		} else if ((grp = getgrnam(up->u_primgrp)) != (struct group *) NULL) {
			gid = grp->gr_gid;
		} else if (is_number(up->u_primgrp) &&
			   (grp = getgrgid((gid_t)atoi(up->u_primgrp))) != (struct group *) NULL) {
			gid = grp->gr_gid;
		} else {
			(void) close(ptmpfd);
			(void) pw_abort();
			errx(EXIT_FAILURE, "group %s not found", up->u_primgrp);
		}
		/* if changing name, check new name isn't already in use */
		if (strcmp(login, newlogin) != 0 && getpwnam(newlogin) != (struct passwd *) NULL) {
			(void) close(ptmpfd);
			(void) pw_abort();
			errx(EXIT_FAILURE, "already a `%s' user", newlogin);
		}
		/* if home directory hasn't been given, use the old one */
		if (!up->u_homeset) {
			(void) strlcpy(home, pwp->pw_dir, sizeof(home));
		} else {
		  	(void) strlcpy(home, up->u_home, sizeof(home));
		}
		expire = 0;
		if (up->u_expire != NULL) {
			(void) memset(&tm, 0, sizeof(tm));
			if (strptime(up->u_expire, "%c", &tm) == NULL) {
				warnx("invalid time format `%s'", optarg);
			} else {
				expire = mktime(&tm);
			}
		}
		if (up->u_password != NULL &&
		    strlen(up->u_password) <= PasswordLength) {
			(void) strlcpy(password, up->u_password, sizeof(password));
		} else {
			(void) strlcpy(password, pwp->pw_passwd, sizeof(password));
		}
		if (strcmp(up->u_comment, DEF_COMMENT) == 0) {
			memsave(&up->u_comment, pwp->pw_gecos, strlen(pwp->pw_gecos));
		}
		if (strcmp(up->u_shell, DEF_SHELL) == 0 && strcmp(pwp->pw_shell, DEF_SHELL) != 0) {
			memsave(&up->u_shell, pwp->pw_shell, strlen(pwp->pw_shell));
		}
	}
	loginc = strlen(login);
	while (fgets(buf, sizeof(buf), master) != NULL) {
		cc = strlen(buf);
		if ((colon = strchr(buf, ':')) == NULL) {
			warnx("Malformed entry `%s'. Skipping", buf);
			continue;
		}
		colonc = (size_t)(colon - buf);
		if (strncmp(login, buf, loginc) == 0 && loginc == colonc) {
			if (up != (user_t *) NULL) {
				cc = snprintf(buf, sizeof(buf), "%s:%s:%d:%d::%d:%ld:%s:%s:%s\n",
					newlogin,
					password,
					up->u_uid,
					gid,
					up->u_inactive,
					(long) expire,
					up->u_comment,
					home,
					up->u_shell);
				if (cc >= MaxPasswordEntryLen ||
				    (strchr(up->u_comment, '&') != NULL &&
				     cc + strlen(newlogin) >= MaxPasswordEntryLen)) {
					(void) close(ptmpfd);
					(void) pw_abort();
					errx(EXIT_FAILURE, "can't add `%s', line too long (%d bytes)", buf, cc + strlen(newlogin));
				}
				if (write(ptmpfd, buf, (size_t) cc) != cc) {
					(void) close(ptmpfd);
					(void) pw_abort();
					err(EXIT_FAILURE, "can't add `%s'", buf);
				}
				/* Modify secondary groups */
				grp = getgrgid (gid);
				if (up->u_groupc != 0 || 
				    strcmp (login, newlogin))
					if (!modify_groups (login, NULL, up, grp ? grp->gr_name : newlogin)) {
						(void) close(ptmpfd);
				    		(void) pw_abort();
						err(EXIT_FAILURE, 
						    "failed to modify secondary groups for login %s", 
						    login);
					}
			}
		} else if (write(ptmpfd, buf, (size_t)(cc)) != cc) {
			(void) close(masterfd);
			(void) close(ptmpfd);
			(void) pw_abort();
			err(EXIT_FAILURE, "short write to /etc/ptmp (not %d chars)", cc);
		}
	}
	if (up != (user_t *) NULL &&
	    up->u_mkdir &&
	    asystem("%s %s %s", MV, oldhome, home) != 0) {
		(void) close(ptmpfd);
		(void) pw_abort();
		err(EXIT_FAILURE, "can't move `%s' to `%s'", oldhome, home);
	}
	(void) close(ptmpfd);
	if (up != (user_t *) NULL && strcmp(login, newlogin) == 0)
		cc = pw_mkdb(login);
	else
		cc = pw_mkdb(NULL);
	if (cc < 0) {
		err(EXIT_FAILURE, "pw_mkdb failed");
	}
	return 1;
}


#ifdef EXTENSIONS
/* see if we can find out the user struct */
static struct passwd *
find_user_info(char *name)
{
	struct passwd	*pwp;

	if ((pwp = getpwnam(name)) != (struct passwd *) NULL) {
		return pwp;
	}
	if (is_number(name) && (pwp = getpwuid((uid_t)atoi(name))) != (struct passwd *) NULL) {
		return pwp;
	}
	return (struct passwd *) NULL;
}
#endif

#ifdef EXTENSIONS
/* see if we can find out the group struct */
static struct group *
find_group_info(char *name)
{
	struct group	*grp;

	if ((grp = getgrnam(name)) != (struct group *) NULL) {
		return grp;
	}
	if (is_number(name) && (grp = getgrgid((gid_t)atoi(name))) != (struct group *) NULL) {
		return grp;
	}
	return (struct group *) NULL;
}
#endif

/* print out usage message, and then exit */
void
usermgmt_usage(char *prog)
{
	if (strcmp(prog, "useradd") == 0) {
		(void) fprintf(stderr, "usage: %s -D [-b basedir] [-e expiry] "
		    "[-f inactive] [-g group]\n\t\t[-r lowuid..highuid] "
		    "[-s shell]\n", prog);
		(void) fprintf(stderr, "usage: %s [-G group[,group,...]] "
		    "[-b basedir] [-c comment] [-d homedir]\n\t\t"
		    "[-e expiry] [-f inactive] [-g group] "
		    "[-k skeletondir]\n\t\t[-m] [-o] [-p password] "
		    "[-r lowuid..highuid]\n\t\t[-s shell] [-u uid] "
		    "[-v] user\n", prog);
	} else if (strcmp(prog, "usermod") == 0) {
		(void) fprintf(stderr, "usage: %s [-G group[,group,...]] "
		    "[-c comment] [-d homedir] [-e expire]\n\t\t"
		    "[-f inactive] [-g group] [-l newname] [-m] [-o]\n\t\t"
		    "[-p password] [-s shell] [-u uid] [-v] user\n", prog);
	} else if (strcmp(prog, "userdel") == 0) {
		(void) fprintf(stderr, "usage: %s -D [-p preserve]\n", prog);
		(void) fprintf(stderr, "usage: %s [-p preserve] [-r] [-v] "
		    "user\n", prog);
#ifdef EXTENSIONS
	} else if (strcmp(prog, "userinfo") == 0) {
		(void) fprintf(stderr, "usage: %s [-e] [-v] user\n", prog);
#endif
	} else if (strcmp(prog, "groupadd") == 0) {
		(void) fprintf(stderr, "usage: %s [-g gid] [-o] [-v] group\n",
		    prog);
	} else if (strcmp(prog, "groupdel") == 0) {
		(void) fprintf(stderr, "usage: %s [-v] group\n", prog);
	} else if (strcmp(prog, "groupmod") == 0) {
		(void) fprintf(stderr, "usage: %s [-g gid] [-o] [-n newname] "
		    "[-v] group\n", prog);
	} else if (strcmp(prog, "user") == 0 || strcmp(prog, "group") == 0) {
		(void) fprintf(stderr, "usage: %s [ add | del | mod "
#ifdef EXTENSIONS
		"| info "
#endif
		"] ...\n",
		    prog);
#ifdef EXTENSIONS
	} else if (strcmp(prog, "groupinfo") == 0) {
		(void) fprintf(stderr, "usage: %s [-e] [-v] group\n", prog);
#endif
	}
	exit(EXIT_FAILURE);
	/* NOTREACHED */
}

#ifdef EXTENSIONS
#define ADD_OPT_EXTENSIONS	"p:r:v"
#else
#define ADD_OPT_EXTENSIONS	
#endif

int
useradd(int argc, char **argv)
{
	user_t	u;
	int	defaultfield;
	int	bigD;
	int	c;
	int	i;
	char    buf[MaxEntryLen], *s, *p;

	(void) memset(&u, 0, sizeof(u));
	read_defaults(&u);
	u.u_uid = -1;
	defaultfield = bigD = 0;
	while ((c = getopt(argc, argv, "DG:b:c:d:e:f:g:k:mou:s:" ADD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'D':
			bigD = 1;
			break;
		case 'G':
		        strlcpy (buf, optarg, sizeof(buf));
			p = buf;
			while ((s = strsep (&p, ",")) != NULL &&
			       u.u_groupc < NGROUPS_MAX - 2)
			  	memsave (&u.u_groupv[u.u_groupc++], s, 
					 strlen(s));
			if (p != NULL)
			  	errx(EXIT_FAILURE, "too many groups for -G");
			break;
		case 'b':
			defaultfield = 1;
			memsave(&u.u_basedir, optarg, strlen(optarg));
			break;
		case 'c':
			memsave(&u.u_comment, optarg, strlen(optarg));
			break;
		case 'd':
			u.u_homeset = 1;
			memsave(&u.u_home, optarg, strlen(optarg));
			break;
		case 'e':
			defaultfield = 1;
			memsave(&u.u_expire, optarg, strlen(optarg));
			break;
		case 'f':
			defaultfield = 1;
			u.u_inactive = atoi(optarg);
			break;
		case 'g':
			defaultfield = 1;
			memsave(&u.u_primgrp, optarg, strlen(optarg));
			break;
		case 'k':
			memsave(&u.u_skeldir, optarg, strlen(optarg));
			break;
		case 'm':
			u.u_mkdir = 1;
			break;
		case 'o':
			u.u_dupuid = 1;
			break;
#ifdef EXTENSIONS
		case 'p':
			memsave(&u.u_password, optarg, strlen(optarg));
			break;
#endif
#ifdef EXTENSIONS
		case 'r':
			defaultfield = 1;
			(void) save_range(&u, optarg);
			break;
#endif
		case 's':
			defaultfield = 1;
			memsave(&u.u_shell, optarg, strlen(optarg));
			break;
		case 'u':
			if (!is_number(optarg)) {
				errx(EXIT_FAILURE, "When using [-u uid], the uid must be numeric");
			}
			u.u_uid = atoi(optarg);
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		}
	}
	if (bigD) {
		if (defaultfield) {
			checkeuid();
			return setdefaults(&u) ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		(void) printf("group\t\t%s\n", u.u_primgrp);
		(void) printf("base_dir\t%s\n", u.u_basedir);
		(void) printf("skel_dir\t%s\n", u.u_skeldir);
		(void) printf("shell\t\t%s\n", u.u_shell);
		(void) printf("inactive\t%d\n", u.u_inactive);
		(void) printf("expire\t\t%s\n", (u.u_expire == NULL) ? UNSET_EXPIRY : u.u_expire);
#ifdef EXTENSIONS
		for (i = 0 ; i < u.u_rc ; i++) {
			(void) printf("range\t\t%d..%d\n", u.u_rv[i].r_from, u.u_rv[i].r_to);
		}
#endif
		return EXIT_SUCCESS;
	}
	if (argc == optind) {
		usermgmt_usage("useradd");
	}
	checkeuid();
	return adduser(argv[optind], &u) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef EXTENSIONS
#define MOD_OPT_EXTENSIONS	"p:v"
#else
#define MOD_OPT_EXTENSIONS	
#endif

int
usermod(int argc, char **argv)
{
	user_t	u;
	char	newuser[MaxUserNameLen + 1];
	int	have_new_user;
	int	c;
	char    buf[MaxEntryLen], *s, *p;

	(void) memset(&u, 0, sizeof(u));
	(void) memset(newuser, 0, sizeof(newuser));
	read_defaults(&u);
	u.u_uid = -1;
	free(u.u_primgrp);
	u.u_primgrp = NULL;
	have_new_user = 0;
	while ((c = getopt(argc, argv, "G:c:d:e:f:g:l:mos:u:" MOD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'G':
		        strlcpy (buf, optarg, sizeof(buf));
			p = buf;
			while ((s = strsep (&p, ",")) != NULL &&
			       u.u_groupc < NGROUPS_MAX - 2)
			  	memsave (&u.u_groupv[u.u_groupc++], s, 
					 strlen(s));
			if (p != NULL)
			  	errx(EXIT_FAILURE, "too many groups for -G");
			break;
		case 'c':
			memsave(&u.u_comment, optarg, strlen(optarg));
			break;
		case 'd':
			u.u_homeset = 1;
			memsave(&u.u_home, optarg, strlen(optarg));
			break;
		case 'e':
			memsave(&u.u_expire, optarg, strlen(optarg));
			break;
		case 'f':
			u.u_inactive = atoi(optarg);
			break;
		case 'g':
			memsave(&u.u_primgrp, optarg, strlen(optarg));
			break;
		case 'l':
			have_new_user = 1;
			(void) strlcpy(newuser, optarg, sizeof(newuser));
			break;
		case 'm':
			u.u_mkdir = 1;
			break;
		case 'o':
			u.u_dupuid = 1;
			break;
#ifdef EXTENSIONS
		case 'p':
			memsave(&u.u_password, optarg, strlen(optarg));
			break;
#endif
		case 's':
			memsave(&u.u_shell, optarg, strlen(optarg));
			break;
		case 'u':
			if (!is_number(optarg)) {
				errx(EXIT_FAILURE, "When using [-u uid], the uid must be numeric");
			}
			u.u_uid = atoi(optarg);
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		}
	}
	if (argc == optind) {
		usermgmt_usage("usermod");
	}
	checkeuid();
	return moduser(argv[optind], (have_new_user) ? newuser : argv[optind],
		       &u) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef EXTENSIONS
#define DEL_OPT_EXTENSIONS	"Dp:v"
#else
#define DEL_OPT_EXTENSIONS	
#endif

int
userdel(int argc, char **argv)
{
	struct passwd	*pwp;
	struct stat	st;
	user_t		u;
	char		password[PasswordLength + 1];
	int		defaultfield;
	int		rmhome;
	int		bigD;
	int		c;

	(void) memset(&u, 0, sizeof(u));
	read_defaults(&u);
	defaultfield = bigD = rmhome = 0;
	while ((c = getopt(argc, argv, "r" DEL_OPT_EXTENSIONS)) != -1) {
		switch(c) {
#ifdef EXTENSIONS
		case 'D':
			bigD = 1;
			break;
#endif
#ifdef EXTENSIONS
		case 'p':
			defaultfield = 1;
			u.u_preserve = (strcmp(optarg, "true") == 0) ? 1 :
					(strcmp(optarg, "yes") == 0) ? 1 :
					 atoi(optarg);
			break;
#endif
		case 'r':
			rmhome = 1;
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		}
	}
#ifdef EXTENSIONS
	if (bigD) {
		if (defaultfield) {
			checkeuid();
			return setdefaults(&u) ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		(void) printf("preserve\t%s\n", (u.u_preserve) ? "true" : "false");
		return EXIT_SUCCESS;
	}
#endif
	if (argc == optind) {
		usermgmt_usage("userdel");
	}
	checkeuid();
	if ((pwp = getpwnam(argv[optind])) == (struct passwd *) NULL) {
		warnx("No such user `%s'", argv[optind]);
		return EXIT_FAILURE;
	}
	if (rmhome) {
		if (stat(pwp->pw_dir, &st) < 0) {
			warn("Home directory `%s' does not exist", pwp->pw_dir);
			return EXIT_FAILURE;
		}
		(void) asystem("%s -rf %s", RM, pwp->pw_dir);
	}
	if (u.u_preserve) {
		memsave(&u.u_shell, NOLOGIN, strlen(NOLOGIN));
		(void) strlcpy(password, "*", sizeof(password));
		memsave(&u.u_password, password, PasswordLength);
		return moduser(argv[optind], argv[optind], &u) ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	return moduser(argv[optind], argv[optind], (user_t *) NULL) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#ifdef EXTENSIONS
#define GROUP_ADD_OPT_EXTENSIONS	"v"
#else
#define GROUP_ADD_OPT_EXTENSIONS	
#endif

/* add a group */
int
groupadd(int argc, char **argv)
{
	int	dupgid;
	int	gid;
	int	c;

	gid = -1;
	dupgid = 0;
	while ((c = getopt(argc, argv, "g:o" GROUP_ADD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'g':
			if (!is_number(optarg)) {
				errx(EXIT_FAILURE, "When using [-g gid], the gid must be numeric");
			}
			gid = atoi(optarg);
			break;
		case 'o':
			dupgid = 1;
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		}
	}
	if (argc == optind) {
		usermgmt_usage("groupadd");
	}
	checkeuid();
	if (gid < 0 && !getnextgid(&gid, LowGid, HighGid)) {
		err(EXIT_FAILURE, "can't add group: can't get next gid");
	}
	if (!dupgid && getgrgid((gid_t) gid) != (struct group *) NULL) {
		errx(EXIT_FAILURE, "can't add group: gid %d is a duplicate", gid);
	}
	if (!valid_group(argv[optind])) {
		warnx("warning - invalid group name `%s'", argv[optind]);
	}
	if (!creategid(argv[optind], gid)) {
		err(EXIT_FAILURE, "can't add group: problems with %s file", _PATH_GROUP);
	}
	return EXIT_SUCCESS;
}

#ifdef EXTENSIONS
#define GROUP_DEL_OPT_EXTENSIONS	"v"
#else
#define GROUP_DEL_OPT_EXTENSIONS	
#endif

/* remove a group */
int
groupdel(int argc, char **argv)
{
	int	c;

	while ((c = getopt(argc, argv, "" GROUP_DEL_OPT_EXTENSIONS)) != -1) {
		switch(c) {
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		}
	}
	if (argc == optind) {
		usermgmt_usage("groupdel");
	}
	checkeuid();
	if (!modify_gid(argv[optind], NULL)) {
		err(EXIT_FAILURE, "can't change %s file", _PATH_GROUP);
	}
	return EXIT_SUCCESS;
}

#ifdef EXTENSIONS
#define GROUP_MOD_OPT_EXTENSIONS	"v"
#else
#define GROUP_MOD_OPT_EXTENSIONS	
#endif

/* modify a group */
int
groupmod(int argc, char **argv)
{
	struct group	*grp;
	char		buf[MaxEntryLen];
	char		*newname;
	char		**cpp;
	int		dupgid;
	int		gid;
	int		cc;
	int		c;

	gid = -1;
	dupgid = 0;
	newname = NULL;
	while ((c = getopt(argc, argv, "g:on:" GROUP_MOD_OPT_EXTENSIONS)) != -1) {
		switch(c) {
		case 'g':
			if (!is_number(optarg)) {
				errx(EXIT_FAILURE, "When using [-g gid], the gid must be numeric");
			}
			gid = atoi(optarg);
			break;
		case 'o':
			dupgid = 1;
			break;
		case 'n':
			memsave(&newname, optarg, strlen(optarg));
			break;
#ifdef EXTENSIONS
		case 'v':
			verbose = 1;
			break;
#endif
		}
	}
	if (argc == optind) {
		usermgmt_usage("groupmod");
	}
	checkeuid();
	if (gid < 0 && newname == NULL) {
		err(EXIT_FAILURE, "Nothing to change");
	}
	if (dupgid && gid < 0) {
		err(EXIT_FAILURE, "Duplicate which gid?");
	}
	if ((grp = getgrnam(argv[optind])) == (struct group *) NULL) {
		err(EXIT_FAILURE, "can't find group `%s' to modify", argv[optind]);
	}
	if (newname != NULL && !valid_group(newname)) {
		warn("warning - invalid group name `%s'", newname);
	}
	cc = snprintf(buf, sizeof(buf), "%s:%s:%d:",
			(newname) ? newname : grp->gr_name,
			grp->gr_passwd,
			(gid < 0) ? grp->gr_gid : gid);
	for (cpp = grp->gr_mem ; *cpp && cc < sizeof(buf) ; cpp++) {
		cc += snprintf(&buf[cc], sizeof(buf) - cc, "%s%s", *cpp,
			(cpp[1] == NULL) ? "" : ",");
	}
	cc += snprintf(&buf[cc], sizeof(buf) - cc, "\n");
	if (!modify_gid(argv[optind], buf)) {
		err(EXIT_FAILURE, "can't change %s file", _PATH_GROUP);
	}
	return EXIT_SUCCESS;
}

#ifdef EXTENSIONS
/* display user information */
int
userinfo(int argc, char **argv)
{
	struct passwd	*pwp;
	struct group	*grp;
	char		buf[MaxEntryLen];
	char		**cpp;
	int		exists;
	int		cc;
	int		i;

	exists = 0;
	while ((i = getopt(argc, argv, "ev")) != -1) {
		switch(i) {
		case 'e':
			exists = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		}
	}
	if (argc == optind) {
		usermgmt_usage("userinfo");
	}
	pwp = find_user_info(argv[optind]);
	if (exists) {
		exit((pwp) ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	if (pwp == (struct passwd *) NULL) {
		errx(EXIT_FAILURE, "can't find user `%s'", argv[optind]);
	}
	(void) printf("login\t%s\n", pwp->pw_name);
	(void) printf("passwd\t%s\n", pwp->pw_passwd);
	(void) printf("uid\t%d\n", pwp->pw_uid);
	for (cc = 0 ; (grp = getgrent()) != (struct group *) NULL ; ) {
		for (cpp = grp->gr_mem ; *cpp ; cpp++) {
			if (strcmp(*cpp, argv[optind]) == 0 && grp->gr_gid != pwp->pw_gid) {
				cc += snprintf(&buf[cc], sizeof(buf) - cc, "%s ", grp->gr_name);
			}
		}
	}
	if ((grp = getgrgid(pwp->pw_gid)) == (struct group *) NULL) {
		(void) printf("groups\t%d %s\n", pwp->pw_gid, buf);
	} else {
		(void) printf("groups\t%s %s\n", grp->gr_name, buf);
	}
	(void) printf("change\t%s", ctime(&pwp->pw_change));
	(void) printf("class\t%s\n", pwp->pw_class);
	(void) printf("gecos\t%s\n", pwp->pw_gecos);
	(void) printf("dir\t%s\n", pwp->pw_dir);
	(void) printf("shell\t%s\n", pwp->pw_shell);
	(void) printf("expire\t%s", ctime(&pwp->pw_expire));
	return EXIT_SUCCESS;
}
#endif

#ifdef EXTENSIONS
/* display user information */
int
groupinfo(int argc, char **argv)
{
	struct group	*grp;
	char		**cpp;
	int		exists;
	int		i;

	exists = 0;
	while ((i = getopt(argc, argv, "ev")) != -1) {
		switch(i) {
		case 'e':
			exists = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		}
	}
	if (argc == optind) {
		usermgmt_usage("groupinfo");
	}
	grp = find_group_info(argv[optind]);
	if (exists) {
		exit((grp) ? EXIT_SUCCESS : EXIT_FAILURE);
	}
	if (grp == (struct group *) NULL) {
		errx(EXIT_FAILURE, "can't find group `%s'", argv[optind]);
	}
	(void) printf("name\t%s\n", grp->gr_name);
	(void) printf("passwd\t%s\n", grp->gr_passwd);
	(void) printf("gid\t%d\n", grp->gr_gid);
	(void) printf("members\t");
	for (cpp = grp->gr_mem ; *cpp ; cpp++) {
		(void) printf("%s ", *cpp);
	}
	(void) fputc('\n', stdout);
	return EXIT_SUCCESS;
}
#endif
