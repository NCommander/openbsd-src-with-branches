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
 * Routines which jump to a new location in the file.
 */

#include "less.h"
#include "position.h"

extern int hit_eof;
extern int jump_sline;
extern int squished;
extern int screen_trashed;
extern int sc_width, sc_height;

/*
 * Jump to the end of the file.
 */
	public void
jump_forw()
{
	POSITION pos;

	if (ch_end_seek())
	{
		error("Cannot seek to end of file", NULL_PARG);
		return;
	}
	/*
	 * Position the last line in the file at the last screen line.
	 * Go back one line from the end of the file
	 * to get to the beginning of the last line.
	 */
	pos = back_line(ch_tell());
	if (pos == NULL_POSITION)
		jump_loc((POSITION)0, sc_height-1);
	else
		jump_loc(pos, sc_height-1);
}

/*
 * Jump to line n in the file.
 */
	public void
jump_back(n)
	int n;
{
	POSITION pos;
	PARG parg;

	/*
	 * Find the position of the specified line.
	 * If we can seek there, just jump to it.
	 * If we can't seek, but we're trying to go to line number 1,
	 * use ch_beg_seek() to get as close as we can.
	 */
	pos = find_pos(n);
	if (pos != NULL_POSITION && ch_seek(pos) == 0)
	{
		jump_loc(pos, jump_sline);
	} else if (n <= 1 && ch_beg_seek() == 0)
	{
		jump_loc(ch_tell(), jump_sline);
		error("Cannot seek to beginning of file", NULL_PARG);
	} else
	{
		parg.p_int = n;
		error("Cannot seek to line number %d", &parg);
	}
}

/*
 * Repaint the screen.
 */
	public void
repaint()
{
	struct scrpos scrpos;
	/*
	 * Start at the line currently at the top of the screen
	 * and redisplay the screen.
	 */
	get_scrpos(&scrpos);
	pos_clear();
	jump_loc(scrpos.pos, scrpos.ln);
}

/*
 * Jump to a specified percentage into the file.
 */
	public void
jump_percent(percent)
	int percent;
{
	POSITION pos, len;

	/*
	 * Determine the position in the file
	 * (the specified percentage of the file's length).
	 */
	if ((len = ch_length()) == NULL_POSITION)
	{
		ierror("Determining length of file", NULL_PARG);
		ch_end_seek();
	}
	if ((len = ch_length()) == NULL_POSITION)
	{
		error("Don't know length of file", NULL_PARG);
		return;
	}
	/*
	 * {{ This calculation may overflow! }}
	 */
	pos = (percent * len) / 100;
	if (pos >= len)
		pos = len-1;

	jump_line_loc(pos, jump_sline);
}

/*
 * Jump to a specified position in the file.
 * Like jump_loc, but the position need not be 
 * the first character in a line.
 */
	public void
jump_line_loc(pos, sline)
	POSITION pos;
	int sline;
{
	int c;

	if (ch_seek(pos) == 0)
	{
		/*
		 * Back up to the beginning of the line.
		 */
		while ((c = ch_back_get()) != '\n' && c != EOI)
			;
		if (c == '\n')
			(void) ch_forw_get();
		pos = ch_tell();
	}
	jump_loc(pos, sline);
}

/*
 * Jump to a specified position in the file.
 * The position must be the first character in a line.
 * Place the target line on a specified line on the screen.
 */
	public void
jump_loc(pos, sline)
	POSITION pos;
	int sline;
{
	register int nline;
	POSITION tpos;
	POSITION bpos;

	/*
	 * Normalize sline.
	 */
	sline = adjsline(sline);

	if ((nline = onscreen(pos)) >= 0)
	{
		/*
		 * The line is currently displayed.  
		 * Just scroll there.
		 */
		nline -= sline;
		if (nline > 0)
			forw(nline, position(BOTTOM_PLUS_ONE), 1, 0, 0);
		else
			back(-nline, position(TOP), 1, 0);
		return;
	}

	/*
	 * Line is not on screen.
	 * Seek to the desired location.
	 */
	if (ch_seek(pos))
	{
		error("Cannot seek to that file position", NULL_PARG);
		return;
	}

	/*
	 * See if the desired line is before or after 
	 * the currently displayed screen.
	 */
	tpos = position(TOP);
	bpos = position(BOTTOM_PLUS_ONE);
	if (tpos == NULL_POSITION || pos >= tpos)
	{
		/*
		 * The desired line is after the current screen.
		 * Move back in the file far enough so that we can
		 * call forw() and put the desired line at the 
		 * sline-th line on the screen.
		 */
		for (nline = 0;  nline < sline;  nline++)
		{
			if (bpos != NULL_POSITION && pos <= bpos)
			{
				/*
				 * Surprise!  The desired line is
				 * close enough to the current screen
				 * that we can just scroll there after all.
				 */
				forw(sc_height-sline+nline-1, bpos, 1, 0, 0);
				return;
			}
			pos = back_line(pos);
			if (pos == NULL_POSITION)
			{
				/*
				 * Oops.  Ran into the beginning of the file.
				 * Exit the loop here and rely on forw()
				 * below to draw the required number of
				 * blank lines at the top of the screen.
				 */
				break;
			}
		}
		lastmark();
		hit_eof = 0;
		squished = 0;
		screen_trashed = 0;
		forw(sc_height-1, pos, 1, 0, sline-nline);
	} else
	{
		/*
		 * The desired line is before the current screen.
		 * Move forward in the file far enough so that we
		 * can call back() and put the desired line at the 
		 * sline-th line on the screen.
		 */
		for (nline = sline;  nline < sc_height - 1;  nline++)
		{
			pos = forw_line(pos);
			if (pos == NULL_POSITION)
			{
				/*
				 * Ran into end of file.
				 * This shouldn't normally happen, 
				 * but may if there is some kind of read error.
				 */
				break;
			}
			if (pos >= tpos)
			{
				/* 
				 * Surprise!  The desired line is
				 * close enough to the current screen
				 * that we can just scroll there after all.
				 */
				back(nline+1, tpos, 1, 0);
				return;
			}
		}
		lastmark();
		clear();
		screen_trashed = 0;
		add_back_pos(pos);
		back(sc_height-1, pos, 1, 0);
	}
}
