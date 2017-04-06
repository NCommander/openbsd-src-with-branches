/*	$OpenBSD: tty.h,v 1.5 2004/12/20 11:34:26 otto Exp $	*/

/*
	tty.h -- centralized definitions for a variety of terminal interfaces

	created by DPK, Oct. 1986

	Rearranged to work with autoconf, added TTY_state, get_tty/set_tty
						Michael Rendell, May '94

	last edit:	30-Jul-1987	D A Gwyn
*/

#include <termios.h>

extern int		tty_fd;		/* dup'd tty file descriptor */
extern int		tty_devtty;	/* true if tty_fd is from /dev/tty */
extern struct termios	tty_state;	/* saved tty state */

extern void	tty_init(int);
extern void	tty_close(void);
