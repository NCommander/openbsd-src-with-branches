/*
 * This program is in the public domain and may be used freely by anyone
 * who wants to.
 *
 * Please send bug fixes/bug reports to: Peter Eriksson <pen@lysator.liu.se>
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <stdio.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <nlist.h>
#include <kvm.h>
#include <unistd.h>

#include "identd.h"
#include "error.h"

#define IO_TIMEOUT	30	/* Timeout I/O operations after N seconds */

int check_noident __P((char *));
ssize_t timed_read __P((int, void *, size_t, time_t));
ssize_t timed_write __P((int, const void *, size_t, time_t));
void gentoken __P((char *, int));

/*
 * A small routine to check for the existance of the ".noident"
 * file in a users home directory.
 */
int
check_noident(homedir)
	char   *homedir;
{
	char   path[MAXPATHLEN];
	struct stat st;

	if (!homedir)
		return 0;
	if (snprintf(path, sizeof path, "%s/.noident", homedir) >= sizeof path)
		return 0;
	if (stat(path, &st) == 0)
		return 1;
	return 0;
}

static char token0cnv[] = "abcdefghijklmnopqrstuvwxyz";
static char tokencnv[] = "abcdefghijklmnopqrstuvwxyz0123456789";

void
gentoken(buf, len)
	char *buf;
	int len;
{
	char *p;

	if (len == 0)
		return;
	for (p = buf; len > 1; p++, len--) {
		if (p == buf)
			*p = token0cnv[arc4random() % (sizeof token0cnv-1)];
		else
			*p = tokencnv[arc4random() % (sizeof tokencnv-1)];
	}
	*p = '\0';
}

/*
 * Returns 0 on timeout, -1 on error, #bytes read on success.
 */
ssize_t
timed_read(fd, buf, siz, timeout)
	int fd;
	void *buf;
	size_t siz;
	time_t timeout;
{
	int error, tot = 0, i, r;
	char *p = buf;
	struct pollfd rfd[1];
	struct timeval tv, start, after, duration, tmp;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	while (1) {
		rfd[0].fd = fd;
		rfd[0].events = POLLIN;
		rfd[0].revents = 0;

		gettimeofday(&start, NULL);
		if ((error = poll(rfd, 1, tv.tv_sec * 1000 +
		    tv.tv_usec / 1000)) <= 0)
			return error;
		r = read(fd, p, siz - tot);
		if (r == -1 || r == 0)
			return (r);
		for (i = 0; i < r; i++)
			if (p[i] == '\r' || p[i] == '\n') {
				tot += r;
				return (tot);
			}
		gettimeofday(&after, NULL);
		timersub(&start, &after, &duration);
		timersub(&tv, &duration, &tmp);
		tv = tmp;
		if (tv.tv_sec < 0 || !timerisset(&tv))
			return (tot);
		tot += r;
		p += r;
	}
}

/*
 * Returns 0 on timeout, -1 on error, #bytes read on success.
 */
ssize_t
timed_write(fd, buf, siz, timeout)
	int fd;
	const void *buf;
	size_t siz;
	time_t timeout;
{
	int error;
	fd_set writeds;
	struct timeval tv;

	FD_ZERO(&writeds);
	FD_SET(fd, &writeds);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	if ((error = select(fd + 1, 0, &writeds, 0, &tv)) <= 0)
		return error;
	return(write(fd, buf, siz));
}

int
parse(fd, laddr, faddr)
	int fd;
	struct in_addr *laddr, *faddr;
{
	char	buf[BUFSIZ], *p;
	struct	in_addr laddr2, faddr2;
	struct	passwd *pw;
	int	n;
	uid_t	uid;

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "In function parse(), from %s to %s", 
		       gethost(faddr), gethost(laddr));

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "  Before read from remote host");
	faddr2 = *faddr;
	laddr2 = *laddr;
	lport = fport = 0;

	/* Read query from client */
	if ((n = timed_read(fd, buf, sizeof(buf) - 1, IO_TIMEOUT)) <= 0) {
		if (syslog_flag)
			syslog(LOG_NOTICE, "read from %s: %m", gethost(faddr));
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : ERROR : UNKNOWN-ERROR\r\n", lport, fport);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost(faddr));
			return 1;
		}
		return 0;
	}
	buf[n] = '\0';

	/* Pull out local and remote ports */
	p = buf;
	while (*p != '\0' && isspace(*p))
		p++;
	if ((p = strtok(p, " \t,"))) {
		lport = atoi(p);
		if ((p = strtok(NULL, " \t,")))
			fport = atoi(p);
	}

	if (lport < 1 || lport > 65535 || fport < 1 || fport > 65535) {
		if (syslog_flag)
			syslog(LOG_NOTICE,
			    "scanf: invalid-port(s): %d , %d from %s",
			    lport, fport, gethost(faddr));
		n = snprintf(buf, sizeof(buf), "%d , %d : ERROR : %s\r\n",
		    lport, fport, unknown_flag ? "UNKNOWN-ERROR" : "INVALID-PORT");
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost(faddr));
			return 1;
		}
		return 0;
	}
	if (syslog_flag && verbose_flag)
		syslog(LOG_NOTICE, "request for (%d,%d) from %s",
		    lport, fport, gethost(faddr));

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "  After fscanf(), before k_getuid()");

	/*
	 * Next - get the specific TCP connection and return the
	 * uid - user number.
	 *
	 */
	if (k_getuid(&faddr2, htons(fport), laddr,
	    htons(lport), &uid) == -1) {
		if (syslog_flag)
			syslog(LOG_DEBUG, "Returning: %d , %d : NO-USER",	
			    lport, fport);
		n = snprintf(buf, sizeof(buf), "%d , %d : ERROR : %s\r\n",
		    lport, fport, unknown_flag ? "UNKNOWN-ERROR" : "NO-USER");
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost(faddr));
			return 1;
		}
		return 0;
	}
	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "  After k_getuid(), before getpwuid()");

	pw = getpwuid(uid);
	if (!pw) {
		if (syslog_flag)
			syslog(LOG_WARNING,
			    "getpwuid() could not map uid (%d) to name",
			    uid);
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : USERID : OTHER%s%s :%d\r\n",
		    lport, fport, charset_name ? " , " : "",
		    charset_name ? charset_name : "", uid);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost(faddr));
			return 1;
		}
		return 0;
	}

	if (syslog_flag)
		syslog(LOG_DEBUG, "Successful lookup: %d , %d : %s",
		    lport, fport, pw->pw_name);

	if (noident_flag && check_noident(pw->pw_dir)) {
		if (syslog_flag && verbose_flag)
			syslog(LOG_NOTICE,
			    "user %s requested HIDDEN-USER for host %s: %d, %d",
			    pw->pw_name, gethost(faddr), lport, fport);
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : ERROR : HIDDEN-USER\r\n", lport, fport);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost(faddr));
			return 1;
		}
		return 0;
	}

	if (token_flag) {
		char token[21];

		gentoken(token, sizeof token);
		syslog(LOG_NOTICE, "token %s == uid %u (%s)", token, uid,
		    pw->pw_name);
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : USERID : OTHER%s%s :%s\r\n",
		    lport, fport, charset_name ? " , " : "",
		    charset_name ? charset_name : "", token);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost(faddr));
			return 1;
		}
		return 0;
	}
	if (number_flag) {
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : USERID : OTHER%s%s :%d\r\n",
		    lport, fport, charset_name ? " , " : "",
		    charset_name ? charset_name : "", uid);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost(faddr));
			return 1;
		}
		return 0;
	}
	n = snprintf(buf, sizeof(buf), "%d , %d : USERID : %s%s%s :%s\r\n",
	    lport, fport, other_flag ? "OTHER" : "UNIX",
	    charset_name ? " , " : "",
	    charset_name ? charset_name : "", pw->pw_name);
	if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
		syslog(LOG_NOTICE, "write to %s: %m", gethost(faddr));
		return 1;
	}
	return 0;
}


/* Parse, a-la IPv6 */
int
parse6(fd, laddr, faddr)
	int fd;
	struct sockaddr_in6 *laddr, *faddr;
{
	char	buf[BUFSIZ], *p;
	struct	sockaddr_in6 laddr2, faddr2;
	struct	passwd *pw;
	int	n;
	uid_t	uid;

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "In function parse6(), from %s to %s", 
		       gethost6(faddr), gethost6(laddr));

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "  Before read from remote host");
	faddr2 = *faddr;
	laddr2 = *laddr;
	lport = fport = 0;

	/* Read query from client */
	if ((n = timed_read(fd, buf, sizeof(buf) - 1, IO_TIMEOUT)) <= 0) {
		if (syslog_flag)
			syslog(LOG_NOTICE, "read from %s: %m", gethost6(faddr));
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : ERROR : UNKNOWN-ERROR\r\n", lport, fport);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost6(faddr));
			return 1;
		}
		return 0;
	}
	buf[n] = '\0';

	/* Pull out local and remote ports */
	p = buf;
	while (*p != '\0' && isspace(*p))
		p++;
	if ((p = strtok(p, " \t,"))) {
		lport = atoi(p);
		if ((p = strtok(NULL, " \t,")))
			fport = atoi(p);
	}

	if (lport < 1 || lport > 65535 || fport < 1 || fport > 65535) {
		if (syslog_flag)
			syslog(LOG_NOTICE,
			    "scanf: invalid-port(s): %d , %d from %s",
			    lport, fport, gethost6(faddr));
		n = snprintf(buf, sizeof(buf), "%d , %d : ERROR : %s\r\n",
		    lport, fport, unknown_flag ? "UNKNOWN-ERROR" : "INVALID-PORT");
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost6(faddr));
			return 1;
		}
		return 0;
	}
	if (syslog_flag && verbose_flag)
		syslog(LOG_NOTICE, "request for (%d,%d) from %s",
		    lport, fport, gethost6(faddr));

	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "  After fscanf(), before k_getuid6()");

	/*
	 * Next - get the specific TCP connection and return the
	 * uid - user number.
	 *
	 */
	if (k_getuid6(&faddr2, htons(fport), laddr,
	    htons(lport), &uid) == -1) {
		if (syslog_flag)
			syslog(LOG_DEBUG, "Returning: %d , %d : NO-USER",	
			    lport, fport);
		n = snprintf(buf, sizeof(buf), "%d , %d : ERROR : %s\r\n",
		    lport, fport, unknown_flag ? "UNKNOWN-ERROR" : "NO-USER");
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost6(faddr));
			return 1;
		}
		return 0;
	}
	if (debug_flag && syslog_flag)
		syslog(LOG_DEBUG, "  After k_getuid6(), before getpwuid()");

	pw = getpwuid(uid);
	if (!pw) {
		if (syslog_flag)
			syslog(LOG_WARNING,
			    "getpwuid() could not map uid (%d) to name",
			    uid);
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : USERID : OTHER%s%s :%d\r\n",
		    lport, fport, charset_name ? " , " : "",
		    charset_name ? charset_name : "", uid);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost6(faddr));
			return 1;
		}
		return 0;
	}

	if (syslog_flag)
		syslog(LOG_DEBUG, "Successful lookup: %d , %d : %s",
		    lport, fport, pw->pw_name);

	if (noident_flag && check_noident(pw->pw_dir)) {
		if (syslog_flag && verbose_flag)
			syslog(LOG_NOTICE,
			    "user %s requested HIDDEN-USER for host %s: %d, %d",
			    pw->pw_name, gethost6(faddr), lport, fport);
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : ERROR : HIDDEN-USER\r\n", lport, fport);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost6(faddr));
			return 1;
		}
		return 0;
	}

	if (token_flag) {
		char token[21];

		gentoken(token, sizeof token);
		syslog(LOG_NOTICE, "token %s == uid %u (%s)", token, uid,
		    pw->pw_name);
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : USERID : OTHER%s%s :%s\r\n",
		    lport, fport, charset_name ? " , " : "",
		    charset_name ? charset_name : "", token);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost6(faddr));
			return 1;
		}
		return 0;
	}
	if (number_flag) {
		n = snprintf(buf, sizeof(buf),
		    "%d , %d : USERID : OTHER%s%s :%d\r\n",
		    lport, fport, charset_name ? " , " : "",
		    charset_name ? charset_name : "", uid);
		if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
			syslog(LOG_NOTICE, "write to %s: %m", gethost6(faddr));
			return 1;
		}
		return 0;
	}
	n = snprintf(buf, sizeof(buf), "%d , %d : USERID : %s%s%s :%s\r\n",
	    lport, fport, other_flag ? "OTHER" : "UNIX",
	    charset_name ? " , " : "",
	    charset_name ? charset_name : "", pw->pw_name);
	if (timed_write(fd, buf, n, IO_TIMEOUT) != n && syslog_flag) {
		syslog(LOG_NOTICE, "write to %s: %m", gethost6(faddr));
		return 1;
	}
	return 0;
}
