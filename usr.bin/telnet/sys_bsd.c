/*	$OpenBSD: sys_bsd.c,v 1.17 2014/07/20 05:22:02 guenther Exp $	*/
/*	$NetBSD: sys_bsd.c,v 1.11 1996/02/28 21:04:10 thorpej Exp $	*/

/*
 * Copyright (c) 1988, 1990, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include "telnet_locl.h"

#include <sys/ioctl.h>
#include <poll.h>
#include <unistd.h>

/*
 * The following routines try to encapsulate what is system dependent
 * (at least between 4.x and dos) which is used in telnet.c.
 */

int
	tout,			/* Output file descriptor */
	tin,			/* Input file descriptor */
	net;

#define TELNET_FD_TOUT	0
#define TELNET_FD_TIN	1
#define TELNET_FD_NET	2
#define TELNET_FD_NUM	3

struct	termios old_tc = { 0 };
extern struct termios new_tc;

    void
init_sys()
{
    tout = fileno(stdout);
    tin = fileno(stdin);

    errno = 0;
}


    int
TerminalWrite(buf, n)
    char *buf;
    int  n;
{
    return write(tout, buf, n);
}

    int
TerminalRead(buf, n)
    unsigned char *buf;
    int  n;
{
    return read(tin, buf, n);
}

/*
 *
 */

    int
TerminalAutoFlush()
{
#if	defined(LNOFLSH)
    int flush;

    ioctl(0, TIOCLGET, (char *)&flush);
    return !(flush&LNOFLSH);	/* if LNOFLSH, no autoflush */
#else	/* LNOFLSH */
    return 1;
#endif	/* LNOFLSH */
}

#ifdef	KLUDGELINEMODE
extern int kludgelinemode;
#endif
/*
 * TerminalSpecialChars()
 *
 * Look at an input character to see if it is a special character
 * and decide what to do.
 *
 * Output:
 *
 *	0	Don't add this character.
 *	1	Do add this character
 */

    int
TerminalSpecialChars(c)
    int	c;
{
    if (c == termIntChar) {
	intp();
	return 0;
    } else if (c == termQuitChar) {
#ifdef	KLUDGELINEMODE
	if (kludgelinemode)
	    sendbrk();
	else
#endif
	    sendabort();
	return 0;
    } else if (c == termEofChar) {
	if (my_want_state_is_will(TELOPT_LINEMODE)) {
	    sendeof();
	    return 0;
	}
	return 1;
    } else if (c == termSuspChar) {
	sendsusp();
	return(0);
    } else if (c == termFlushChar) {
	xmitAO();		/* Transmit Abort Output */
	return 0;
    } else if (!MODE_LOCAL_CHARS(globalmode)) {
	if (c == termKillChar) {
	    xmitEL();
	    return 0;
	} else if (c == termEraseChar) {
	    xmitEC();		/* Transmit Erase Character */
	    return 0;
	}
    }
    return 1;
}


/*
 * Flush output to the terminal
 */

    void
TerminalFlushOutput()
{
#ifdef	TIOCFLUSH
    int com = FWRITE;
    (void) ioctl(fileno(stdout), TIOCFLUSH, (int *) &com);
#else
    (void) ioctl(fileno(stdout), TCFLSH, (int *) 0);
#endif
}

    void
TerminalSaveState()
{
    tcgetattr(0, &old_tc);

    new_tc = old_tc;

#ifndef	VDISCARD
    termFlushChar = CONTROL('O');
#endif
#ifndef	VWERASE
    termWerasChar = CONTROL('W');
#endif
#ifndef	VREPRINT
    termRprntChar = CONTROL('R');
#endif
#ifndef	VLNEXT
    termLiteralNextChar = CONTROL('V');
#endif
#ifndef	VSTART
    termStartChar = CONTROL('Q');
#endif
#ifndef	VSTOP
    termStopChar = CONTROL('S');
#endif
#ifndef	VSTATUS
    termAytChar = CONTROL('T');
#endif
}

    cc_t *
tcval(func)
    int func;
{
    switch(func) {
    case SLC_IP:	return(&termIntChar);
    case SLC_ABORT:	return(&termQuitChar);
    case SLC_EOF:	return(&termEofChar);
    case SLC_EC:	return(&termEraseChar);
    case SLC_EL:	return(&termKillChar);
    case SLC_XON:	return(&termStartChar);
    case SLC_XOFF:	return(&termStopChar);
    case SLC_FORW1:	return(&termForw1Char);
    case SLC_FORW2:	return(&termForw2Char);
# ifdef	VDISCARD
    case SLC_AO:	return(&termFlushChar);
# endif
# ifdef	VSUSP
    case SLC_SUSP:	return(&termSuspChar);
# endif
# ifdef	VWERASE
    case SLC_EW:	return(&termWerasChar);
# endif
# ifdef	VREPRINT
    case SLC_RP:	return(&termRprntChar);
# endif
# ifdef	VLNEXT
    case SLC_LNEXT:	return(&termLiteralNextChar);
# endif
# ifdef	VSTATUS
    case SLC_AYT:	return(&termAytChar);
# endif

    case SLC_SYNCH:
    case SLC_BRK:
    case SLC_EOR:
    default:
	return((cc_t *)0);
    }
}

    void
TerminalDefaultChars()
{
    memmove(new_tc.c_cc, old_tc.c_cc, sizeof(old_tc.c_cc));
# ifndef	VDISCARD
    termFlushChar = CONTROL('O');
# endif
# ifndef	VWERASE
    termWerasChar = CONTROL('W');
# endif
# ifndef	VREPRINT
    termRprntChar = CONTROL('R');
# endif
# ifndef	VLNEXT
    termLiteralNextChar = CONTROL('V');
# endif
# ifndef	VSTART
    termStartChar = CONTROL('Q');
# endif
# ifndef	VSTOP
    termStopChar = CONTROL('S');
# endif
# ifndef	VSTATUS
    termAytChar = CONTROL('T');
# endif
}

/*
 * TerminalNewMode - set up terminal to a specific mode.
 *	MODE_ECHO: do local terminal echo
 *	MODE_FLOW: do local flow control
 *	MODE_TRAPSIG: do local mapping to TELNET IAC sequences
 *	MODE_EDIT: do local line editing
 *
 *	Command mode:
 *		MODE_ECHO|MODE_EDIT|MODE_FLOW|MODE_TRAPSIG
 *		local echo
 *		local editing
 *		local xon/xoff
 *		local signal mapping
 *
 *	Linemode:
 *		local/no editing
 *	Both Linemode and Single Character mode:
 *		local/remote echo
 *		local/no xon/xoff
 *		local/no signal mapping
 */

#ifdef SIGTSTP
static void susp();
#endif /* SIGTSTP */
#ifdef SIGINFO
static void ayt();
#endif

    void
TerminalNewMode(f)
    int f;
{
    static int prevmode = 0;
    struct termios tmp_tc;
    int onoff;
    int old;
    cc_t esc;

    globalmode = f&~MODE_FORCE;
    if (prevmode == f)
	return;

    /*
     * Write any outstanding data before switching modes
     * ttyflush() returns 0 only when there is no more data
     * left to write out, it returns -1 if it couldn't do
     * anything at all, otherwise it returns 1 + the number
     * of characters left to write.
     */
    old = ttyflush(SYNCHing|flushout);
    if (old < 0 || old > 1) {
	tcgetattr(tin, &tmp_tc);
	do {
	    /*
	     * Wait for data to drain, then flush again.
	     */
	    tcsetattr(tin, TCSADRAIN, &tmp_tc);
	    old = ttyflush(SYNCHing|flushout);
	} while (old < 0 || old > 1);
    }

    old = prevmode;
    prevmode = f&~MODE_FORCE;
    tmp_tc = new_tc;

    if (f&MODE_ECHO) {
	tmp_tc.c_lflag |= ECHO;
	tmp_tc.c_oflag |= ONLCR;
	if (crlf)
		tmp_tc.c_iflag |= ICRNL;
    } else {
	tmp_tc.c_lflag &= ~ECHO;
	tmp_tc.c_oflag &= ~ONLCR;
    }

    if ((f&MODE_FLOW) == 0) {
	tmp_tc.c_iflag &= ~(IXOFF|IXON);	/* Leave the IXANY bit alone */
    } else {
	if (restartany < 0) {
		tmp_tc.c_iflag |= IXOFF|IXON;	/* Leave the IXANY bit alone */
	} else if (restartany > 0) {
		tmp_tc.c_iflag |= IXOFF|IXON|IXANY;
	} else {
		tmp_tc.c_iflag |= IXOFF|IXON;
		tmp_tc.c_iflag &= ~IXANY;
	}
    }

    if ((f&MODE_TRAPSIG) == 0) {
	tmp_tc.c_lflag &= ~ISIG;
	localchars = 0;
    } else {
	tmp_tc.c_lflag |= ISIG;
	localchars = 1;
    }

    if (f&MODE_EDIT) {
	tmp_tc.c_lflag |= ICANON;
    } else {
	tmp_tc.c_lflag &= ~ICANON;
	tmp_tc.c_iflag &= ~ICRNL;
	tmp_tc.c_cc[VMIN] = 1;
	tmp_tc.c_cc[VTIME] = 0;
    }

    if ((f&(MODE_EDIT|MODE_TRAPSIG)) == 0) {
	tmp_tc.c_lflag &= ~IEXTEN;
    }

    if (f&MODE_SOFT_TAB) {
# ifdef	OXTABS
	tmp_tc.c_oflag |= OXTABS;
# endif
# ifdef	TABDLY
	tmp_tc.c_oflag &= ~TABDLY;
	tmp_tc.c_oflag |= TAB3;
# endif
    } else {
# ifdef	OXTABS
	tmp_tc.c_oflag &= ~OXTABS;
# endif
# ifdef	TABDLY
	tmp_tc.c_oflag &= ~TABDLY;
# endif
    }

    if (f&MODE_LIT_ECHO) {
# ifdef	ECHOCTL
	tmp_tc.c_lflag &= ~ECHOCTL;
# endif
    } else {
# ifdef	ECHOCTL
	tmp_tc.c_lflag |= ECHOCTL;
# endif
    }

    if (f == -1) {
	onoff = 0;
    } else {
	if (f & MODE_INBIN)
		tmp_tc.c_iflag &= ~ISTRIP;
	else
		tmp_tc.c_iflag |= ISTRIP;
	if ((f & MODE_OUTBIN) || (f & MODE_OUT8)) {
		tmp_tc.c_cflag &= ~(CSIZE|PARENB);
		tmp_tc.c_cflag |= CS8;
		if(f & MODE_OUTBIN)
			tmp_tc.c_oflag &= ~OPOST;
		else
			tmp_tc.c_oflag |= OPOST;

	} else {
		tmp_tc.c_cflag &= ~(CSIZE|PARENB);
		tmp_tc.c_cflag |= old_tc.c_cflag & (CSIZE|PARENB);
		tmp_tc.c_oflag |= OPOST;
	}
	onoff = 1;
    }

    if (f != -1) {
#ifdef	SIGTSTP
	(void) signal(SIGTSTP, susp);
#endif	/* SIGTSTP */
#ifdef	SIGINFO
	(void) signal(SIGINFO, ayt);
#endif
#if	defined(NOKERNINFO)
	tmp_tc.c_lflag |= NOKERNINFO;
#endif
	/*
	 * We don't want to process ^Y here.  It's just another
	 * character that we'll pass on to the back end.  It has
	 * to process it because it will be processed when the
	 * user attempts to read it, not when we send it.
	 */
# ifdef	VDSUSP
	tmp_tc.c_cc[VDSUSP] = (cc_t)(_POSIX_VDISABLE);
# endif
	/*
	 * If the VEOL character is already set, then use VEOL2,
	 * otherwise use VEOL.
	 */
	esc = (rlogin != _POSIX_VDISABLE) ? rlogin : escape;
	if ((tmp_tc.c_cc[VEOL] != esc)
# ifdef	VEOL2
	    && (tmp_tc.c_cc[VEOL2] != esc)
# endif
	    ) {
		if (tmp_tc.c_cc[VEOL] == (cc_t)(_POSIX_VDISABLE))
		    tmp_tc.c_cc[VEOL] = esc;
# ifdef	VEOL2
		else if (tmp_tc.c_cc[VEOL2] == (cc_t)(_POSIX_VDISABLE))
		    tmp_tc.c_cc[VEOL2] = esc;
# endif
	}
    } else {
#ifdef	SIGTSTP
	sigset_t mask;
#endif	/* SIGTSTP */
#ifdef	SIGINFO
	void ayt_status();

	(void) signal(SIGINFO, (void (*)(int))ayt_status);
#endif
#ifdef	SIGTSTP
	(void) signal(SIGTSTP, SIG_DFL);
	sigemptyset(&mask);
	sigaddset(&mask, SIGTSTP);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);
#endif	/* SIGTSTP */
	tmp_tc = old_tc;
    }
    if (tcsetattr(tin, TCSADRAIN, &tmp_tc) < 0)
	tcsetattr(tin, TCSANOW, &tmp_tc);

    ioctl(tin, FIONBIO, (char *)&onoff);
    ioctl(tout, FIONBIO, (char *)&onoff);
}

/*
 * Try to guess whether speeds are "encoded" (4.2BSD) or just numeric (4.4BSD).
 */
#if B4800 != 4800
#define	DECODE_BAUD
#endif

#ifdef	DECODE_BAUD
#ifndef	B7200
#define B7200   B4800
#endif

#ifndef	B14400
#define B14400  B9600
#endif

#ifndef	B19200
# define B19200 B14400
#endif

#ifndef	B28800
#define B28800  B19200
#endif

#ifndef	B38400
# define B38400 B28800
#endif

#ifndef B57600
#define B57600  B38400
#endif

#ifndef B76800
#define B76800  B57600
#endif

#ifndef B115200
#define B115200 B76800
#endif

#ifndef B230400
#define B230400 B115200
#endif


/*
 * This code assumes that the values B0, B50, B75...
 * are in ascending order.  They do not have to be
 * contiguous.
 */
struct termspeeds {
	long speed;
	long value;
} termspeeds[] = {
	{ 0,      B0 },      { 50,    B50 },    { 75,     B75 },
	{ 110,    B110 },    { 134,   B134 },   { 150,    B150 },
	{ 200,    B200 },    { 300,   B300 },   { 600,    B600 },
	{ 1200,   B1200 },   { 1800,  B1800 },  { 2400,   B2400 },
	{ 4800,   B4800 },   { 7200,  B7200 },  { 9600,   B9600 },
	{ 14400,  B14400 },  { 19200, B19200 }, { 28800,  B28800 },
	{ 38400,  B38400 },  { 57600, B57600 }, { 115200, B115200 },
	{ 230400, B230400 }, { -1,    B230400 }
};
#endif	/* DECODE_BAUD */

    void
TerminalSpeeds(ispeed, ospeed)
    long *ispeed;
    long *ospeed;
{
#ifdef	DECODE_BAUD
    struct termspeeds *tp;
#endif	/* DECODE_BAUD */
    long in, out;

    out = cfgetospeed(&old_tc);
    in = cfgetispeed(&old_tc);
    if (in == 0)
	in = out;

#ifdef	DECODE_BAUD
    tp = termspeeds;
    while ((tp->speed != -1) && (tp->value < in))
	tp++;
    *ispeed = tp->speed;

    tp = termspeeds;
    while ((tp->speed != -1) && (tp->value < out))
	tp++;
    *ospeed = tp->speed;
#else	/* DECODE_BAUD */
	*ispeed = in;
	*ospeed = out;
#endif	/* DECODE_BAUD */
}

    int
TerminalWindowSize(rows, cols)
    long *rows, *cols;
{
#ifdef	TIOCGWINSZ
    struct winsize ws;

    if (ioctl(fileno(stdin), TIOCGWINSZ, (char *)&ws) >= 0) {
	*rows = ws.ws_row;
	*cols = ws.ws_col;
	return 1;
    }
#endif	/* TIOCGWINSZ */
    return 0;
}

    int
NetClose(fd)
    int	fd;
{
    return close(fd);
}


    void
NetNonblockingIO(fd, onoff)
    int fd;
    int onoff;
{
    ioctl(fd, FIONBIO, (char *)&onoff);
}


/*
 * Various signal handling routines.
 */

    /* ARGSUSED */
    void
deadpeer(sig)
    int sig;
{
	setcommandmode();
	longjmp(peerdied, -1);
}

volatile sig_atomic_t intr_happened = 0;
volatile sig_atomic_t intr_waiting = 0;

    /* ARGSUSED */
    void
intr(sig)
    int sig;
{
    if (intr_waiting) {
	intr_happened = 1;
	return;
    }
    if (localchars) {
	intp();
	return;
    }
    setcommandmode();
    longjmp(toplevel, -1);
}

    /* ARGSUSED */
    void
intr2(sig)
    int sig;
{
    if (localchars) {
#ifdef	KLUDGELINEMODE
	if (kludgelinemode)
	    sendbrk();
	else
#endif
	    sendabort();
	return;
    }
}

#ifdef	SIGTSTP
    /* ARGSUSED */
    void
susp(sig)
    int sig;
{
    if ((rlogin != _POSIX_VDISABLE) && rlogin_susp())
	return;
    if (localchars)
	sendsusp();
}
#endif

#ifdef	SIGWINCH
    /* ARGSUSED */
    void
sendwin(sig)
    int sig;
{
    if (connected) {
	sendnaws();
    }
}
#endif

#ifdef	SIGINFO
    /* ARGSUSED */
    void
ayt(sig)
    int sig;
{
    if (connected)
	sendayt();
    else
	ayt_status();
}
#endif


    void
sys_telnet_init()
{
    int one = 1;

    (void) signal(SIGINT, intr);
    (void) signal(SIGQUIT, intr2);
    (void) signal(SIGPIPE, deadpeer);
#ifdef	SIGWINCH
    (void) signal(SIGWINCH, sendwin);
#endif
#ifdef	SIGTSTP
    (void) signal(SIGTSTP, susp);
#endif
#ifdef	SIGINFO
    (void) signal(SIGINFO, ayt);
#endif

    setconnmode(0);

    NetNonblockingIO(net, 1);

    if (setsockopt(net, SOL_SOCKET, SO_OOBINLINE, &one, sizeof(one)) == -1) {
	perror("setsockopt");
    }
}

/*
 * Process rings -
 *
 *	This routine tries to fill up/empty our various rings.
 *
 *	The parameter specifies whether this is a poll operation,
 *	or a block-until-something-happens operation.
 *
 *	The return value is 1 if something happened, 0 if not.
 */

    int
process_rings(netin, netout, netex, ttyin, ttyout, dopoll)
    int dopoll;		/* If 0, then block until something to do */
{
    int c;
		/* One wants to be a bit careful about setting returnValue
		 * to one, since a one implies we did some useful work,
		 * and therefore probably won't be called to block next
		 * time (TN3270 mode only).
		 */
    int returnValue = 0;
    struct pollfd pfd[TELNET_FD_NUM];

    if (ttyout) {
	pfd[TELNET_FD_TOUT].fd = tout;
	pfd[TELNET_FD_TOUT].events = POLLOUT;
    } else {
	pfd[TELNET_FD_TOUT].fd = -1;
    }
    if (ttyin) {
	pfd[TELNET_FD_TIN].fd = tin;
	pfd[TELNET_FD_TIN].events = POLLIN;
    } else {
	pfd[TELNET_FD_TIN].fd = -1;
    }
    if (netout || netin || netex) {
	pfd[TELNET_FD_NET].fd = net;
	pfd[TELNET_FD_NET].events = 0;
	if (netout)
	    pfd[TELNET_FD_NET].events |= POLLOUT;
	if (netin)
	    pfd[TELNET_FD_NET].events |= POLLIN;
	if (netex)
	    pfd[TELNET_FD_NET].events |= POLLRDBAND;
    } else {
	pfd[TELNET_FD_NET].fd = -1;
    }

    if ((c = poll(pfd, TELNET_FD_NUM, dopoll ? 0 : -1)) < 0) {
	if (c == -1) {
		    /*
		     * we can get EINTR if we are in line mode,
		     * and the user does an escape (TSTP), or
		     * some other signal generator.
		     */
	    if (errno == EINTR) {
		return 0;
	    }
		    /* I don't like this, does it ever happen? */
	    printf("sleep(5) from telnet, after poll\r\n");
	    sleep(5);
	}
	return 0;
    }

    /*
     * Any urgent data?
     */
    if (pfd[TELNET_FD_NET].revents & POLLRDBAND) {
	SYNCHing = 1;
	(void) ttyflush(1);	/* flush already enqueued data */
    }

    /*
     * Something to read from the network...
     */
    if (pfd[TELNET_FD_NET].revents & (POLLIN|POLLHUP)) {
	int canread;

	canread = ring_empty_consecutive(&netiring);
	c = recv(net, (char *)netiring.supply, canread, 0);
	if (c < 0 && errno == EWOULDBLOCK) {
	    c = 0;
	} else if (c <= 0) {
	    return -1;
	}
	if (netdata) {
	    Dump('<', netiring.supply, c);
	}
	if (c)
	    ring_supplied(&netiring, c);
	returnValue = 1;
    }

    /*
     * Something to read from the tty...
     */
    if (pfd[TELNET_FD_TIN].revents & (POLLIN|POLLHUP)) {
	c = TerminalRead(ttyiring.supply, ring_empty_consecutive(&ttyiring));
	if (c < 0 && errno == EIO)
	    c = 0;
	if (c < 0 && errno == EWOULDBLOCK) {
	    c = 0;
	} else {
	    /* EOF detection for line mode!!!! */
	    if ((c == 0) && MODE_LOCAL_CHARS(globalmode) && isatty(tin)) {
		/* must be an EOF... */
		*ttyiring.supply = termEofChar;
		c = 1;
	    }
	    if (c <= 0) {
		return -1;
	    }
	    if (termdata) {
		Dump('<', ttyiring.supply, c);
	    }
	    ring_supplied(&ttyiring, c);
	}
	returnValue = 1;		/* did something useful */
    }

    if (pfd[TELNET_FD_NET].revents & POLLOUT) {
	returnValue |= netflush();
    }
    if (pfd[TELNET_FD_TOUT].revents & POLLOUT) {
	returnValue |= (ttyflush(SYNCHing|flushout) > 0);
    }

    return returnValue;
}
