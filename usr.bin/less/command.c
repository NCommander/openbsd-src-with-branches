/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * User-level command processor.
 */

#include "less.h"
#include "position.h"
#include "option.h"
#include "cmd.h"

extern int erase_char, kill_char;
extern int sigs;
extern int quit_at_eof;
extern int hit_eof;
extern int sc_width;
extern int sc_height;
extern int swindow;
extern int jump_sline;
extern int quitting;
extern int wscroll;
extern int nohelp;
extern int top_scroll;
extern int ignore_eoi;
extern char *every_first_cmd;
extern char *curr_altfilename;
extern char version[];
extern struct scrpos initial_scrpos;
extern IFILE curr_ifile;
#if CMD_HISTORY
extern void *ml_search;
extern void *ml_examine;
#if SHELL_ESCAPE || PIPEC
extern void *ml_shell;
#endif
#else
/* No CMD_HISTORY */
#define	ml_search	NULL
#define	ml_examine	NULL
#define	ml_shell  	NULL
#endif
#if EDITOR
extern char *editor;
extern char *editproto;
#endif
extern int screen_trashed;	/* The screen has been overwritten */
extern int be_helpful;

public int helpprompt;

static char ungot[100];
static char *ungotp = NULL;
#if SHELL_ESCAPE
static char *shellcmd = NULL;	/* For holding last shell command for "!!" */
#endif
static int mca;			/* The multicharacter command (action) */
static int search_type;		/* The previous type of search */
static int number;		/* The number typed by the user */
static char optchar;
static int optflag;
#if PIPEC
static char pipec;
#endif

static void multi_search();

/*
 * Move the cursor to lower left before executing a command.
 * This looks nicer if the command takes a long time before
 * updating the screen.
 */
	static void
cmd_exec()
{
	lower_left();
	flush();
}

/*
 * Set up the display to start a new multi-character command.
 */
	static void
start_mca(action, prompt, mlist)
	int action;
	char *prompt;
	void *mlist;
{
	mca = action;
	clear_bot();
	cmd_putstr(prompt);
#if CMD_HISTORY
	set_mlist(mlist);
#endif
}

	public int
in_mca()
{
	return (mca != 0 && mca != A_PREFIX);
}

/*
 * Set up the display to start a new search command.
 */
	static void
mca_search()
{
	if (search_type & SRCH_FORW)
		mca = A_F_SEARCH;
	else
		mca = A_B_SEARCH;

	clear_bot();

	if (search_type & SRCH_FIRST_FILE)
		cmd_putstr("@");

	if (search_type & SRCH_PAST_EOF)
		cmd_putstr("*");

	if (search_type & SRCH_NOMATCH)
		cmd_putstr("!");

	if (search_type & SRCH_FORW)
		cmd_putstr("/");
	else
		cmd_putstr("?");
#if CMD_HISTORY
	set_mlist(ml_search);
#endif
}

/*
 * Execute a multicharacter command.
 */
	static void
exec_mca()
{
	register char *cbuf;

	cmd_exec();
	cbuf = get_cmdbuf();

	switch (mca)
	{
	case A_F_SEARCH:
	case A_B_SEARCH:
		multi_search(cbuf, number);
		break;
	case A_FIRSTCMD:
		/*
		 * Skip leading spaces or + signs in the string.
		 */
		while (*cbuf == '+' || *cbuf == ' ')
			cbuf++;
		if (every_first_cmd != NULL)
			free(every_first_cmd);
		if (*cbuf == '\0')
			every_first_cmd = NULL;
		else
			every_first_cmd = save(cbuf);
		break;
	case A_OPT_TOGGLE:
		toggle_option(optchar, cbuf, optflag);
		optchar = '\0';
		break;
	case A_F_BRACKET:
		match_brac(cbuf[0], cbuf[1], 1, number);
		break;
	case A_B_BRACKET:
		match_brac(cbuf[1], cbuf[0], 0, number);
		break;
#if EXAMINE
	case A_EXAMINE:
		edit_list(cbuf);
		break;
#endif
#if SHELL_ESCAPE
	case A_SHELL:
		/*
		 * !! just uses whatever is in shellcmd.
		 * Otherwise, copy cmdbuf to shellcmd,
		 * expanding any special characters ("%" or "#").
		 */
		if (*cbuf != '!')
		{
			if (shellcmd != NULL)
				free(shellcmd);
			shellcmd = fexpand(cbuf);
		}

		if (shellcmd == NULL)
			lsystem("");
		else
			lsystem(shellcmd);
		error("!done", NULL_PARG);
		break;
#endif
#if PIPEC
	case A_PIPE:
		(void) pipe_mark(pipec, cbuf);
		error("|done", NULL_PARG);
		break;
#endif
	}
}

/*
 * Add a character to a multi-character command.
 */
	static int
mca_char(c)
	int c;
{
	char *p;
	int flag;
	char buf[3];

	switch (mca)
	{
	case 0:
		/*
		 * Not in a multicharacter command.
		 */
		return (NO_MCA);

	case A_PREFIX:
		/*
		 * In the prefix of a command.
		 * This not considered a multichar command
		 * (even tho it uses cmdbuf, etc.).
		 * It is handled in the commands() switch.
		 */
		return (NO_MCA);

	case A_DIGIT:
		/*
		 * Entering digits of a number.
		 * Terminated by a non-digit.
		 */
		if ((c < '0' || c > '9') && 
		  editchar(c, EC_PEEK|EC_NOHISTORY|EC_NOCOMPLETE) == A_INVALID)
		{
			/*
			 * Not part of the number.
			 * Treat as a normal command character.
			 */
			number = cmd_int();
			mca = 0;
			cmd_accept();
			return (NO_MCA);
		}
		break;

	case A_OPT_TOGGLE:
		/*
		 * Special case for the TOGGLE_OPTION command.
		 * If the option letter which was entered is a
		 * single-char option, execute the command immediately,
		 * so user doesn't have to hit RETURN.
		 * If the first char is + or -, this indicates
		 * OPT_UNSET or OPT_SET respectively, instead of OPT_TOGGLE.
		 */
		if (c == erase_char || c == kill_char)
			break;
		if (optchar != '\0' && optchar != '+' && optchar != '-')
			/*
			 * We already have the option letter.
			 */
			break;
		switch (c)
		{
		case '+':
			optflag = OPT_UNSET;
			break;
		case '-':
			optflag = OPT_SET;
			break;
		default:
			optchar = c;
			if (optflag != OPT_TOGGLE || single_char_option(c))
			{
				toggle_option(c, "", optflag);
				return (MCA_DONE);
			}
			break;
		}
		if (optchar == '+' || optchar == '-')
		{
			optchar = c;
			break;
		}
		/*
		 * Display a prompt appropriate for the option letter.
		 */
		if ((p = opt_prompt(c)) == NULL)
		{
			buf[0] = '-';
			buf[1] = c;
			buf[2] = '\0';
			p = buf;
		}
		start_mca(A_OPT_TOGGLE, p, (void*)NULL);
		return (MCA_MORE);

	case A_F_SEARCH:
	case A_B_SEARCH:
		/*
		 * Special case for search commands.
		 * Certain characters as the first char of 
		 * the pattern have special meaning:
		 *	!  Toggle the NOMATCH flag
		 *	*  Toggle the PAST_EOF flag
		 *	@  Toggle the FIRST_FILE flag
		 */
		if (len_cmdbuf() > 0)
			/*
			 * Only works for the first char of the pattern.
			 */
			break;

		flag = 0;
		switch (c)
		{
		case '!':
			flag = SRCH_NOMATCH;
			break;
		case '@':
			flag = SRCH_FIRST_FILE;
			break;
		case '*':
			flag = SRCH_PAST_EOF;
			break;
		}
		if (flag != 0)
		{
			search_type ^= flag;
			mca_search();
			return (MCA_MORE);
		}
		break;
	}

	/*
	 * Any other multicharacter command
	 * is terminated by a newline.
	 */
	if (c == '\n' || c == '\r')
	{
		/*
		 * Execute the command.
		 */
		exec_mca();
		return (MCA_DONE);
	}
	/*
	 * Append the char to the command buffer.
	 */
	if (cmd_char(c) == CC_QUIT)
		/*
		 * Abort the multi-char command.
		 */
		return (MCA_DONE);

	if ((mca == A_F_BRACKET || mca == A_B_BRACKET) && len_cmdbuf() >= 2)
	{
		/*
		 * Special case for the bracket-matching commands.
		 * Execute the command after getting exactly two
		 * characters from the user.
		 */
		exec_mca();
		return (MCA_DONE);
	}

	/*
	 * Need another character.
	 */
	return (MCA_MORE);
}

/*
 * Display the appropriate prompt.
 */
	static void
prompt()
{
	register char *p;

	if (ungotp != NULL && ungotp > ungot)
	{
		/*
		 * No prompt necessary if commands are from 
		 * ungotten chars rather than from the user.
		 */
		return;
	}

	/*
	 * If nothing is displayed yet, display starting from initial_scrpos.
	 */
	if (empty_screen())
	{
		if (initial_scrpos.pos == NULL_POSITION)
			/*
			 * {{ Maybe this should be:
			 *    jump_loc(ch_zero(), jump_sline);
			 *    but this behavior seems rather unexpected 
			 *    on the first screen. }}
			 */
			jump_loc(ch_zero(), 1);
		else
			jump_loc(initial_scrpos.pos, initial_scrpos.ln);
	} else if (screen_trashed)
	{
		int save_top_scroll;
		save_top_scroll = top_scroll;
		top_scroll = 1;
		repaint();
		top_scroll = save_top_scroll;
	}

	/*
	 * If the -E flag is set and we've hit EOF on the last file, quit.
	 */
	if (quit_at_eof == OPT_ONPLUS && hit_eof && 
	    next_ifile(curr_ifile) == NULL_IFILE)
		quit(QUIT_OK);

	/*
	 * Select the proper prompt and display it.
	 */
	clear_bot();
	if (helpprompt) {
		so_enter();
		putstr("[Press 'h' for instructions.]");
		so_exit();
		helpprompt = 0;
	} else {
		p = pr_string();
		if (p == NULL)
			putchr(':');
		else
		{
			so_enter();
			putstr(p);
			if (be_helpful)
				putstr(" [Press space to continue, 'q' to quit.]");
			so_exit();
		}
	}
}

	public void
dispversion()
{
	PARG parg;

	parg.p_string = version;
	error("less  version %s", &parg);
}

/*
 * Get command character.
 * The character normally comes from the keyboard,
 * but may come from ungotten characters
 * (characters previously given to ungetcc or ungetsc).
 */
	public int
getcc()
{
	if (ungotp == NULL)
		/*
		 * Normal case: no ungotten chars, so get one from the user.
		 */
		return (getchr());

	if (ungotp > ungot)
		/*
		 * Return the next ungotten char.
		 */
		return (*--ungotp);

	/*
	 * We have just run out of ungotten chars.
	 */
	ungotp = NULL;
	if (len_cmdbuf() == 0 || !empty_screen())
		return (getchr());
	/*
	 * Command is incomplete, so try to complete it.
	 */
	switch (mca)
	{
	case A_DIGIT:
		/*
		 * We have a number but no command.  Treat as #g.
		 */
		return ('g');

	case A_F_SEARCH:
	case A_B_SEARCH:
		/*
		 * We have "/string" but no newline.  Add the \n.
		 */
		return ('\n'); 

	default:
		/*
		 * Some other incomplete command.  Let user complete it.
		 */
		return (getchr());
	}
}

/*
 * "Unget" a command character.
 * The next getcc() will return this character.
 */
	public void
ungetcc(c)
	int c;
{
	if (ungotp == NULL)
		ungotp = ungot;
	if (ungotp >= ungot + sizeof(ungot))
	{
		error("ungetcc overflow", NULL_PARG);
		quit(QUIT_ERROR);
	}
	*ungotp++ = c;
}

/*
 * Unget a whole string of command characters.
 * The next sequence of getcc()'s will return this string.
 */
	public void
ungetsc(s)
	char *s;
{
	register char *p;

	for (p = s + strlen(s) - 1;  p >= s;  p--)
		ungetcc(*p);
}

/*
 * Search for a pattern, possibly in multiple files.
 * If SRCH_FIRST_FILE is set, begin searching at the first file.
 * If SRCH_PAST_EOF is set, continue the search thru multiple files.
 */
	static void
multi_search(pattern, n)
	char *pattern;
	int n;
{
	register int nomore;
	IFILE save_ifile;
	int changed_file;

	changed_file = 0;
	save_ifile = curr_ifile;

	if (search_type & SRCH_FIRST_FILE)
	{
		/*
		 * Start at the first (or last) file 
		 * in the command line list.
		 */
		if (search_type & SRCH_FORW)
			nomore = edit_first();
		else
			nomore = edit_last();
		if (nomore)
			return;
		changed_file = 1;
		search_type &= ~SRCH_FIRST_FILE;
	}

	for (;;)
	{
		if ((n = search(search_type, pattern, n)) == 0)
			/*
			 * Found it.
			 */
			return;

		if (n < 0)
			/*
			 * Some kind of error in the search.
			 * Error message has been printed by search().
			 */
			break;

		if ((search_type & SRCH_PAST_EOF) == 0)
			/*
			 * We didn't find a match, but we're
			 * supposed to search only one file.
			 */
			break;
		/*
		 * Move on to the next file.
		 */
		if (search_type & SRCH_FORW)
			nomore = edit_next(1);
		else
			nomore = edit_prev(1);
		if (nomore)
			break;
		changed_file = 1;
	}

	/*
	 * Didn't find it.
	 * Print an error message if we haven't already.
	 */
	if (n > 0)
		error("Pattern not found", NULL_PARG);

	if (changed_file)
	{
		/*
		 * Restore the file we were originally viewing.
		 */
		if (edit_ifile(save_ifile))
			quit(QUIT_ERROR);
	}
}

/*
 * Main command processor.
 * Accept and execute commands until a quit command.
 */
	public void
commands()
{
	register int c;
	register int action;
	register char *cbuf;
	int save_search_type;
	char *s;
	char tbuf[2];
	PARG parg;

	search_type = SRCH_FORW;
	wscroll = (sc_height + 1) / 2;

	for (;;)
	{
		mca = 0;
		cmd_accept();
		number = 0;
		optchar = '\0';

		/*
		 * See if any signals need processing.
		 */
		if (sigs)
		{
			psignals();
			if (quitting)
				quit(QUIT_SAVED_STATUS);
		}
			
		/*
		 * Display prompt and accept a character.
		 */
		cmd_reset();
		prompt();
		if (sigs)
			continue;
		c = getcc();

	again:
		if (sigs)
			continue;

		/*
		 * If we are in a multicharacter command, call mca_char.
		 * Otherwise we call fcmd_decode to determine the
		 * action to be performed.
		 */
		if (mca)
			switch (mca_char(c))
			{
			case MCA_MORE:
				/*
				 * Need another character.
				 */
				c = getcc();
				goto again;
			case MCA_DONE:
				/*
				 * Command has been handled by mca_char.
				 * Start clean with a prompt.
				 */
				continue;
			case NO_MCA:
				/*
				 * Not a multi-char command
				 * (at least, not anymore).
				 */
				break;
			}

		/*
		 * Decode the command character and decide what to do.
		 */
		if (mca)
		{
			/*
			 * We're in a multichar command.
			 * Add the character to the command buffer
			 * and display it on the screen.
			 * If the user backspaces past the start 
			 * of the line, abort the command.
			 */
			if (cmd_char(c) == CC_QUIT || len_cmdbuf() == 0)
				continue;
			cbuf = get_cmdbuf();
		} else
		{
			/*
			 * Don't use cmd_char if we're starting fresh
			 * at the beginning of a command, because we
			 * don't want to echo the command until we know
			 * it is a multichar command.  We also don't
			 * want erase_char/kill_char to be treated
			 * as line editing characters.
			 */
			tbuf[0] = c;
			tbuf[1] = '\0';
			cbuf = tbuf;
		}
		s = NULL;
		action = fcmd_decode(cbuf, &s);
		/*
		 * If an "extra" string was returned,
		 * process it as a string of command characters.
		 */
		if (s != NULL)
			ungetsc(s);
		/*
		 * Clear the cmdbuf string.
		 * (But not if we're in the prefix of a command,
		 * because the partial command string is kept there.)
		 */
		if (action != A_PREFIX)
			cmd_reset();

		switch (action)
		{
		case A_DIGIT:
			/*
			 * First digit of a number.
			 */
			start_mca(A_DIGIT, ":", (void*)NULL);
			goto again;

		case A_F_WINDOW:
			/*
			 * Forward one window (and set the window size).
			 */
			if (number > 0)
				swindow = number;
			/* FALLTHRU */
		case A_F_SCREEN:
			/*
			 * Forward one screen.
			 */
			if (number <= 0)
				number = get_swindow();
			cmd_exec();
			forward(number, 0, 1);
			break;

		case A_B_WINDOW:
			/*
			 * Backward one window (and set the window size).
			 */
			if (number > 0)
				swindow = number;
			/* FALLTHRU */
		case A_B_SCREEN:
			/*
			 * Backward one screen.
			 */
			if (number <= 0)
				number = get_swindow();
			cmd_exec();
			backward(number, 0, 1);
			break;

		case A_F_LINE:
			/*
			 * Forward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			forward(number, 0, 0);
			break;

		case A_B_LINE:
			/*
			 * Backward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			backward(number, 0, 0);
			break;

		case A_FF_LINE:
			/*
			 * Force forward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			forward(number, 1, 0);
			break;

		case A_BF_LINE:
			/*
			 * Force backward N (default 1) line.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			backward(number, 1, 0);
			break;
		
		case A_F_FOREVER:
			/*
			 * Forward forever, ignoring EOF.
			 */
			cmd_exec();
			jump_forw();
			ignore_eoi = 1;
			hit_eof = 0;
			while (!ABORT_SIGS())
				forward(1, 0, 0);
			ignore_eoi = 0;
			break;

		case A_F_SCROLL:
			/*
			 * Forward N lines 
			 * (default same as last 'd' or 'u' command).
			 */
			if (number > 0)
				wscroll = number;
			cmd_exec();
			forward(wscroll, 0, 0);
			break;

		case A_B_SCROLL:
			/*
			 * Forward N lines 
			 * (default same as last 'd' or 'u' command).
			 */
			if (number > 0)
				wscroll = number;
			cmd_exec();
			backward(wscroll, 0, 0);
			break;

		case A_FREPAINT:
			/*
			 * Flush buffers, then repaint screen.
			 * Don't flush the buffers on a pipe!
			 */
			if (ch_getflags() & CH_CANSEEK)
			{
				ch_flush();
				clr_linenum();
			}
			/* FALLTHRU */
		case A_REPAINT:
			/*
			 * Repaint screen.
			 */
			cmd_exec();
			repaint();
			break;

		case A_GOLINE:
			/*
			 * Go to line N, default beginning of file.
			 */
			if (number <= 0)
				number = 1;
			cmd_exec();
			jump_back(number);
			break;

		case A_PERCENT:
			/*
			 * Go to a specified percentage into the file.
			 */
			if (number < 0)
				number = 0;
			if (number > 100)
				number = 100;
			cmd_exec();
			jump_percent(number);
			break;

		case A_GOEND:
			/*
			 * Go to line N, default end of file.
			 */
			cmd_exec();
			if (number <= 0)
				jump_forw();
			else
				jump_back(number);
			break;

		case A_GOPOS:
			/*
			 * Go to a specified byte position in the file.
			 */
			cmd_exec();
			if (number < 0)
				number = 0;
			jump_line_loc((POSITION)number, jump_sline);
			break;

		case A_STAT:
			/*
			 * Print file name, etc.
			 */
			cmd_exec();
			parg.p_string = eq_message();
			error("%s", &parg);
			break;
			
		case A_VERSION:
			/*
			 * Print version number, without the "@(#)".
			 */
			cmd_exec();
			dispversion();
			break;

		case A_QUIT:
			/*
			 * Exit.
			 */
			quit(QUIT_OK);

/*
 * Define abbreviation for a commonly used sequence below.
 */
#define	DO_SEARCH()	if (number <= 0) number = 1;	\
			mca_search();			\
			cmd_exec();			\
			multi_search((char *)NULL, number);


		case A_F_SEARCH:
			/*
			 * Search forward for a pattern.
			 * Get the first char of the pattern.
			 */
			search_type = SRCH_FORW;
			if (number <= 0)
				number = 1;
			mca_search();
			c = getcc();
			goto again;

		case A_B_SEARCH:
			/*
			 * Search backward for a pattern.
			 * Get the first char of the pattern.
			 */
			search_type = SRCH_BACK;
			if (number <= 0)
				number = 1;
			mca_search();
			c = getcc();
			goto again;

		case A_AGAIN_SEARCH:
			/*
			 * Repeat previous search.
			 */
			DO_SEARCH();
			break;
		
		case A_T_AGAIN_SEARCH:
			/*
			 * Repeat previous search, multiple files.
			 */
			search_type |= SRCH_PAST_EOF;
			DO_SEARCH();
			break;

		case A_REVERSE_SEARCH:
			/*
			 * Repeat previous search, in reverse direction.
			 */
			save_search_type = search_type;
			search_type = SRCH_REVERSE(search_type);
			DO_SEARCH();
			search_type = save_search_type;
			break;

		case A_T_REVERSE_SEARCH:
			/* 
			 * Repeat previous search, 
			 * multiple files in reverse direction.
			 */
			save_search_type = search_type;
			search_type = SRCH_REVERSE(search_type);
			search_type |= SRCH_PAST_EOF;
			DO_SEARCH();
			search_type = save_search_type;
			break;

		case A_UNDO_SEARCH:
			undo_search();
			break;

		case A_HELP:
			/*
			 * Help.
			 */
			if (nohelp)
			{
				bell();
				break;
			}
			clear_bot();
			putstr(" help");
			cmd_exec();
			help(0);
			break;

		case A_EXAMINE:
#if EXAMINE
			/*
			 * Edit a new file.  Get the filename.
			 */
			start_mca(A_EXAMINE, "Examine: ", ml_examine);
			c = getcc();
			goto again;
#else
			error("Command not available", NULL_PARG);
			break;
#endif
			
		case A_VISUAL:
			/*
			 * Invoke an editor on the input file.
			 */
#if EDITOR
			if (strcmp(get_filename(curr_ifile), "-") == 0)
			{
				error("Cannot edit standard input", NULL_PARG);
				break;
			}
			if (curr_altfilename != NULL)
			{
				error("Cannot edit file processed with LESSOPEN", 
					NULL_PARG);
				break;
			}
			/*
			 * Expand the editor prototype string
			 * and pass it to the system to execute.
			 */
			cmd_exec();
			lsystem(pr_expand(editproto, 0));
			/*
			 * Re-edit the file, since data may have changed.
			 * Some editors even recreate the file, so flushing
			 * buffers is not sufficient.
			 */
			if (edit_ifile(curr_ifile))
				quit(QUIT_ERROR);
			break;
#else
			error("Command not available", NULL_PARG);
			break;
#endif

		case A_NEXT_FILE:
			/*
			 * Examine next file.
			 */
			if (number <= 0)
				number = 1;
			if (edit_next(number))
			{
				if (quit_at_eof && hit_eof)
					quit(QUIT_OK);
				parg.p_string = (number > 1) ? "(N-th) " : "";
				error("No %snext file", &parg);
			}
			break;

		case A_PREV_FILE:
			/*
			 * Examine previous file.
			 */
			if (number <= 0)
				number = 1;
			if (edit_prev(number))
			{
				parg.p_string = (number > 1) ? "(N-th) " : "";
				error("No %sprevious file", &parg);
			}
			break;

		case A_INDEX_FILE:
			/*
			 * Examine a particular file.
			 */
			if (number <= 0)
				number = 1;
			if (edit_index(number))
				error("No such file", NULL_PARG);
			break;

		case A_OPT_TOGGLE:
			start_mca(A_OPT_TOGGLE, "-", (void*)NULL);
			optflag = OPT_TOGGLE;
			c = getcc();
			goto again;

		case A_DISP_OPTION:
			/*
			 * Report a flag setting.
			 */
			start_mca(A_DISP_OPTION, "_", (void*)NULL);
			c = getcc();
			if (c == erase_char || c == kill_char)
				break;
			toggle_option(c, "", OPT_NO_TOGGLE);
			break;

		case A_FIRSTCMD:
			/*
			 * Set an initial command for new files.
			 */
			start_mca(A_FIRSTCMD, "+", (void*)NULL);
			c = getcc();
			goto again;

		case A_SHELL:
			/*
			 * Shell escape.
			 */
#if SHELL_ESCAPE
			start_mca(A_SHELL, "!", ml_shell);
			c = getcc();
			goto again;
#else
			error("Command not available", NULL_PARG);
			break;
#endif

		case A_SETMARK:
			/*
			 * Set a mark.
			 */
			start_mca(A_SETMARK, "mark: ", (void*)NULL);
			c = getcc();
			if (c == erase_char || c == kill_char ||
			    c == '\n' || c == '\r')
				break;
			setmark(c);
			break;

		case A_GOMARK:
			/*
			 * Go to a mark.
			 */
			start_mca(A_GOMARK, "goto mark: ", (void*)NULL);
			c = getcc();
			if (c == erase_char || c == kill_char || 
			    c == '\n' || c == '\r')
				break;
			gomark(c);
			break;

		case A_PIPE:
#if PIPEC
			start_mca(A_PIPE, "|mark: ", (void*)NULL);
			c = getcc();
			if (c == erase_char || c == kill_char)
				break;
			if (c == '\n' || c == '\r')
				c = '.';
			if (badmark(c))
				break;
			pipec = c;
			start_mca(A_PIPE, "!", ml_shell);
			c = getcc();
			goto again;
#else
			error("Command not available", NULL_PARG);
			break;
#endif

		case A_B_BRACKET:
		case A_F_BRACKET:
			start_mca(action, "Brackets: ", (void*)NULL);
			c = getcc();
			goto again;

		case A_PREFIX:
			/*
			 * The command is incomplete (more chars are needed).
			 * Display the current char, so the user knows
			 * what's going on, and get another character.
			 */
			if (mca != A_PREFIX)
			{
				start_mca(A_PREFIX, " ", (void*)NULL);
				cmd_reset();
				(void) cmd_char(c);
			}
			c = getcc();
			goto again;

		case A_NOACTION:
			break;

		default:
			if (be_helpful)
				helpprompt = 1;
			else
				bell();
			break;
		}
	}
}
