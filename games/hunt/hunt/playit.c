/*	$OpenBSD: playit.c,v 1.2 1999/01/21 05:47:39 d Exp $	*/
/*	$NetBSD: playit.c,v 1.4 1997/10/20 00:37:15 lukem Exp $	*/
/*
 *  Hunt
 *  Copyright (c) 1985 Conrad C. Huang, Gregory S. Couch, Kenneth C.R.C. Arnold
 *  San Francisco, California
 */

#include <sys/file.h>
#include <err.h>
#include <errno.h>
#include <ctype.h>
#include <termios.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "hunt.h"
#include "display.h"
#include "client.h"

static int	nchar_send;
static FLAG	Last_player;

# define	MAX_SEND	5
# define	STDIN		0

/*
 * ibuf is the input buffer used for the stream from the driver.
 * It is small because we do not check for user input when there
 * are characters in the input buffer.
 */
static int		icnt = 0;
static unsigned char	ibuf[256], *iptr = ibuf;

#define	GETCHR()	(--icnt < 0 ? getchr() : *iptr++)

static	unsigned char	getchr __P((void));
static	void		send_stuff __P((void));

/*
 * playit:
 *	Play a given game, handling all the curses commands from
 *	the driver.
 */
void
playit()
{
	int		ch;
	int		y, x;
	u_int32_t	version;
	int		otto_y, otto_x;
	char		otto_face;

	if (read(Socket, &version, sizeof version) != sizeof version) {
		bad_con();
		/* NOTREACHED */
	}
	if (ntohl(version) != HUNT_VERSION) {
		bad_ver();
		/* NOTREACHED */
	}
	errno = 0;
	Otto_count = 0;
	nchar_send = MAX_SEND;
	while ((ch = GETCHR()) != EOF) {
		switch (ch & 0377) {
		  case MOVE:
			y = GETCHR();
			x = GETCHR();
			display_move(y, x);
			break;
		  case ADDCH:
			ch = GETCHR();
			if (Otto_mode)
				switch (ch) {

				case '<':
				case '>':
				case '^':
				case 'v':
					otto_face = ch;
					display_getyx(&otto_y, &otto_x);
					break;
				}

			display_put_ch(ch);
			break;
		  case CLRTOEOL:
			display_clear_eol();
			break;
		  case CLEAR:
			display_clear_the_screen();
			break;
		  case REFRESH:
			display_refresh();
			break;
		  case REDRAW:
			display_redraw_screen();
			display_refresh();
			break;
		  case ENDWIN:
			display_refresh();
			if ((ch = GETCHR()) == LAST_PLAYER)
				Last_player = TRUE;
			ch = EOF;
			goto out;
		  case BELL:
			display_beep();
			break;
		  case READY:
			display_refresh();
			if (nchar_send < 0)
				tcflush(STDIN, TCIFLUSH);
			nchar_send = MAX_SEND;
			Otto_count -= (GETCHR() & 0xff);
			if (!Am_monitor) {
				if (Otto_count == 0 && Otto_mode)
					otto(otto_y, otto_x, otto_face);
			}
			break;
		  default:
			if (Otto_mode)
				switch (ch) {

				case '<':
				case '>':
				case '^':
				case 'v':
					otto_face = ch;
					display_getyx(&otto_y, &otto_x);
					break;
				}
			display_put_ch(ch);
			break;
		}
	}
out:
	(void) close(Socket);
}

/*
 * getchr:
 *	Grab input and pass it along to the driver
 *	Return any characters from the driver
 *	When this routine is called by GETCHR, we already know there are
 *	no characters in the input buffer.
 */
static unsigned char
getchr()
{
	fd_set	readfds, s_readfds;
	int	nfds, s_nfds;

	FD_ZERO(&s_readfds);
	FD_SET(Socket, &s_readfds);
	FD_SET(STDIN, &s_readfds);
	s_nfds = (Socket > STDIN) ? Socket : STDIN;
	s_nfds++;

one_more_time:
	do {
		errno = 0;
		readfds = s_readfds;
		nfds = s_nfds;
		nfds = select(nfds, &readfds, NULL, NULL, NULL);
	} while (nfds <= 0 && errno == EINTR);

	if (FD_ISSET(STDIN, &readfds))
		send_stuff();
	if (! FD_ISSET(Socket, &readfds))
		goto one_more_time;
	icnt = read(Socket, ibuf, sizeof ibuf);
	if (icnt < 0) {
		bad_con();
		/* NOTREACHED */
	}
	if (icnt == 0)
		goto one_more_time;
	iptr = ibuf;
	icnt--;
	return *iptr++;
}

/*
 * send_stuff:
 *	Send standard input characters to the driver
 */
static void
send_stuff()
{
	int		count;
	char		*sp, *nsp;
	static char	inp[BUFSIZ];
	static char	Buf[BUFSIZ];

	/* Drain the user's keystrokes: */
	count = read(STDIN, Buf, sizeof Buf);
	if (count <= 0)
		return;

	if (nchar_send <= 0 && !no_beep) {
		(void) write(1, "\7", 1);	/* CTRL('G') */
		return;
	}

	/*
	 * look for 'q'uit commands; if we find one,
	 * confirm it.  If it is not confirmed, strip
	 * it out of the input
	 */
	Buf[count] = '\0';
	for (sp = Buf, nsp = inp; *sp != '\0'; sp++, nsp++) {
		*nsp = map_key[(int)*sp];
		if (*nsp == 'q')
			intr(0);
	}
	count = nsp - inp;
	if (count) {
		if (Otto_mode)
			Otto_count += count;
		nchar_send -= count;
		if (nchar_send < 0)
			count += nchar_send;
		(void) write(Socket, inp, count);
	}
}

/*
 * quit:
 *	Handle the end of the game when the player dies
 */
int
quit(old_status)
	int	old_status;
{
	int	explain, ch;

	if (Last_player)
		return Q_QUIT;
	if (Otto_mode)
		return Q_CLOAK;
	display_move(HEIGHT, 0);
	display_put_str("Re-enter game [ynwo]? ");
	display_clear_eol();
	explain = FALSE;
	for (;;) {
		display_refresh();
		if (isupper(ch = getchar()))
			ch = tolower(ch);
		if (ch == 'y')
			return old_status;
		else if (ch == 'o')
			break;
		else if (ch == 'n') {
			display_move(HEIGHT, 0);
			display_put_str("Write a parting message [yn]? ");
			display_clear_eol();
			display_refresh();
			for (;;) {
				if (isupper(ch = getchar()))
					ch = tolower(ch);
				if (ch == 'y')
					goto get_message;
				if (ch == 'n')
					return Q_QUIT;
			}
		}
		else if (ch == 'w') {
			static	char	buf[WIDTH + WIDTH % 2];
			char		*cp, c;

get_message:
			c = ch;		/* save how we got here */
			display_move(HEIGHT, 0);
			display_put_str("Message: ");
			display_clear_eol();
			display_refresh();
			cp = buf;
			for (;;) {
				display_refresh();
				if ((ch = getchar()) == '\n' || ch == '\r')
					break;
				if (display_iserasechar(ch))
				{
					if (cp > buf) {
						int y, x;

						display_getyx(&y, &x);
						display_move(y, x - 1);
						cp -= 1;
						display_clear_eol();
					}
					continue;
				}
				else if (display_iskillchar(ch))
				{
					int y, x;

					display_getyx(&y, &x);
					display_move(y, x - (cp - buf));
					cp = buf;
					display_clear_eol();
					continue;
				} else if (!isprint(ch)) {
					display_beep();
					continue;
				}
				display_put_ch(ch);
				*cp++ = ch;
				if (cp + 1 >= buf + sizeof buf)
					break;
			}
			*cp = '\0';
			Send_message = buf;
			return (c == 'w') ? old_status : Q_MESSAGE;
		}
		display_beep();
		if (!explain) {
			display_put_str("(Yes, No, Write message, or Options) ");
			explain = TRUE;
		}
	}

	display_move(HEIGHT, 0);
	display_put_str("Scan, Cloak, Flying, or Quit? ");
	display_clear_eol();
	display_refresh();
	explain = FALSE;
	for (;;) {
		if (isupper(ch = getchar()))
			ch = tolower(ch);
		if (ch == 's')
			return Q_SCAN;
		else if (ch == 'c')
			return Q_CLOAK;
		else if (ch == 'f')
			return Q_FLY;
		else if (ch == 'q')
			return Q_QUIT;
		display_beep();
		if (!explain) {
			display_put_str("[SCFQ] ");
			explain = TRUE;
		}
		display_refresh();
	}
}

/*
 * do_message:
 *	Send a message to the driver and return
 */
void
do_message()
{
	u_int32_t	version;

	if (read(Socket, &version, sizeof version) != sizeof version) {
		bad_con();
		/* NOTREACHED */
	}
	if (ntohl(version) != HUNT_VERSION) {
		bad_ver();
		/* NOTREACHED */
	}
	if (write(Socket, Send_message, strlen(Send_message)) < 0) {
		bad_con();
		/* NOTREACHED */
	}
	(void) close(Socket);
}
