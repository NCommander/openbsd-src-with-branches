/*	$OpenBSD: util.c,v 1.11 2003/04/08 00:18:31 deraadt Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: util.c,v 1.11 2003/04/08 00:18:31 deraadt Exp $";
#endif /* not lint */

#include "EXTERN.h"
#include "common.h"
#include "INTERN.h"
#include "util.h"
#include "backupfile.h"

void my_exit(int) __attribute__((noreturn));

/* Rename a file, copying it if necessary. */

int
move_file(from,to)
char *from, *to;
{
    char bakname[MAXPATHLEN];
    char *s;
    int i;
    int fromfd;

    /* to stdout? */

    if (strEQ(to, "-")) {
#ifdef DEBUGGING
	if (debug & 4)
	    say("Moving %s to stdout.\n", from);
#endif
	fromfd = open(from, O_RDONLY);
	if (fromfd < 0)
	    pfatal("internal error, can't reopen %s", from);
	while ((i=read(fromfd, buf, sizeof buf)) > 0)
	    if (write(1, buf, i) != 1)
		pfatal("write failed");
	close(fromfd);
	return 0;
    }

    if (origprae) {
	if (strlcpy(bakname, origprae, sizeof(bakname)) >= sizeof(bakname) ||
	    strlcat(bakname, to, sizeof(bakname)) >= sizeof(bakname))
	    fatal("filename %s too long for buffer\n", origprae);
    } else {
#ifndef NODIR
	char *backupname = find_backup_file_name(to);
	if (backupname == (char *) 0)
	    fatal("out of memory\n");
	if (strlcpy(bakname, backupname, sizeof(bakname)) >= sizeof(bakname))
	    fatal("filename %s too long for buffer\n", backupname);
	free(backupname);
#else /* NODIR */
	if (strlcpy(bakname, to, sizeof(bakname)) >= sizeof(bakname) ||
	    strlcat(bakname, simple_backup_suffix, sizeof(bakname)) >= sizeof(bakname))
	    fatal("filename %s too long for buffer\n", to);
#endif /* NODIR */
    }

    if (stat(to, &filestat) == 0) {	/* output file exists */
	dev_t to_device = filestat.st_dev;
	ino_t to_inode  = filestat.st_ino;
	char *simplename = bakname;

	for (s=bakname; *s; s++) {
	    if (*s == '/')
		simplename = s+1;
	}
	/* Find a backup name that is not the same file.
	   Change the first lowercase char into uppercase;
	   if that isn't sufficient, chop off the first char and try again.  */
	while (stat(bakname, &filestat) == 0 &&
		to_device == filestat.st_dev && to_inode == filestat.st_ino) {
	    /* Skip initial non-lowercase chars.  */
	    for (s=simplename; *s && !islower(*s); s++) ;
	    if (*s)
		*s = toupper(*s);
	    else
		memmove(simplename, simplename+1, strlen(simplename+1)+1);
	}
	while (unlink(bakname) >= 0) ;	/* while() is for benefit of Eunice */
#ifdef DEBUGGING
	if (debug & 4)
	    say("Moving %s to %s.\n", to, bakname);
#endif
	if (link(to, bakname) < 0) {
	    /* Maybe `to' is a symlink into a different file system.
	       Copying replaces the symlink with a file; using rename
	       would be better.  */
	    int tofd;
	    int bakfd;

	    bakfd = creat(bakname, 0666);
	    if (bakfd < 0) {
		say("Can't backup %s, output is in %s: %s\n", to, from,
		     strerror(errno));
		return -1;
	    }
	    tofd = open(to, O_RDONLY);
	    if (tofd < 0)
		pfatal("internal error, can't open %s", to);
	    while ((i=read(tofd, buf, sizeof buf)) > 0)
		if (write(bakfd, buf, i) != i)
		    pfatal("write failed");
	    close(tofd);
	    close(bakfd);
	}
	while (unlink(to) >= 0) ;
    }
#ifdef DEBUGGING
    if (debug & 4)
	say("Moving %s to %s.\n", from, to);
#endif
    if (link(from, to) < 0) {		/* different file system? */
	int tofd;

	tofd = creat(to, 0666);
	if (tofd < 0) {
	    say("Can't create %s, output is in %s: %s\n",
	      to, from, strerror(errno));
	    return -1;
	}
	fromfd = open(from, O_RDONLY);
	if (fromfd < 0)
	    pfatal("internal error, can't reopen %s", from);
	while ((i=read(fromfd, buf, sizeof buf)) > 0)
	    if (write(tofd, buf, i) != i)
		pfatal("write failed");
	close(fromfd);
	close(tofd);
    }
    unlink(from);
    return 0;
}

/* Copy a file. */

void
copy_file(from,to)
char *from, *to;
{
    int tofd;
    int fromfd;
    int i;

    tofd = creat(to, 0666);
    if (tofd < 0)
	pfatal("can't create %s", to);
    fromfd = open(from, O_RDONLY);
    if (fromfd < 0)
	pfatal("internal error, can't reopen %s", from);
    while ((i=read(fromfd, buf, sizeof buf)) > 0)
	if (write(tofd, buf, i) != i)
	    pfatal("write to %s failed", to);
    close(fromfd);
    close(tofd);
}

/* Allocate a unique area for a string. */

char *
savestr(s)
char *s;
{
    char *rv;
    char *t;

    if (!s)
	s = "Oops";
    t = s;
    while (*t++);
    rv = malloc((MEM) (t - s));
    if (rv == Nullch) {
	if (using_plan_a)
	    out_of_mem = TRUE;
	else
	    fatal("out of memory\n");
    }
    else {
	t = rv;
	while ((*t++ = *s++))
	    ;
    }
    return rv;
}

/* Vanilla terminal output (buffered). */

void
say(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fflush(stderr);
}

/* Terminal output, pun intended. */

void
fatal(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "patch: **** ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    my_exit(1);
}

/* Say something from patch, something from the system, then silence . . . */

void
pfatal(char *fmt, ...)
{
    va_list ap;
    int errnum = errno;

    fprintf(stderr, "patch: **** ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, ": %s\n", strerror(errnum));
    my_exit(1);
}

/* Get a response from the user, somehow or other. */

void
ask(char *fmt, ...)
{
    va_list ap;
    int ttyfd;
    int r;
    bool tty2 = isatty(2);

    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    fflush(stderr);
    write(2, buf, strlen(buf));
    if (tty2) {				/* might be redirected to a file */
	r = read(2, buf, sizeof buf);
    } else if (isatty(1)) {		/* this may be new file output */
	fflush(stdout);
	write(1, buf, strlen(buf));
	r = read(1, buf, sizeof buf);
    } else if ((ttyfd = open(_PATH_TTY, O_RDWR)) >= 0 && isatty(ttyfd)) {
	/* might be deleted or unwriteable */
	write(ttyfd, buf, strlen(buf));
	r = read(ttyfd, buf, sizeof buf);
	close(ttyfd);
    } else if (isatty(0)) {		/* this is probably patch input */
	fflush(stdin);
	write(0, buf, strlen(buf));
	r = read(0, buf, sizeof buf);
    } else {				/* no terminal at all--default it */
	buf[0] = '\n';
	r = 1;
    }
    if (r <= 0)
	buf[0] = 0;
    else
	buf[r] = '\0';
    if (!tty2)
	say(buf);
}

/* How to handle certain events when not in a critical region. */

void
set_signals(reset)
int reset;
{
#ifndef lint
    static sig_t hupval, intval;

    if (!reset) {
	hupval = signal(SIGHUP, SIG_IGN);
	if (hupval != SIG_IGN)
	    hupval = (sig_t)my_exit;
	intval = signal(SIGINT, SIG_IGN);
	if (intval != SIG_IGN)
	    intval = (sig_t)my_exit;
    }
    signal(SIGHUP, hupval);
    signal(SIGINT, intval);
#endif
}

/* How to handle certain events when in a critical region. */

void
ignore_signals()
{
#ifndef lint
    signal(SIGHUP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
#endif
}

/* Make sure we'll have the directories to create a file.
   If `striplast' is TRUE, ignore the last element of `filename'.  */

void
makedirs(filename,striplast)
char *filename;
bool striplast;
{
    char *tmpbuf;

    if ((tmpbuf = strdup(filename)) == NULL)
        fatal("out of memory\n");

    if (striplast) {
        char *s = strrchr(tmpbuf, '/');
	if (s == NULL)
	  return; /* nothing to be done */
	*s = '\0';
    }

    strlcpy(buf, "/bin/mkdir -p ", sizeof buf);
    if (strlcat(buf, tmpbuf, sizeof(buf)) >= sizeof(buf))
      fatal("buffer too small to hold %.20s...\n", tmpbuf);

    if (system(buf))
      pfatal("%.40s failed", buf);
}

/* Make filenames more reasonable. */

char *
fetchname(at,strip_leading,assume_exists)
char *at;
int strip_leading;
int assume_exists;
{
    char *fullname;
    char *name;
    char *t;
    char tmpbuf[200];
    int sleading = strip_leading;

    if (!at || *at == '\0')
	return Nullch;
    while (isspace(*at))
	at++;
#ifdef DEBUGGING
    if (debug & 128)
	say("fetchname %s %d %d\n",at,strip_leading,assume_exists);
#endif
    if (strnEQ(at, "/dev/null", 9))	/* so files can be created by diffing */
	return Nullch;			/*   against /dev/null. */
    name = fullname = t = savestr(at);

    /* Strip off up to `sleading' leading slashes and null terminate.  */
    for (; *t && !isspace(*t); t++)
	if (*t == '/')
	    if (--sleading >= 0)
		name = t+1;
    *t = '\0';

    /* If no -p option was given (957 is the default value!),
       we were given a relative pathname,
       and the leading directories that we just stripped off all exist,
       put them back on.  */
    if (strip_leading == 957 && name != fullname && *fullname != '/') {
	name[-1] = '\0';
	if (stat(fullname, &filestat) == 0 && S_ISDIR (filestat.st_mode)) {
	    name[-1] = '/';
	    name=fullname;
	}
    }

    name = savestr(name);
    free(fullname);

    if (stat(name, &filestat) && !assume_exists) {
	char *filebase = basename(name);
	char *filedir = dirname(name);

#define try(f, a1, a2, a3) (snprintf(tmpbuf, sizeof tmpbuf, f, a1, a2, a3), stat(tmpbuf, &filestat) == 0)
	if (   try("%s/RCS/%s%s", filedir, filebase, RCSSUFFIX)
	    || try("%s/RCS/%s%s", filedir, filebase,        "")
	    || try(    "%s/%s%s", filedir, filebase, RCSSUFFIX)
	    || try("%s/SCCS/%s%s", filedir, SCCSPREFIX, filebase)
	    || try(     "%s/%s%s", filedir, SCCSPREFIX, filebase))
	  return name;
	free(name);
	name = Nullch;
    }

    return name;
}

void
version()
{
    fprintf(stderr, "Patch version 2.0-12u8-OpenBSD\n");
    my_exit(0);
}
