
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/


/*
**	lib_tstp.c
**
**	The routine _nc_signal_handler().
**
*/

#include "curses.priv.h"

#include <signal.h>
#include <stdlib.h>

#if HAVE_SIGACTION
#if !HAVE_TYPE_SIGACTION
typedef struct sigaction sigaction_t;
#endif
#else
#include "SigAction.h"
#endif

#ifdef SVR4_ACTION
#define _POSIX_SOURCE
#endif

/*
 * Note: This code is fragile!  Its problem is that different OSs
 * handle restart of system calls interrupted by signals differently.
 * The ncurses code needs signal-call restart to happen -- otherwise,
 * interrupted wgetch() calls will return FAIL, probably making the
 * application think the input stream has ended and it should
 * terminate.  In particular, you know you have this problem if, when
 * you suspend an ncurses-using lynx with ^Z and resume, it dies
 * immediately.
 *
 * Default behavior of POSIX sigaction(2) is not to restart
 * interrupted system calls, but Linux's sigaction does it anyway (at
 * least, on and after the 1.1.47 I (esr) use).  Thus this code works
 * OK under Linux.  The 4.4BSD sigaction(2) supports a (non-portable)
 * SA_RESTART flag that forces the right behavior.  Thus, this code
 * should work OK under BSD/OS, NetBSD, and FreeBSD (let us know if it
 * does not).
 *
 * Stock System Vs (and anything else using a strict-POSIX
 * sigaction(2) without SA_RESTART) may have a problem.  Possible
 * solutions:
 *
 *    sigvec      restarts by default (SV_INTERRUPT flag to not restart)
 *    signal      restarts by default in SVr4 (assuming you link with -lucb)
 *                and BSD, but not SVr3.
 *    sigset      restarts, but is only available under SVr4/Solaris.
 *
 * The signal(3) call is mandated by the ANSI standard, and its
 * interaction with sigaction(2) is described in the POSIX standard
 * (3.3.4.2, page 72,line 934).  According to section 8.1, page 191,
 * however, signal(3) itself is not required by POSIX.1.  And POSIX is
 * silent on whether it is required to restart signals.
 *
 * So.  The present situation is, we use sigaction(2) with no
 * guarantee of restart anywhere but on Linux and BSD.  We could
 * switch to signal(3) and collar Linux, BSD, and SVr4.  Any way
 * we slice it, System V UNIXes older than SVr4 will probably lose
 * (this may include XENIX).
 *
 * This implementation will probably be changed to use signal(3) in
 * the future.  If nothing else, it's simpler...
 */

#ifdef SIGTSTP
static void tstp(int dummy)
{
	sigset_t mask, omask;
	sigaction_t act, oact;

	T(("tstp() called"));

	/*
	 * The user may have changed the prog_mode tty bits, so save them.
	 */
	def_prog_mode();

	/*
	 * Block window change and timer signals.  The latter
	 * is because applications use timers to decide when
	 * to repaint the screen.
	 */
	(void)sigemptyset(&mask);
	(void)sigaddset(&mask, SIGALRM);
#ifdef SIGWINCH
	(void)sigaddset(&mask, SIGWINCH);
#endif
	(void)sigprocmask(SIG_BLOCK, &mask, &omask);

	/*
	 * End window mode, which also resets the terminal state to the
	 * original (pre-curses) modes.
	 */
	endwin();

	/* Unblock SIGTSTP. */
	(void)sigemptyset(&mask);
	(void)sigaddset(&mask, SIGTSTP);
	(void)sigprocmask(SIG_UNBLOCK, &mask, NULL);

	/* Now we want to resend SIGSTP to this process and suspend it */ 
	act.sa_handler = SIG_DFL;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
#ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */
	sigaction(SIGTSTP, &act, &oact);
	kill(getpid(), SIGTSTP);

	/* Process gets suspended...time passes...process resumes */

	T(("SIGCONT received"));
	sigaction(SIGTSTP, &oact, NULL);
	flushinp();

	/*
	 * If the user modified the tty state while suspended, he wants
	 * those changes to stick.  So save the new "default" terminal state.
	 */
	def_shell_mode();

	/*
	 * This relies on the fact that doupdate() will restore the 
	 * program-mode tty state, and issue enter_ca_mode if need be.
	 */
	doupdate();

	/* Reset the signals. */
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
}
#endif	/* defined(SIGTSTP) */

static void cleanup(int sig)
{
	/*
	 * Actually, doing any sort of I/O from within an signal handler is
	 * "unsafe".  But we'll _try_ to clean up the screen and terminal
	 * settings on the way out.
	 */
	if (sig == SIGINT
	 || sig == SIGQUIT) {
		sigaction_t act;
		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = SIG_IGN;
		if (sigaction(sig, &act, (sigaction_t *)0) == 0) {
			endwin();
		}
	}
	exit(1);
}

/*
 * If the given signal is still in its default state, set it to the given
 * handler.
 */
static int CatchIfDefault(int sig, sigaction_t *act)
{
	sigaction_t old_act;

#ifdef SA_RESTART
	act->sa_flags |= SA_RESTART;
#endif /* SA_RESTART */
	if (sigaction(sig, (sigaction_t *)0, &old_act) == 0
	 && old_act.sa_handler == SIG_DFL) {
		(void)sigaction(sig, act, (sigaction_t *)0);
		return TRUE;
	}
	return FALSE;
}

/*
 * This is invoked once at the beginning (e.g., from 'initscr()'), to
 * initialize the signal catchers, and thereafter when spawning a shell (and
 * returning) to disable/enable the SIGTSTP (i.e., ^Z) catcher.
 *
 * If the application has already set one of the signals, we'll not modify it
 * (during initialization).
 *
 * The XSI document implies that we shouldn't keep the SIGTSTP handler if
 * the caller later changes its mind, but that doesn't seem correct.
 */
void _nc_signal_handler(bool enable)
{
#ifdef SIGTSTP		/* Xenix 2.x doesn't have this */
static sigaction_t act, oact;
static int ignore;

	if (!ignore)
	{
		if (!enable)
		{
			act.sa_handler = SIG_IGN;
			sigaction(SIGTSTP, &act, &oact);
		}
		else if (act.sa_handler)
		{
			sigaction(SIGTSTP, &oact, NULL);
		}
		else	/*initialize */
		{
			sigemptyset(&act.sa_mask);
			act.sa_flags = 0;
#ifdef SA_RESTART
			act.sa_flags |= SA_RESTART;
#endif /* SA_RESTART */

			act.sa_handler = cleanup;
			CatchIfDefault(SIGINT,  &act);
			CatchIfDefault(SIGTERM, &act);

			act.sa_handler = tstp;
			if (!CatchIfDefault(SIGTSTP, &act))
				ignore = TRUE;
		}
	}
#else
	if (enable)
	{
		static sigaction_t act;
		act.sa_handler = cleanup;
		CatchIfDefault(SIGINT,  &act);
		CatchIfDefault(SIGTERM, &act);
	}
#endif
}
