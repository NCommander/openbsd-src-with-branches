/* This is the Assembler Pre-Processor
   Copyright (C) 1987, 1990, 1991, 1992, 1994 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Modified by Allen Wirfs-Brock, Instantiations Inc 2/90 */
/* App, the assembler pre-processor.  This pre-processor strips out excess
   spaces, turns single-quoted characters into a decimal constant, and turns
   # <number> <filename> <garbage> into a .line <number>\n.file <filename>
   pair.  This needs better error-handling.  */

#ifndef lint
static char rcsid[] = "$Id: app.c,v 1.6 1994/08/24 20:12:01 pk Exp $";
#endif

#include <stdio.h>
#include "as.h"			/* For BAD_CASE() only */

#if (__STDC__ != 1)
#ifndef const
#define const  /* empty */
#endif
#endif

static char lex[256];
static const char symbol_chars[] =
"$._ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

#define LEX_IS_SYMBOL_COMPONENT		1
#define LEX_IS_WHITESPACE		2
#define LEX_IS_LINE_SEPARATOR		3
#define LEX_IS_COMMENT_START		4
#define LEX_IS_LINE_COMMENT_START	5
#define	LEX_IS_TWOCHAR_COMMENT_1ST	6
#define	LEX_IS_TWOCHAR_COMMENT_2ND	7
#define	LEX_IS_STRINGQUOTE		8
#define	LEX_IS_COLON			9
#define	LEX_IS_NEWLINE			10
#define	LEX_IS_ONECHAR_QUOTE		11
#define IS_SYMBOL_COMPONENT(c)		(lex[c] == LEX_IS_SYMBOL_COMPONENT)
#define IS_WHITESPACE(c)		(lex[c] == LEX_IS_WHITESPACE)
#define IS_LINE_SEPARATOR(c)		(lex[c] == LEX_IS_LINE_SEPARATOR)
#define IS_COMMENT(c)			(lex[c] == LEX_IS_COMMENT_START)
#define IS_LINE_COMMENT(c)		(lex[c] == LEX_IS_LINE_COMMENT_START)
#define	IS_NEWLINE(c)			(lex[c] == LEX_IS_NEWLINE)

static int process_escape PARAMS ((int));

/* FIXME-soon: The entire lexer/parser thingy should be
   built statically at compile time rather than dynamically
   each and every time the assembler is run.  xoxorich. */

void 
do_scrub_begin ()
{
  const char *p;

  lex[' '] = LEX_IS_WHITESPACE;
  lex['\t'] = LEX_IS_WHITESPACE;
  lex['\n'] = LEX_IS_NEWLINE;
  lex[';'] = LEX_IS_LINE_SEPARATOR;
  lex['"'] = LEX_IS_STRINGQUOTE;
#ifndef TC_HPPA
  lex['\''] = LEX_IS_ONECHAR_QUOTE;
#endif
  lex[':'] = LEX_IS_COLON;



#ifdef SINGLE_QUOTE_STRINGS
  lex['\''] = LEX_IS_STRINGQUOTE;
#endif

  /* Note that these override the previous defaults, e.g. if ';' is a
     comment char, then it isn't a line separator.  */
  for (p = symbol_chars; *p; ++p)
    {
      lex[(unsigned char) *p] = LEX_IS_SYMBOL_COMPONENT;
    }				/* declare symbol characters */

  for (p = comment_chars; *p; p++)
    {
      lex[(unsigned char) *p] = LEX_IS_COMMENT_START;
    }				/* declare comment chars */

  for (p = line_comment_chars; *p; p++)
    {
      lex[(unsigned char) *p] = LEX_IS_LINE_COMMENT_START;
    }				/* declare line comment chars */

  for (p = line_separator_chars; *p; p++)
    {
      lex[(unsigned char) *p] = LEX_IS_LINE_SEPARATOR;
    }				/* declare line separators */

  /* Only allow slash-star comments if slash is not in use */
  if (lex['/'] == 0)
    {
      lex['/'] = LEX_IS_TWOCHAR_COMMENT_1ST;
    }
  /* FIXME-soon.  This is a bad hack but otherwise, we can't do
     c-style comments when '/' is a line comment char. xoxorich. */
  if (lex['*'] == 0)
    {
      lex['*'] = LEX_IS_TWOCHAR_COMMENT_2ND;
    }
}				/* do_scrub_begin() */

FILE *scrub_file;

int 
scrub_from_file ()
{
  return getc (scrub_file);
}

void 
scrub_to_file (ch)
     int ch;
{
  ungetc (ch, scrub_file);
}				/* scrub_to_file() */

char *scrub_string;
char *scrub_last_string;

int 
scrub_from_string ()
{
  return scrub_string == scrub_last_string ? EOF : *scrub_string++;
}				/* scrub_from_string() */

void 
scrub_to_string (ch)
     int ch;
{
  *--scrub_string = ch;
}				/* scrub_to_string() */

/* Saved state of the scrubber */
static int state;
static int old_state;
static char *out_string;
static char out_buf[20];
static int add_newlines = 0;

/* Data structure for saving the state of app across #include's.  Note that
   app is called asynchronously to the parsing of the .include's, so our
   state at the time .include is interpreted is completely unrelated.
   That's why we have to save it all.  */

struct app_save
  {
    int state;
    int old_state;
    char *out_string;
    char out_buf[sizeof (out_buf)];
    int add_newlines;
    char *scrub_string;
    char *scrub_last_string;
    FILE *scrub_file;
  };

char *
app_push ()
{
  register struct app_save *saved;

  saved = (struct app_save *) xmalloc (sizeof (*saved));
  saved->state = state;
  saved->old_state = old_state;
  saved->out_string = out_string;
  memcpy (saved->out_buf, out_buf, sizeof (out_buf));
  saved->add_newlines = add_newlines;
  saved->scrub_string = scrub_string;
  saved->scrub_last_string = scrub_last_string;
  saved->scrub_file = scrub_file;

  /* do_scrub_begin() is not useful, just wastes time. */
  return (char *) saved;
}

void 
app_pop (arg)
     char *arg;
{
  register struct app_save *saved = (struct app_save *) arg;

  /* There is no do_scrub_end (). */
  state = saved->state;
  old_state = saved->old_state;
  out_string = saved->out_string;
  memcpy (out_buf, saved->out_buf, sizeof (out_buf));
  add_newlines = saved->add_newlines;
  scrub_string = saved->scrub_string;
  scrub_last_string = saved->scrub_last_string;
  scrub_file = saved->scrub_file;

  free (arg);
}				/* app_pop() */

/* @@ This assumes that \n &c are the same on host and target.  This is not
   necessarily true.  */
static int 
process_escape (ch)
     int ch;
{
  switch (ch)
    {
    case 'b':
      return '\b';
    case 'f':
      return '\f';
    case 'n':
      return '\n';
    case 'r':
      return '\r';
    case 't':
      return '\t';
    case '\'':
      return '\'';
    case '"':
      return '\"';
    default:
      return ch;
    }
}
int 
do_scrub_next_char (get, unget)
     int (*get) ();
     void (*unget) ();
{
  /*State 0: beginning of normal line
	  1: After first whitespace on line (flush more white)
	  2: After first non-white (opcode) on line (keep 1white)
	  3: after second white on line (into operands) (flush white)
	  4: after putting out a .line, put out digits
	  5: parsing a string, then go to old-state
	  6: putting out \ escape in a "d string.
	  7: After putting out a .appfile, put out string.
	  8: After putting out a .appfile string, flush until newline.
	  9: After seeing symbol char in state 3 (keep 1white after symchar)
	 10: After seeing whitespace in state 9 (keep white before symchar)
	 11: After seeing a symbol character in state 0 (eg a label definition)
	 -1: output string in out_string and go to the state in old_state
	 -2: flush text until a '*' '/' is seen, then go to state old_state
	  */

  /* I added states 9 and 10 because the MIPS ECOFF assembler uses
     constructs like ``.loc 1 20''.  This was turning into ``.loc
     120''.  States 9 and 10 ensure that a space is never dropped in
     between characters which could appear in a identifier.  Ian
     Taylor, ian@cygnus.com.

     I added state 11 so that something like "Lfoo add %r25,%r26,%r27" works
     correctly on the PA (and any other target where colons are optional).
     Jeff Law, law@cs.utah.edu.  */

  register int ch, ch2 = 0;
  int not_cpp_line = 0;

  switch (state)
    {
    case -1:
      ch = *out_string++;
      if (*out_string == 0)
	{
	  state = old_state;
	  old_state = 3;
	}
      return ch;

    case -2:
      for (;;)
	{
	  do
	    {
	      ch = (*get) ();
	    }
	  while (ch != EOF && ch != '\n' && ch != '*');
	  if (ch == '\n' || ch == EOF)
	    return ch;

	  /* At this point, ch must be a '*' */
	  while ((ch = (*get) ()) == '*')
	    {
	      ;
	    }
	  if (ch == EOF || ch == '/')
	    break;
	  (*unget) (ch);
	}
      state = old_state;
      return ' ';

    case 4:
      ch = (*get) ();
      if (ch == EOF || (ch >= '0' && ch <= '9'))
	return ch;
      else
	{
	  while (ch != EOF && IS_WHITESPACE (ch))
	    ch = (*get) ();
	  if (ch == '"')
	    {
	      (*unget) (ch);
	      out_string = "\n\t.appfile ";
	      old_state = 7;
	      state = -1;
	      return *out_string++;
	    }
	  else
	    {
	      while (ch != EOF && ch != '\n')
		ch = (*get) ();
	      state = 0;
	      return ch;
	    }
	}

    case 5:
      ch = (*get) ();
      if (lex[ch] == LEX_IS_STRINGQUOTE)
	{
	  state = old_state;
	  return ch;
	}
#ifndef NO_STRING_ESCAPES
      else if (ch == '\\')
	{
	  state = 6;
	  return ch;
	}
#endif
      else if (ch == EOF)
	{
	  as_warn ("End of file in string: inserted '\"'");
	  state = old_state;
	  (*unget) ('\n');
	  return '"';
	}
      else
	{
	  return ch;
	}

    case 6:
      state = 5;
      ch = (*get) ();
      switch (ch)
	{
	  /* Handle strings broken across lines, by turning '\n' into
	     '\\' and 'n'.  */
	case '\n':
	  (*unget) ('n');
	  add_newlines++;
	  return '\\';

	case '"':
	case '\\':
	case 'b':
	case 'f':
	case 'n':
	case 'r':
	case 't':
#ifdef BACKSLASH_V
	case 'v':
#endif /* BACKSLASH_V */
	case 'x':
	case 'X':
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	  break;
#if defined(IGNORE_NONSTANDARD_ESCAPES) | defined(ONLY_STANDARD_ESCAPES)
	default:
	  as_warn ("Unknown escape '\\%c' in string: Ignored", ch);
	  break;
#else /* ONLY_STANDARD_ESCAPES */
	default:
	  /* Accept \x as x for any x */
	  break;
#endif /* ONLY_STANDARD_ESCAPES */

	case EOF:
	  as_warn ("End of file in string: '\"' inserted");
	  return '"';
	}
      return ch;

    case 7:
      ch = (*get) ();
      state = 5;
      old_state = 8;
      return ch;

    case 8:
      do
	ch = (*get) ();
      while (ch != '\n');
      state = 0;
      return ch;
    }

  /* OK, we are somewhere in states 0 through 4 or 9 through 11 */

  /* flushchar: */
  ch = (*get) ();
recycle:
  if (ch == EOF)
    {
      if (state != 0)
	as_warn ("End of file not at end of a line: Newline inserted.");
      return ch;
    }

  switch (lex[ch])
    {
    case LEX_IS_WHITESPACE:
      do
	/* Preserve a single whitespace character at the beginning of
	   a line.  */
	if (state == 0)
	  {
	    state = 1;
	    return ch;
	  }
	else
	  ch = (*get) ();
      while (ch != EOF && IS_WHITESPACE (ch));
      if (ch == EOF)
	return ch;

      if (IS_COMMENT (ch)
	  || (state == 0 && IS_LINE_COMMENT (ch))
	  || ch == '/'
	  || IS_LINE_SEPARATOR (ch))
	{
	  /* cpp never outputs a leading space before the #, so try to
	     avoid being confused.  */
	  not_cpp_line = 1;
	  goto recycle;
	}
#ifdef MRI
      (*unget) (ch);		/* Put back */
      return ' ';		/* Always return one space at start of line */
#endif

      /* If we're in state 2 or 11, we've seen a non-white character
	 followed by whitespace.  If the next character is ':', this
	 is whitespace after a label name which we *must* ignore.  */
      if ((state == 2 || state == 11) && lex[ch] == LEX_IS_COLON)
	{
	  state = 1;
	  return ch;
	}

      switch (state)
	{
	case 0:
	  state++;
	  goto recycle;		/* Punted leading sp */
	case 1:
	  /* We can arrive here if we leave a leading whitespace character
	     at the beginning of a line.  */
	  goto recycle;
	case 2:
	  state = 3;
	  (*unget) (ch);
	  return ' ';		/* Sp after opco */
	case 3:
	  goto recycle;		/* Sp in operands */
	case 9:
	case 10:
	  state = 10;		/* Sp after symbol char */
	  goto recycle;
	case 11:
	  state = 1;
	  (*unget) (ch);
	  return ' ';		/* Sp after label definition.  */
	default:
	  BAD_CASE (state);
	}
      break;

    case LEX_IS_TWOCHAR_COMMENT_1ST:
      ch2 = (*get) ();
      if (ch2 != EOF && lex[ch2] == LEX_IS_TWOCHAR_COMMENT_2ND)
	{
	  for (;;)
	    {
	      do
		{
		  ch2 = (*get) ();
		  if (ch2 != EOF && IS_NEWLINE (ch2))
		    add_newlines++;
		}
	      while (ch2 != EOF &&
		     (lex[ch2] != LEX_IS_TWOCHAR_COMMENT_2ND));

	      while (ch2 != EOF &&
		     (lex[ch2] == LEX_IS_TWOCHAR_COMMENT_2ND))
		{
		  ch2 = (*get) ();
		}

	      if (ch2 == EOF
		  || lex[ch2] == LEX_IS_TWOCHAR_COMMENT_1ST)
		break;
	      (*unget) (ch);
	    }
	  if (ch2 == EOF)
	    as_warn ("End of file in multiline comment");

	  ch = ' ';
	  goto recycle;
	}
      else
	{
	  if (ch2 != EOF)
	    (*unget) (ch2);
	  if (state == 9 || state == 10)
	    state = 3;
	  return ch;
	}
      break;

    case LEX_IS_STRINGQUOTE:
      if (state == 9 || state == 10)
	old_state = 3;
      else
	old_state = state;
      state = 5;
      return ch;
#ifndef MRI
#ifndef IEEE_STYLE
    case LEX_IS_ONECHAR_QUOTE:
      ch = (*get) ();
      if (ch == EOF)
	{
	  as_warn ("End-of-file after a one-character quote; \\000 inserted");
	  ch = 0;
	}
      if (ch == '\\')
	{
	  ch = (*get) ();
	  ch = process_escape (ch);
	}
      sprintf (out_buf, "%d", (int) (unsigned char) ch);


      /* None of these 'x constants for us.  We want 'x'.  */
      if ((ch = (*get) ()) != '\'')
	{
#ifdef REQUIRE_CHAR_CLOSE_QUOTE
	  as_warn ("Missing close quote: (assumed)");
#else
	  (*unget) (ch);
#endif
	}
      if (strlen (out_buf) == 1)
	{
	  return out_buf[0];
	}
      if (state == 9 || state == 10)
	old_state = 3;
      else
	old_state = state;
      state = -1;
      out_string = out_buf;
      return *out_string++;
#endif
#endif
    case LEX_IS_COLON:
      if (state == 9 || state == 10)
	state = 3;
      else if (state != 3)
	state = 1;
      return ch;

    case LEX_IS_NEWLINE:
      /* Roll out a bunch of newlines from inside comments, etc.  */
      if (add_newlines)
	{
	  --add_newlines;
	  (*unget) (ch);
	}
      /* fall thru into... */

    case LEX_IS_LINE_SEPARATOR:
      state = 0;
      return ch;

    case LEX_IS_LINE_COMMENT_START:
      if (state == 0)		/* Only comment at start of line.  */
	{
	  /* FIXME-someday: The two character comment stuff was badly
	     thought out.  On i386, we want '/' as line comment start
	     AND we want C style comments.  hence this hack.  The
	     whole lexical process should be reworked.  xoxorich.  */
	  if (ch == '/')
	    {
	      ch2 = (*get) ();
	      if (ch2 == '*')
		{
		  state = -2;
		  return (do_scrub_next_char (get, unget));
		}
	      else
		{
		  (*unget) (ch2);
		}
	    }			/* bad hack */

	  if (ch != '#')
	    not_cpp_line = 1;

	  do
	    ch = (*get) ();
	  while (ch != EOF && IS_WHITESPACE (ch));
	  if (ch == EOF)
	    {
	      as_warn ("EOF in comment:  Newline inserted");
	      return '\n';
	    }
	  if (ch < '0' || ch > '9' || not_cpp_line)
	    {
	      /* Non-numerics:  Eat whole comment line */
	      while (ch != EOF && !IS_NEWLINE (ch))
		ch = (*get) ();
	      if (ch == EOF)
		as_warn ("EOF in Comment: Newline inserted");
	      state = 0;
	      return '\n';
	    }
	  /* Numerics begin comment.  Perhaps CPP `# 123 "filename"' */
	  (*unget) (ch);
	  old_state = 4;
	  state = -1;
	  out_string = "\t.appline ";
	  return *out_string++;
	}

      /* We have a line comment character which is not at the start of
	 a line.  If this is also a normal comment character, fall
	 through.  Otherwise treat it as a default character.  */
      if (strchr (comment_chars, ch) == NULL)
	goto de_fault;
      /* Fall through.  */
    case LEX_IS_COMMENT_START:
      do
	ch = (*get) ();
      while (ch != EOF && !IS_NEWLINE (ch));
      if (ch == EOF)
	as_warn ("EOF in comment:  Newline inserted");
      state = 0;
      return '\n';

    case LEX_IS_SYMBOL_COMPONENT:
      if (state == 10)
	{
	  /* This is a symbol character following another symbol
	     character, with whitespace in between.  We skipped the
	     whitespace earlier, so output it now.  */
	  (*unget) (ch);
	  state = 3;
	  return ' ';
	}
      if (state == 3)
	state = 9;
      /* Fall through.  */
    default:
    de_fault:
      /* Some relatively `normal' character.  */
      if (state == 0)
	{
	  state = 11;		/* Now seeing label definition */
	  return ch;
	}
      else if (state == 1)
	{
	  state = 2;		/* Ditto */
	  return ch;
	}
      else if (state == 9)
	{
	  if (lex[ch] != LEX_IS_SYMBOL_COMPONENT)
	    state = 3;
	  return ch;
	}
      else if (state == 10)
	{
	  state = 3;
	  return ch;
	}
      else
	{
	  return ch;		/* Opcode or operands already */
	}
    }
  return -1;
}

#ifdef TEST

const char comment_chars[] = "|";
const char line_comment_chars[] = "#";

main ()
{
  int ch;

  app_begin ();
  while ((ch = do_scrub_next_char (stdin)) != EOF)
    putc (ch, stdout);
}

as_warn (str)
     char *str;
{
  fputs (str, stderr);
  putc ('\n', stderr);
}

#endif

/* end of app.c */
