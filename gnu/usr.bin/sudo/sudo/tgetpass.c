/*
 *  CU sudo version 1.5.3
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *******************************************************************
 *
 *  This module contains tgetpass(), getpass(3) with a timeout.
 *  It should work on any OS that supports sgtty (4BSD), termio (SYSV),
 *  or termios (POSIX) line disciplines.
 *
 *  Todd C. Miller  Sun Jun  5 17:22:31 MDT 1994
 */

#ifndef lint
static char rcsid[] = "$Id: tgetpass.c,v 1.50 1996/11/14 02:37:16 millert Exp $";
#endif /* lint */

#include "config.h"

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#include <limits.h>
#include <pwd.h>
#include <sys/types.h>
#ifdef HAVE_SYS_BSDTYPES_H
#include <sys/bsdtypes.h>
#endif /* HAVE_SYS_BSDTYPES_H */
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#ifdef HAVE_TERMIO_H
#include <termio.h>
#else
#include <sgtty.h>
#include <sys/ioctl.h>
#endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */
#if (SHADOW_TYPE == SPW_SECUREWARE)
#  ifdef __hpux
#    include <hpsecurity.h>
#  else
#    include <sys/security.h>
#  endif /* __hpux */ 
#  include <prot.h>
#endif /* SPW_SECUREWARE */

#include <pathnames.h>
#include "compat.h"

#ifndef TCSASOFT
#define TCSASOFT	0
#endif /* TCSASOFT */


/******************************************************************
 *
 *  tgetpass()
 *
 *  this function prints a prompt and gets a password from /dev/tty
 *  or stdin.  Echo is turned off (if possible) during password entry
 *  and input will time out based on the value of timeout.
 */

char * tgetpass(prompt, timeout, user, host)
    const char *prompt;
    int timeout;
    char *user;
    char *host;
{
#ifdef HAVE_TERMIOS_H
    struct termios term;
#else
#ifdef HAVE_TERMIO_H
    struct termio term;
#else
    struct sgttyb ttyb;
#endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */
#ifdef POSIX_SIGNALS
    sigset_t oldmask;
    sigset_t mask;
#else
    int oldmask;
#endif /* POSIX_SIGNALS */
    int n, echo;
    FILE *input, *output;
    static char buf[_PASSWD_LEN + 1];
    fd_set readfds;
    struct timeval tv;
    char *p;

    /*
     * mask out SIGINT and SIGTSTP, should probably just catch and deal.
     */
#ifdef POSIX_SIGNALS
    (void) sigemptyset(&mask);
    (void) sigaddset(&mask, SIGINT);
    (void) sigaddset(&mask, SIGTSTP);
    (void) sigprocmask(SIG_BLOCK, &mask, &oldmask);
#else
    oldmask = sigblock(sigmask(SIGINT)|sigmask(SIGTSTP));
#endif

    /*
     * open /dev/tty for reading/writing if possible or use
     * stdin and stderr instead.
     */
    if ((input = fopen(_PATH_TTY, "r+")) == NULL) {
	input = stdin;
	output = stderr;
	(void) fflush(output);
    } else {
	output = input;
    }

    /*
     * turn off echo
     */
#ifdef HAVE_TERMIOS_H
    (void) tcgetattr(fileno(input), &term);
    if ((echo = (term.c_lflag & ECHO))) {
	term.c_lflag &= ~ECHO;
	(void) tcsetattr(fileno(input), TCSAFLUSH|TCSASOFT, &term);
    }
#else
#ifdef HAVE_TERMIO_H
    (void) ioctl(fileno(input), TCGETA, &term);
    if ((echo = (term.c_lflag & ECHO))) {
	term.c_lflag &= ~ECHO;
	(void) ioctl(fileno(input), TCSETA, &term);
    }
#else
    (void) ioctl(fileno(input), TIOCGETP, &ttyb);
    if ((echo = (ttyb.sg_flags & ECHO))) {
	ttyb.sg_flags &= ~ECHO;
	(void) ioctl(fileno(input), TIOCSETP, &ttyb);
    }
#endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

    /* print the prompt */
    if (prompt) {
	p = (char *) prompt;
	do {
	    /* expand %u -> username, %h -> host */
	    switch (*p) {
		case '%':   if (user && *(p+1) == 'u') {
				(void) fputs(user, output);
				p++;
				break;
			    } else if (host && *(p+1) == 'h') {
				(void) fputs(host, output);
				p++;
				break;
			    }

		default:    (void) fputc(*p, output);
	    }
	} while (*(++p));
    }

    /* rewind if necesary */
    if (input == output) {
	(void) fflush(output);
	(void) rewind(output);
    }

    /*
     * Timeout of <= 0 means no timeout
     */
    if (timeout > 0) {
	/* setup for select(2) */
	FD_ZERO(&readfds);
	FD_SET(fileno(input), &readfds);

	/* set timeout for select */
	tv.tv_sec = timeout;
	tv.tv_usec = 0;

	/* how many file descriptors may we have? */
#ifdef HAVE_SYSCONF
	n = sysconf(_SC_OPEN_MAX);
#else
	n = getdtablesize();
#endif /* HAVE_SYSCONF */

	/*
	 * get password or return empty string if nothing to read by timeout
	 */
	buf[0] = '\0';
	if (select(n, &readfds, 0, 0, &tv) > 0 && fgets(buf, sizeof(buf), input)) {
	    n = strlen(buf);
	    if (buf[n - 1] == '\n')
		buf[n - 1] = '\0';
	}
    } else {
	buf[0] = '\0';
	if (fgets(buf, sizeof(buf), input)) {
	    n = strlen(buf);
	    if (buf[n - 1] == '\n')
		buf[n - 1] = '\0';
	}
    }

     /* turn on echo */
#ifdef HAVE_TERMIOS_H
    if (echo) {
	term.c_lflag |= ECHO;
	(void) tcsetattr(fileno(input), TCSAFLUSH|TCSASOFT, &term);
    }
#else
#ifdef HAVE_TERMIO_H
    if (echo) {
	term.c_lflag |= ECHO;
	(void) ioctl(fileno(input), TCSETA, &term);
    }
#else
    if (echo) {
	ttyb.sg_flags |= ECHO;
	(void) ioctl(fileno(input), TIOCSETP, &ttyb);
    }
#endif /* HAVE_TERMIO_H */
#endif /* HAVE_TERMIOS_H */

    /* rewind if necesary */
    if (input == output) {
	(void) fflush(output);
	(void) rewind(output);
    }

    /* print a newline since echo is turned off */
    (void) fputc('\n', output);

    /* restore old signal mask */
#ifdef POSIX_SIGNALS
    (void) sigprocmask(SIG_SETMASK, &oldmask, NULL);
#else
    (void) sigsetmask(oldmask);
#endif

    /* close /dev/tty if that's what we opened */
    if (input != stdin)
	(void) fclose(input);

    return(buf);
}
