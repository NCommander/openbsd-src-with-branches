/*	$NetBSD: util.c,v 1.4 1997/02/01 11:26:34 lukem Exp $	*/

/*
 * Copyright (c) 1985, 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
static char rcsid[] = "$NetBSD: util.c,v 1.4 1997/02/01 11:26:34 lukem Exp $";
#endif /* not lint */

/*
 * FTP User Program -- Misc support routines
 */
#include <sys/ioctl.h>
#include <sys/time.h>
#include <arpa/ftp.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ftp_var.h"
#include "pathnames.h"

/*
 * Connect to peer server and
 * auto-login, if possible.
 */
void
setpeer(argc, argv)
	int argc;
	char *argv[];
{
	char *host;
	short port;

	if (connected) {
		printf("Already connected to %s, use close first.\n",
			hostname);
		code = -1;
		return;
	}
	if (argc < 2)
		(void) another(&argc, &argv, "to");
	if (argc < 2 || argc > 3) {
		printf("usage: %s host-name [port]\n", argv[0]);
		code = -1;
		return;
	}
	port = ftpport;
	if (argc > 2) {
		port = atoi(argv[2]);
		if (port <= 0) {
			printf("%s: bad port number-- %s\n", argv[1], argv[2]);
			printf ("usage: %s host-name [port]\n", argv[0]);
			code = -1;
			return;
		}
		port = htons(port);
	}
	host = hookup(argv[1], port);
	if (host) {
		int overbose;

		connected = 1;
		/*
		 * Set up defaults for FTP.
		 */
		(void) strcpy(typename, "ascii"), type = TYPE_A;
		curtype = TYPE_A;
		(void) strcpy(formname, "non-print"), form = FORM_N;
		(void) strcpy(modename, "stream"), mode = MODE_S;
		(void) strcpy(structname, "file"), stru = STRU_F;
		(void) strcpy(bytename, "8"), bytesize = 8;
		if (autologin)
			(void) login(argv[1]);

		overbose = verbose;
		if (debug == 0)
			verbose = -1;
		if (command("SYST") == COMPLETE && overbose) {
			char *cp, c;
			c = 0;
			cp = strchr(reply_string+4, ' ');
			if (cp == NULL)
				cp = strchr(reply_string+4, '\r');
			if (cp) {
				if (cp[-1] == '.')
					cp--;
				c = *cp;
				*cp = '\0';
			}

			printf("Remote system type is %s.\n",
				reply_string+4);
			if (cp)
				*cp = c;
		}
		if (!strncmp(reply_string, "215 UNIX Type: L8", 17)) {
			if (proxy)
				unix_proxy = 1;
			else
				unix_server = 1;
			/*
			 * Set type to 0 (not specified by user),
			 * meaning binary by default, but don't bother
			 * telling server.  We can use binary
			 * for text files unless changed by the user.
			 */
			type = 0;
			(void) strcpy(typename, "binary");
			if (overbose)
			    printf("Using %s mode to transfer files.\n",
				typename);
		} else {
			if (proxy)
				unix_proxy = 0;
			else
				unix_server = 0;
			if (overbose &&
			    !strncmp(reply_string, "215 TOPS20", 10))
				printf("Remember to set tenex mode when "
				    "transferring binary files from this "
				    "machine.\n");
		}
		verbose = overbose;
	}
}

/*
 * `Another' gets another argument, and stores the new argc and argv.
 * It reverts to the top level (via main.c's intr()) on EOF/error.
 *
 * Returns false if no new arguments have been added.
 */
int
another(pargc, pargv, prompt)
	int *pargc;
	char ***pargv;
	const char *prompt;
{
	int len = strlen(line), ret;

	if (len >= sizeof(line) - 3) {
		printf("sorry, arguments too long\n");
		intr();
	}
	printf("(%s) ", prompt);
	line[len++] = ' ';
	if (fgets(&line[len], sizeof(line) - len, stdin) == NULL)
		intr();
	len += strlen(&line[len]);
	if (len > 0 && line[len - 1] == '\n')
		line[len - 1] = '\0';
	makeargv();
	ret = margc > *pargc;
	*pargc = margc;
	*pargv = margv;
	return (ret);
}

char *
remglob(argv, doswitch)
        char *argv[];
        int doswitch;
{
        char temp[MAXPATHLEN];
        static char buf[MAXPATHLEN];
        static FILE *ftemp = NULL;
        static char **args;
        int oldverbose, oldhash, fd;
        char *cp, *mode;

        if (!mflag) {
                if (!doglob) {
                        args = NULL;
                }
                else {
                        if (ftemp) {
                                (void) fclose(ftemp);
                                ftemp = NULL;
                        }
                }
                return (NULL);
        }
        if (!doglob) {
                if (args == NULL)
                        args = argv;
                if ((cp = *++args) == NULL)
                        args = NULL;
                return (cp);
        }
        if (ftemp == NULL) {
                (void) snprintf(temp, sizeof(temp), "%s%s", _PATH_TMP, TMPFILE)
;
                fd = mkstemp(temp);
                if (fd < 0) {
                        warn("unable to create temporary file %s", temp);
                        return (NULL);
                }
                close(fd);
                oldverbose = verbose, verbose = 0;
                oldhash = hash, hash = 0;
                if (doswitch) {
                        pswitch(!proxy);
                }
                for (mode = "w"; *++argv != NULL; mode = "a")
                        recvrequest ("NLST", temp, *argv, mode, 0);
                if (doswitch) {
                        pswitch(!proxy);
                }
                verbose = oldverbose; hash = oldhash;
                ftemp = fopen(temp, "r");
                (void) unlink(temp);
                if (ftemp == NULL) {
                        printf("can't find list of remote files, oops\n");
                        return (NULL);
                }
        }
        if (fgets(buf, sizeof (buf), ftemp) == NULL) {
                (void) fclose(ftemp), ftemp = NULL;
                return (NULL);
        }
        if ((cp = strchr(buf, '\n')) != NULL)
                *cp = '\0';
        return (buf);
}

int
confirm(cmd, file)
	const char *cmd, *file;
{
	char line[BUFSIZ];

	if (!interactive || confirmrest)
		return (1);
	printf("%s %s? ", cmd, file);
	(void) fflush(stdout);
	if (fgets(line, sizeof(line), stdin) == NULL)
		return (0);
	switch (tolower(*line)) {
		case 'n':
			return (0);
		case 'p':
			interactive = 0;
			printf("Interactive mode: off\n");
			break;
		case 'a':
			confirmrest = 1;
			printf("Prompting off for duration of %s\n", cmd);
			break;
	}
	return (1);
}

/*
 * Glob a local file name specification with
 * the expectation of a single return value.
 * Can't control multiple values being expanded
 * from the expression, we return only the first.
 */
int
globulize(cpp)
	char **cpp;
{
	glob_t gl;
	int flags;

	if (!doglob)
		return (1);

	flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_QUOTE|GLOB_TILDE;
	memset(&gl, 0, sizeof(gl));
	if (glob(*cpp, flags, NULL, &gl) ||
	    gl.gl_pathc == 0) {
		warnx("%s: not found", *cpp);
		globfree(&gl);
		return (0);
	}
	*cpp = strdup(gl.gl_pathv[0]);	/* XXX - wasted memory */
	globfree(&gl);
	return (1);
}

/*
 * determine size of remote file
 */
off_t
remotesize(file, noisy)
	const char *file;
	int noisy;
{
	int overbose;
	off_t size;

	overbose = verbose;
	size = -1;
	if (debug == 0)
		verbose = -1;
	if (command("SIZE %s", file) == COMPLETE)
		sscanf(reply_string, "%*s %qd", &size);
	else if (noisy && debug == 0)
		printf("%s\n", reply_string);
	verbose = overbose;
	return (size);
}

/*
 * determine last modification time (in GMT) of remote file
 */
time_t
remotemodtime(file, noisy)
	const char *file;
	int noisy;
{
	int overbose;
	time_t rtime;

	overbose = verbose;
	rtime = -1;
	if (debug == 0)
		verbose = -1;
	if (command("MDTM %s", file) == COMPLETE) {
		struct tm timebuf;
		int yy, mo, day, hour, min, sec;
		sscanf(reply_string, "%*s %04d%02d%02d%02d%02d%02d", &yy, &mo,
			&day, &hour, &min, &sec);
		memset(&timebuf, 0, sizeof(timebuf));
		timebuf.tm_sec = sec;
		timebuf.tm_min = min;
		timebuf.tm_hour = hour;
		timebuf.tm_mday = day;
		timebuf.tm_mon = mo - 1;
		timebuf.tm_year = yy - 1900;
		timebuf.tm_isdst = -1;
		rtime = mktime(&timebuf);
		if (rtime == -1 && (noisy || debug != 0))
			printf("Can't convert %s to a time\n", reply_string);
		else
			rtime += timebuf.tm_gmtoff;	/* conv. local -> GMT */
	} else if (noisy && debug == 0)
		printf("%s\n", reply_string);
	verbose = overbose;
	return (rtime);
}

void
updateprogressmeter()
{

	progressmeter(0);
}

/*
 * Display a transfer progress bar if progress is non-zero.
 * SIGALRM is hijacked for use by this function.
 * - Before the transfer, set filesize to size of file (or -1 if unknown),
 *   and call with flag = -1. This starts the once per second timer,
 *   and a call to updateprogressmeter() upon SIGALRM.
 * - During the transfer, updateprogressmeter will call progressmeter
 *   with flag = 0
 * - After the transfer, call with flag = 1
 */
static struct timeval start;

void
progressmeter(flag)
	int flag;
{
	/*
	 * List of order of magnitude prefixes.
	 * The last is `P', as 2^64 = 16384 Petabytes
	 */
	static const char prefixes[] = " KMGTP";

	static struct timeval lastupdate;
	static off_t lastsize;
	struct timeval now, td, wait;
	off_t cursize, abbrevsize;
	double elapsed;
	int ratio, barlength, i, remaining;
	char buf[256];

	if (flag == -1) {
		(void) gettimeofday(&start, (struct timezone *)0);
		lastupdate = start;
		lastsize = restart_point;
	}
	(void) gettimeofday(&now, (struct timezone *)0);
	if (!progress || filesize <= 0)
		return;
	cursize = bytes + restart_point;

	ratio = cursize * 100 / filesize;
	ratio = MAX(ratio, 0);
	ratio = MIN(ratio, 100);
	snprintf(buf, sizeof(buf), "\r%3d%% ", ratio);

	barlength = ttywidth - 30;
	if (barlength > 0) {
		i = barlength * ratio / 100;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "|%.*s%*s|", i, 
"*****************************************************************************"
"*****************************************************************************",
		    barlength - i, "");
	}

	i = 0;
	abbrevsize = cursize;
	while (abbrevsize >= 100000 && i < sizeof(prefixes)) {
		i++;
		abbrevsize >>= 10;
	}
	snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
	    " %5qd %c%c ", abbrevsize, prefixes[i],
	    prefixes[i] == ' ' ? ' ' : 'B');

	timersub(&now, &lastupdate, &wait);
	if (cursize > lastsize) {
		lastupdate = now;
		lastsize = cursize;
		if (wait.tv_sec >= STALLTIME) {	/* fudge out stalled time */
			start.tv_sec += wait.tv_sec;
			start.tv_usec += wait.tv_usec;
		}
		wait.tv_sec = 0;
	}

	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);

	if (bytes <= 0 || elapsed <= 0.0) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "   --:-- ETA");
	} else if (wait.tv_sec >= STALLTIME) {
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    " - stalled -");
	} else {
		remaining = (int)((filesize - restart_point) /
				  (bytes / elapsed) - elapsed);
		i = remaining / 3600;
		if (i)
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "%2d:", i);
		else
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
			    "   ");
		i = remaining % 3600;
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
		    "%02d:%02d ETA", i / 60, i % 60);
	}
	(void)write(STDOUT_FILENO, buf, strlen(buf));

	if (flag == -1) {
		(void) signal(SIGALRM, updateprogressmeter);
		alarmtimer(1);		/* set alarm timer for 1 Hz */
	} else if (flag == 1) {
		alarmtimer(0);
		(void) putchar('\n');
	}
	fflush(stdout);
}

/*
 * Display transfer statistics.
 * Requires start to be initialised by progressmeter(-1),
 * direction to be defined by xfer routines, and filesize and bytes
 * to be updated by xfer routines
 * If siginfo is nonzero, an ETA is displayed, and the output goes to STDERR
 * instead of STDOUT.
 */
void
ptransfer(siginfo)
	int siginfo;
{
	struct timeval now, td;
	double elapsed;
	off_t bs;
	int meg, remaining, hh;
	char buf[100];

	if (!verbose && !siginfo)
		return;

	(void) gettimeofday(&now, (struct timezone *)0);
	timersub(&now, &start, &td);
	elapsed = td.tv_sec + (td.tv_usec / 1000000.0);
	bs = bytes / (elapsed == 0.0 ? 1 : elapsed);
	meg = 0;
	if (bs > (1024 * 1024))
		meg = 1;
	(void)snprintf(buf, sizeof(buf),
	    "%qd byte%s %s in %.2f seconds (%.2f %sB/s)\n",
	    bytes, bytes == 1 ? "" : "s", direction, elapsed,
	    bs / (1024.0 * (meg ? 1024.0 : 1.0)), meg ? "M" : "K");
	if (siginfo && bytes > 0 && elapsed > 0.0 && filesize >= 0) {
		remaining = (int)((filesize - restart_point) /
				  (bytes / elapsed) - elapsed);
		hh = remaining / 3600;
		remaining %= 3600;
		snprintf(buf + strlen(buf) - 1, sizeof(buf) - strlen(buf),
		    "  ETA: %02d:%02d:%02d\n", hh, remaining / 60,
		    remaining % 60);
	}
	(void)write(siginfo ? STDERR_FILENO : STDOUT_FILENO, buf, strlen(buf));
}

/*
 * List words in stringlist, vertically arranged
 */
void
list_vertical(sl)
	StringList *sl;
{
	int i, j, w;
	int columns, width, lines, items;
	char *p;

	width = items = 0;

	for (i = 0 ; i < sl->sl_cur ; i++) {
		w = strlen(sl->sl_str[i]);
		if (w > width)
			width = w;
	}
	width = (width + 8) &~ 7;

	columns = ttywidth / width;
	if (columns == 0)
		columns = 1;
	lines = (sl->sl_cur + columns - 1) / columns;
	for (i = 0; i < lines; i++) {
		for (j = 0; j < columns; j++) {
			p = sl->sl_str[j * lines + i];
			if (p)
				printf("%s", p);
			if (j * lines + i + lines >= sl->sl_cur) {
				printf("\n");
				break;
			}
			w = strlen(p);
			while (w < width) {
				w = (w + 8) &~ 7;
				(void) putchar('\t');
			}
		}
	}
}

/*
 * Update the global ttywidth value, using TIOCGWINSZ.
 */
void
setttywidth(a)
	int a;
{
	struct winsize winsize;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &winsize) != -1)
		ttywidth = winsize.ws_col;
	else
		ttywidth = 80;
}

/*
 * Set the SIGALRM interval timer for wait seconds, 0 to disable.
 */

void
alarmtimer(wait)
	int wait;
{
	struct itimerval itv;

	itv.it_value.tv_sec = wait;
	itv.it_value.tv_usec = 0;
	itv.it_interval = itv.it_value;
	setitimer(ITIMER_REAL, &itv, NULL);
}
