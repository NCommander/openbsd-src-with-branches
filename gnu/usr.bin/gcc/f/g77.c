/* G77 preliminary semantic processing for the compiler driver.
   Copyright (C) 1993, 1994, 1995 Free Software Foundation, Inc.
   Contributed by Brendan Kehoe (brendan@cygnus.com), with significant
   modifications for GNU Fortran by James Craig Burley (burley@gnu.ai.mit.edu).

This file is part of GNU Fortran.

GNU Fortran is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Fortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Fortran; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* This program is a wrapper to the main `gcc' driver.  The generic
   goal of this program is to be basically identical to gcc (in that
   it faithfully passes all of the original arguments to gcc) but,
   unless explicitly overridden by the user in certain ways, ensure
   that the needs of the language supported by this wrapper are met.

   For GNU Fortran (g77), we do the following to the argument list
   before passing it to `gcc':

   1.  Put `-xf77' or `-xf77-cpp-input' before each list of foo.f or foo.F
       source files and put `-xnone' after that list, if necessary.
       This shouldn't normally be necessary, but it is done in case
       gcc.c normally treats .f/.F files as, say, to be compiled by f2c.

   2.  Make sure `-lf2c -lm' is at the end of the list.

   3.  Make sure each time `-lf2c' or `-lm' is seen, it forms
       part of the series `-lf2c -lm'.

   #1 is not done if `-xfoo' is in effect (where foo is not "none").
   #2 and #3 are not done if `-nostdlib' or any option that disables
   the linking phase is present, or if `-xfoo' is in effect.  Note that
   -v by itself disables linking.

   This program was originally made out of gcc/cp/g++.c, but the
   way it builds the new argument list was rewritten so it is much
   easier to maintain, improve the way it decides to add or not add
   extra arguments, etc.  And several improvements were made in the
   handling of arguments, primarily to make it more consistent with
   `gcc' itself.  */

#include "config.j"
#include <sys/types.h>
#include <errno.h>

#ifndef _WIN32
#include <sys/file.h>   /* May get R_OK, etc. on some systems.  */
#else
#include <process.h>
#endif

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdio.h>

#ifndef R_OK
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

/* Defined to the name of the compiler; if using a cross compiler, the
   Makefile should compile this file with the proper name
   (e.g., "i386-aout-gcc").  */
#ifndef GCC_NAME
#define GCC_NAME "gcc"
#endif

/* On MSDOS, write temp files in current dir
   because there's no place else we can expect to use.  */
#ifdef __MSDOS__
#ifndef P_tmpdir
#define P_tmpdir "."
#endif
#ifndef R_OK
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif
#endif

/* Add prototype support.  */
#ifndef PROTO
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define PROTO(ARGS) ARGS
#else
#define PROTO(ARGS) ()
#endif
#endif

#ifndef VPROTO
#ifdef __STDC__
#define PVPROTO(ARGS)		ARGS
#define VPROTO(ARGS)		ARGS
#define VA_START(va_list,var)	va_start(va_list,var)
#else
#define PVPROTO(ARGS)		()
#define VPROTO(ARGS)		(va_alist) va_dcl
#define VA_START(va_list,var)	va_start(va_list)
#endif
#endif

/* Define a generic NULL if one hasn't already been defined.  */

#ifndef NULL
#define NULL 0
#endif

/* Define O_RDONLY if the system hasn't defined it for us. */
#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#ifndef GENERIC_PTR
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define GENERIC_PTR void *
#else
#define GENERIC_PTR char *
#endif
#endif

#ifndef NULL_PTR
#define NULL_PTR ((GENERIC_PTR)0)
#endif

#ifdef USG
#define vfork fork
#endif /* USG */

/* On MSDOS, write temp files in current dir
   because there's no place else we can expect to use.  */
#ifdef __MSDOS__
#ifndef P_tmpdir
#define P_tmpdir "."
#endif
#endif

/* By default there is no special suffix for executables.  */
#ifndef EXECUTABLE_SUFFIX
#define EXECUTABLE_SUFFIX ""
#endif

/* By default, colon separates directories in a path.  */
#ifndef PATH_SEPARATOR
#define PATH_SEPARATOR ':'
#endif

#ifndef DIR_SEPARATOR
#define DIR_SEPARATOR '/'
#endif

static char dir_separator_str[] = {DIR_SEPARATOR, 0};

extern char *getenv ();

#ifndef errno
extern int errno;
#endif

extern int sys_nerr;
#ifndef HAVE_STRERROR
#if defined(bsd4_4)
extern const char *const sys_errlist[];
#else
extern char *sys_errlist[];
#endif
#else
extern char *strerror();
#endif

/* Name with which this program was invoked.  */
static char *programname;

/* argc, argv from main().  */
static int xargc;
static char **xargv;

/* The new argument list will be contained in these, though if identical
   to the original list, these will be == xargc, xargv.  */
static int newargc;
static char **newargv;

/* Options this driver needs to recognize, not just know how to
   skip over.  */
typedef enum
{
  OPTION_b,			/* Aka --prefix. */
  OPTION_B,			/* Aka --target. */
  OPTION_c,			/* Aka --compile. */
  OPTION_driver,		/* Wrapper-specific option. */
  OPTION_E,			/* Aka --preprocess. */
  OPTION_i,			/* -imacros, -include, -include-*. */
  OPTION_M,			/* Aka --dependencies. */
  OPTION_MM,			/* Aka --user-dependencies. */
  OPTION_nostdlib,		/* Aka --no-standard-libraries, or
				   -nodefaultlibs. */
  OPTION_P,			/* Aka --print-*-name. */
  OPTION_S,			/* Aka --assemble. */
  OPTION_v,			/* Aka --verbose. */
  OPTION_V,			/* Aka --use-version. */
  OPTION_x,			/* Aka --language. */
  OPTION_			/* Unrecognized or unimportant. */
} Option;

/* THE FOLLOWING COMES STRAIGHT FROM gcc-2.7.0/gcc.c:  */

/* This defines which switch letters take arguments.  */

#ifndef SWITCH_TAKES_ARG
#define SWITCH_TAKES_ARG(CHAR)      \
  ((CHAR) == 'D' || (CHAR) == 'U' || (CHAR) == 'o' \
   || (CHAR) == 'e' || (CHAR) == 'T' || (CHAR) == 'u' \
   || (CHAR) == 'I' || (CHAR) == 'm' || (CHAR) == 'x' \
   || (CHAR) == 'L' || (CHAR) == 'A')
#endif

/* This defines which multi-letter switches take arguments.  */

#define DEFAULT_WORD_SWITCH_TAKES_ARG(STR)		\
 (!strcmp (STR, "Tdata") || !strcmp (STR, "Ttext")	\
  || !strcmp (STR, "Tbss") || !strcmp (STR, "include")	\
  || !strcmp (STR, "imacros") || !strcmp (STR, "aux-info") \
  || !strcmp (STR, "idirafter") || !strcmp (STR, "iprefix") \
  || !strcmp (STR, "iwithprefix") || !strcmp (STR, "iwithprefixbefore") \
  || !strcmp (STR, "isystem"))

#ifndef WORD_SWITCH_TAKES_ARG
#define WORD_SWITCH_TAKES_ARG(STR) DEFAULT_WORD_SWITCH_TAKES_ARG (STR)
#endif

/* This is the common prefix we use to make temp file names.
   It is chosen once for each run of this program.
   It is substituted into a spec by %g.
   Thus, all temp file names contain this prefix.
   In practice, all temp file names start with this prefix.

   This prefix comes from the envvar TMPDIR if it is defined;
   otherwise, from the P_tmpdir macro if that is defined;
   otherwise, in /usr/tmp or /tmp.  */

static char *temp_filename;

/* Length of the prefix.  */

static int temp_filename_length;

/* END OF STUFF FROM gcc-2.7.0/gcc.c.  */

char *
my_strerror(e)
     int e;
{

#ifdef HAVE_STRERROR
  return strerror(e);

#else

  static char buffer[30];
  if (!e)
    return "";

  if (e > 0 && e < sys_nerr)
    return sys_errlist[e];

  sprintf (buffer, "Unknown error %d", e);
  return buffer;
#endif
}

#ifdef HAVE_VPRINTF
/* Output an error message and exit */

static void
fatal VPROTO((char *format, ...))
{
#ifndef __STDC__
  char *format;
#endif
  va_list ap;

  VA_START (ap, format);

#ifndef __STDC__
  format = va_arg (ap, char*);
#endif

  fprintf (stderr, "%s: ", programname);
  vfprintf (stderr, format, ap);
  va_end (ap);
  fprintf (stderr, "\n");
#if 0
  /* XXX Not needed for g77 driver.  */
  delete_temp_files ();
#endif
  exit (1);
}

static void
error VPROTO((char *format, ...))
{
#ifndef __STDC__
  char *format;
#endif
  va_list ap;

  VA_START (ap, format);

#ifndef __STDC__
  format = va_arg (ap, char*);
#endif

  fprintf (stderr, "%s: ", programname);
  vfprintf (stderr, format, ap);
  va_end (ap);

  fprintf (stderr, "\n");
}

#else /* not HAVE_VPRINTF */

static void
error (msg, arg1, arg2)
     char *msg, *arg1, *arg2;
{
  fprintf (stderr, "%s: ", programname);
  fprintf (stderr, msg, arg1, arg2);
  fprintf (stderr, "\n");
}

static void
fatal (msg, arg1, arg2)
     char *msg, *arg1, *arg2;
{
  error (msg, arg1, arg2);
#if 0
  /* XXX Not needed for g77 driver.  */
  delete_temp_files ();
#endif
  exit (1);
}

#endif /* not HAVE_VPRINTF */

/* More 'friendly' abort that prints the line and file.
   config.h can #define abort fancy_abort if you like that sort of thing.  */

void
fancy_abort ()
{
  fatal ("Internal g77 abort.");
}

char *
xmalloc (size)
     unsigned size;
{
  register char *value = (char *) malloc (size);
  if (value == 0)
    fatal ("virtual memory exhausted");
  return value;
}

static char *
concat (s1, s2)
     char *s1, *s2;
{
  int len1 = strlen (s1);
  int len2 = strlen (s2);
  char *result = xmalloc (len1 + len2 + 1);

  strcpy (result, s1);
  strcpy (result + len1, s2);
  *(result + len1 + len2) = 0;

  return result;
}

static char *
concat3 (s1, s2, s3)
     char *s1, *s2, *s3;
{
  return concat (concat (s1, s2), s3);
}

static char *
concat4 (s1, s2, s3, s4)
     char *s1, *s2, *s3, *s4;
{
  return concat (concat (s1, s2), concat (s3, s4));
}

static char *
concat6 (s1, s2, s3, s4, s5, s6)
     char *s1, *s2, *s3, *s4, *s5, *s6;
{
  return concat3 (concat (s1, s2), concat (s3, s4), concat (s5, s6));
}

static void
pfatal_with_name (name)
     char *name;
{
  char *s;

  if (errno < sys_nerr)
    s = concat3 ("%s: ", sys_errlist[errno], "");
  else
    s = "cannot open %s";
  fatal (s, name);
}

/* Compute a string to use as the base of all temporary file names.
   It is substituted for %g.  */

static char *
choose_temp_base_try (try, base)
     char *try;
     char *base;
{
  char *rv;
  if (base)
    rv = base;
  else if (try == (char *)0)
    rv = 0;
  else if (access (try, R_OK | W_OK) != 0)
    rv = 0;
  else
    rv = try;
  return rv;
}

static void
choose_temp_base ()
{
  char *base = 0;
  int len;

  base = choose_temp_base_try (getenv ("TMPDIR"), base);
  base = choose_temp_base_try (getenv ("TMP"), base);
  base = choose_temp_base_try (getenv ("TEMP"), base);

#ifdef P_tmpdir
  base = choose_temp_base_try (P_tmpdir, base);
#endif

  base = choose_temp_base_try (concat4 (dir_separator_str, "usr", 
                                        dir_separator_str, "tmp"), 
                                base);
  base = choose_temp_base_try (concat (dir_separator_str, "tmp"), base);
 
  /* If all else fails, use the current directory! */
  if (base == (char *)0)
    base = concat (".", dir_separator_str);

  len = strlen (base);
  temp_filename = xmalloc (len + strlen (concat (dir_separator_str, 
                                                 "gfXXXXXX")) + 1);
  strcpy (temp_filename, base);
  if (len > 0 && temp_filename[len-1] != '/'
      && temp_filename[len-1] != DIR_SEPARATOR)
    temp_filename[len++] = DIR_SEPARATOR;
  strcpy (temp_filename + len, "gfXXXXXX");

  mktemp (temp_filename);
  temp_filename_length = strlen (temp_filename);
  if (temp_filename_length == 0)
    abort ();
}

#ifdef __MSDOS__
static void
perror_exec (name)
     char *name;
{
  char *s;

  if (errno < sys_nerr)
    s = concat3 ("installation problem, cannot exec %s: ",
		 my_strerror( errno ), "");
  else
    s = "installation problem, cannot exec %s";
  error (s, name);
}

/* This is almost exactly what's in gcc.c:pexecute for MSDOS.  */
void
run_dos (program, argv)
     char *program;
     char *argv[];
{
  char *scmd, *rf;
  FILE *argfile;
  int i;

  choose_temp_base (); /* not in gcc.c */

  scmd = (char *) malloc (strlen (program) + strlen (temp_filename) + 10);
  rf = scmd + strlen (program) + 6;
  sprintf (scmd, "%s.exe @%s.gp", program, temp_filename);

  argfile = fopen (rf, "w");
  if (argfile == 0)
    pfatal_with_name (rf);

  for (i=1; argv[i]; i++)
    {
      char *cp;
      for (cp = argv[i]; *cp; cp++)
	{
	  if (*cp == '"' || *cp == '\'' || *cp == '\\' || isspace (*cp))
	    fputc ('\\', argfile);
	  fputc (*cp, argfile);
	}
      fputc ('\n', argfile);
    }
  fclose (argfile);

  i = system (scmd);

  remove (rf);

  if (i == -1)
    perror_exec (program);
}
#endif /* __MSDOS__ */

/* This structure describes one mapping.  */
struct option_map
{
  /* The long option's name.  */
  char *name;
  /* The equivalent short option.  */
  char *equivalent;
  /* Argument info.  A string of flag chars; NULL equals no options.
     a => argument required.
     o => argument optional.
     j => join argument to equivalent, making one word.
     * => require other text after NAME as an argument.  */
  char *arg_info;
};

/* This is the table of mappings.  Mappings are tried sequentially
   for each option encountered; the first one that matches, wins.  */

struct option_map option_map[] =
 {
   {"--all-warnings", "-Wall", 0},
   {"--ansi", "-ansi", 0},
   {"--assemble", "-S", 0},
   {"--assert", "-A", "a"},
   {"--comments", "-C", 0},
   {"--compile", "-c", 0},
   {"--debug", "-g", "oj"},
   {"--define-macro", "-D", "a"},
   {"--dependencies", "-M", 0},
   {"--driver", "", 0},		/* Wrapper-specific. */
   {"--dump", "-d", "a"},
   {"--dumpbase", "-dumpbase", "a"},
   {"--entry", "-e", 0},
   {"--extra-warnings", "-W", 0},
   {"--for-assembler", "-Wa", "a"},
   {"--for-linker", "-Xlinker", "a"},
   {"--force-link", "-u", "a"},
   {"--imacros", "-imacros", "a"},
   {"--include", "-include", "a"},
   {"--include-barrier", "-I-", 0},
   {"--include-directory", "-I", "a"},
   {"--include-directory-after", "-idirafter", "a"},
   {"--include-prefix", "-iprefix", "a"},
   {"--include-with-prefix", "-iwithprefix", "a"},
   {"--include-with-prefix-before", "-iwithprefixbefore", "a"},
   {"--include-with-prefix-after", "-iwithprefix", "a"},
   {"--language", "-x", "a"},
   {"--library-directory", "-L", "a"},
   {"--machine", "-m", "aj"},
   {"--machine-", "-m", "*j"},
   {"--no-line-commands", "-P", 0},
   {"--no-precompiled-includes", "-noprecomp", 0},
   {"--no-standard-includes", "-nostdinc", 0},
   {"--no-standard-libraries", "-nostdlib", 0},
   {"--no-warnings", "-w", 0},
   {"--optimize", "-O", "oj"},
   {"--output", "-o", "a"},
   {"--pedantic", "-pedantic", 0},
   {"--pedantic-errors", "-pedantic-errors", 0},
   {"--pipe", "-pipe", 0},
   {"--prefix", "-B", "a"},
   {"--preprocess", "-E", 0},
   {"--print-file-name", "-print-file-name=", "aj"},
   {"--print-libgcc-file-name", "-print-libgcc-file-name", 0},
   {"--print-missing-file-dependencies", "-MG", 0},
   {"--print-multi-lib", "-print-multi-lib", 0},
   {"--print-multi-directory", "-print-multi-directory", 0},
   {"--print-prog-name", "-print-prog-name=", "aj"},
   {"--profile", "-p", 0},
   {"--profile-blocks", "-a", 0},
   {"--quiet", "-q", 0},
   {"--save-temps", "-save-temps", 0},
   {"--shared", "-shared", 0},
   {"--silent", "-q", 0},
   {"--static", "-static", 0},
   {"--symbolic", "-symbolic", 0},
   {"--target", "-b", "a"},
   {"--trace-includes", "-H", 0},
   {"--traditional", "-traditional", 0},
   {"--traditional-cpp", "-traditional-cpp", 0},
   {"--trigraphs", "-trigraphs", 0},
   {"--undefine-macro", "-U", "a"},
   {"--use-version", "-V", "a"},
   {"--user-dependencies", "-MM", 0},
   {"--verbose", "-v", 0},
   {"--version", "-dumpversion", 0},
   {"--warn-", "-W", "*j"},
   {"--write-dependencies", "-MD", 0},
   {"--write-user-dependencies", "-MMD", 0},
   {"--", "-f", "*j"}
 };

/* Compares --options that take one arg.  */

static int
opteq (xskip, xarg, opt, name)
     int *xskip;
     char **xarg;
     char *opt;
     char *name;
{
  int optlen;
  int namelen;
  int complen;
  int i;
  int cmp = strcmp (opt, name);
  int skip = 1;
  char *arg = NULL;

  if (cmp == 0)
    {
      /* Easy, a straight match.  */
      *xskip = skip;
      *xarg = arg;
      return cmp;
    }

  optlen = strlen (opt);

  for (i = 0; i < sizeof (option_map) / sizeof (option_map[0]); ++i)
    {
      char *arginfo;
      int j;

      arginfo = option_map[i].arg_info;
      if (arginfo == NULL)
	arginfo = "";

      namelen = strlen (option_map[i].name);
      complen = optlen > namelen ? namelen : optlen;

      if (strncmp (opt, option_map[i].name, complen) == 0)
	{
	  if (optlen < namelen)
	    {
	      for (j = i + 1;
		   j < sizeof (option_map) / sizeof (option_map[0]);
		   ++j)
		if ((strlen (option_map[j].name) >= optlen)
		    && (strncmp (opt, option_map[j].name, optlen) == 0))
		  fatal ("Ambiguous abbreviation `%s'", opt);
	    }

	  if (optlen > namelen)
	    {
	      if (opt[namelen] == '=')
		{
		  skip = 0;
		  arg = opt + namelen + 1;
		}
	      else if (index (arginfo, '*') != 0)
		;
	      else
		continue;
	    }
	  else if (index (arginfo, '*') != 0)
	    fatal ("Incomplete `%s' option", option_map[i].name);

	  if (strcmp (name, option_map[i].name) != 0)
	    return 1;		/* Not what is being looked for. */

	  *xskip = skip;
	  *xarg = arg;
	  return 0;
	}
    }

  return 1;
}

/* Assumes text[0] == '-'.  Returns number of argv items that belong to
   (and follow) this one, an option id for options important to the
   caller, and a pointer to the first char of the arg, if embedded (else
   returns NULL, meaning no arg or it's the next argv).  */

static void
lookup_option (xopt, xskip, xarg, text)
     Option *xopt;
     int *xskip;
     char **xarg;
     char *text;
{
  Option opt = OPTION_;
  int skip = -1;
  char *arg = NULL;

  if ((skip = SWITCH_TAKES_ARG (text[1])) > (text[2] != '\0'))
    skip -= (text[2] != '\0');	/* Usually one of "DUoeTuImLA". */
  else if (text[1] == 'B')
    opt = OPTION_B, skip = (text[2] == '\0'), arg = text + 2;
  else if (text[1] == 'b')
    opt = OPTION_b, skip = (text[2] == '\0'), arg = text + 2;
  else if ((text[1] == 'c') && (text[2] == '\0'))
    opt = OPTION_c, skip = 0;
  else if ((text[1] == 'E') && (text[2] == '\0'))
    opt = OPTION_E, skip = 0;
  else if (text[1] == 'i')
    opt = OPTION_i, skip = 0;
  else if ((text[1] == 'S') && (text[2] == '\0'))
    opt = OPTION_S, skip = 0;
  else if (text[1] == 'V')
    opt = OPTION_V, skip = (text[2] == '\0');
  else if ((text[1] == 'v') && (text[2] == '\0'))
    opt = OPTION_v, skip = 0;
  else if (text[1] == 'x')
    opt = OPTION_x, skip = (text[2] == '\0'), arg = text + 2;
  else
    {
      if ((skip = WORD_SWITCH_TAKES_ARG (text + 1)) != 0)
	/* Usually one of "Tdata", "Ttext", "Tbss", "include",
	   "imacros", "aux-info", "idirafter", "iprefix",
	   "iwithprefix", "iwithprefixbefore", "isystem".  */
	;
      else if (strcmp (text, "--assemble") == 0)
	opt = OPTION_S;
      else if (strcmp (text, "--compile") == 0)
	opt = OPTION_c;
      else if (opteq (&skip, &arg, text, "--driver") == 0)
	opt = OPTION_driver;
      else if ((opteq (&skip, &arg, text, "--imacros") == 0)
	       || (opteq (&skip, &arg, text, "--include") == 0)
	       || (opteq (&skip, &arg, text, "--include-directory-after") == 0)
	       || (opteq (&skip, &arg, text, "--include-prefix") == 0)
	       || (opteq (&skip, &arg, text, "--include-with-prefix") == 0)
	       || (opteq (&skip, &arg, text, "--include-with-prefix-before") == 0)
	       || (opteq (&skip, &arg, text, "--include-with-prefix-after") == 0))
	opt = OPTION_i;
      else if (opteq (&skip, &arg, text, "--language") == 0)
	opt = OPTION_x;
      else if ((strcmp (text, "-M") == 0)
	       || (strcmp (text, "--dependencies") == 0))
	opt = OPTION_M;
      else if ((strcmp (text, "-MM") == 0)
	       || (strcmp (text, "--user-dependencies") == 0))
	opt = OPTION_MM;
      else if (opteq (&skip, &arg, text, "--prefix") == 0)
	opt = OPTION_B;
      else if (strcmp (text, "--preprocess") == 0)
	opt = OPTION_E;
      else if ((opteq (&skip, &arg, text, "--print-file-name") == 0)
	       || (strcmp (text, "--print-libgcc-file-name") == 0)
	       || (strcmp (text, "--print-multi-lib") == 0)
	       || (strcmp (text, "--print-multi-directory") == 0)
	       || (opteq (&skip, &arg, text, "--print-prog-name") == 0))
	opt = OPTION_P;
      else if ((strcmp (text, "-nostdlib") == 0)
	       || (strcmp (text, "--no-standard-libraries") == 0)
	       || (strcmp (text, "-nodefaultlibs") == 0))
	opt = OPTION_nostdlib;
      else if (opteq (&skip, &arg, text, "--use-version") == 0)
	opt = OPTION_V;
      else if (strcmp (text, "--verbose") == 0)
	opt = OPTION_v;
      else if (strcmp (text, "-Xlinker") == 0)
	skip = 1;
      else if ((opteq (&skip, &arg, text, "--assert") == 0)
	       || (opteq (&skip, &arg, text, "--define-macro") == 0)
	       || (opteq (&skip, &arg, text, "--dump") == 0)
	       || (opteq (&skip, &arg, text, "--dumpbase") == 0)
	       || (opteq (&skip, &arg, text, "--for-assembler") == 0)
	       || (opteq (&skip, &arg, text, "--for-linker") == 0)
	       || (opteq (&skip, &arg, text, "--force-link") == 0)
	       || (opteq (&skip, &arg, text, "--library-directory") == 0)
	       || (opteq (&skip, &arg, text, "--machine") == 0)
	       || (opteq (&skip, &arg, text, "--output") == 0)
	       || (opteq (&skip, &arg, text, "--target") == 0)
	       || (opteq (&skip, &arg, text, "--undefine-macro") == 0))
	;
      else
	skip = 0;
    }

  if (xopt != NULL)
    *xopt = opt;
  if (xskip != NULL)
    *xskip = skip;
  if (xarg != NULL)
    *xarg = arg;
}

static void
append_arg (arg)
    char *arg;
{
  static int newargsize;

#if 0
  fprintf (stderr, "`%s'\n", arg);
#endif

  if ((newargv == xargv)
      && (arg == xargv[newargc]))
    {
      ++newargc;
      return;			/* Nothing new here. */
    }

  if (newargv == xargv)
    {				/* Make new arglist. */
      int i;

      newargsize = (xargc << 2) + 20;
      newargv = (char **) malloc (newargsize * sizeof (char *));

      /* Copy what has been done so far.  */
      for (i = 0; i < newargc; ++i)
	newargv[i] = xargv[i];
    }

  if (newargc == newargsize)
    fatal ("overflowed output arg list for `%s'", arg);
  newargv[newargc++] = arg;
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  register int i = 0;
  register char *p;
  int verbose = 0;
  Option opt;
  int skip;
  char *arg;

  /* This will be NULL if we encounter a situation where we should not
     link in libf2c.  */
  char *library = "-lf2c";

  /* This will become 0 if anything other than -v and kin (like -V)
     is seen, meaning the user is trying to accomplish something.
     If it remains nonzero, the user wants version info, so add stuff to
     the command line to make gcc invoke all the appropriate phases
     to get all the version info.  */
  int add_version_magic = 1;

  /* The name of the compiler we will want to run---by default, it
     will be the definition of `GCC_NAME', e.g., `gcc'.  */
  char *gcc = GCC_NAME;

  /* 0 => -xnone in effect on input/output
     1 => -xfoo in effect on input/output
     2 => -xnone in effect on input, -xf77 on output
     3 => -xnone in effect on input, -xf77-cpp-input on output.  */
  int saw_speclang = 0;

  /* 0 => initial/reset state
     1 => last arg was -l<library>
     2 => last two args were -l<library> -lm.  */
  int saw_library = 0;

  /* Initialize for append_arg().  */
  xargc = argc;
  newargv = xargv = argv;
  newargc = 0;

  append_arg (argv[0]);

  p = argv[0] + strlen (argv[0]);
  while (p != argv[0] && p[-1] != '/')
    --p;
  programname = p;

  if (argc == 1)
    fatal ("No input files specified.\n");

#ifndef __MSDOS__
  /* We do a little magic to find out where the main gcc executable
     is.  If they ran us as /usr/local/bin/g77, then we will look
     for /usr/local/bin/gcc; similarly, if they just ran us as `g77',
     we'll just look for `gcc'.  */
  if (p != argv[0])
    {
      *--p = '\0';
      gcc = (char *) malloc ((strlen (argv[0]) + 1 + strlen (GCC_NAME) + 1)
			     * sizeof (char));
      sprintf (gcc, "%s/%s", argv[0], GCC_NAME);
    }
#endif

  /* First pass through arglist.

     If -nostdlib or a "turn-off-linking" option is anywhere in the
     command line, don't do any library-option processing (except
     relating to -x).  Also, if -v is specified, but no other options
     that do anything special (allowing -V version, etc.), remember
     to add special stuff to make gcc command actually invoke all
     the different phases of the compilation process so all the version
     numbers can be seen.

     Also, here is where all problems with missing arguments to options
     are caught.  If this loop is exited normally, it means all options
     have the appropriate number of arguments as far as the rest of this
     program is concerned.  */

  for (i = 1; i < argc; ++i)
    {
      if (argv[i][0] != '-')
	{
	  add_version_magic = 0;
	  continue;
	}

      lookup_option (&opt, &skip, NULL, argv[i]);

      switch (opt)
	{
	case OPTION_nostdlib:
	case OPTION_c:
	case OPTION_S:
	case OPTION_E:
	case OPTION_M:
	case OPTION_MM:
	  /* These options disable linking entirely or linking of the
	     standard libraries.  */
	  library = NULL;
	  add_version_magic = 0;
	  break;

	case OPTION_v:
	  verbose = 1;
	  break;

	case OPTION_b:
	case OPTION_B:
	case OPTION_driver:
	case OPTION_i:
	case OPTION_V:
	  /* These options are useful in conjunction with -v to get
	     appropriate version info.  */
	  break;

	default:
	  add_version_magic = 0;
	  break;
	}

      /* This is the one place we check for missing arguments in the
	 program.  */

      if (i + skip < argc)
	i += skip;
      else
	fatal ("argument to `%s' missing\n", argv[i]);
    }

  /* If only -v and related options (like -V), don't link the standard
     libraries.  */

  if (add_version_magic)
    library = NULL;

  /* Second pass through arglist, transforming arguments as appropriate.  */

  for (i = 1; i < argc; ++i)
    {
      if (argv[i][0] == '\0')
	append_arg (argv[i]);	/* Interesting.  Just append as is. */

      else if ((argv[i][0] == '-') && (argv[i][1] != 'l'))
	{
	  /* Not a filename or library. */

	  if (saw_library == 1)	/* -l<library>. */
	    append_arg ("-lm");
	  saw_library = 0;

	  lookup_option (&opt, &skip, &arg, argv[i]);

	  if (argv[i][1] == '\0')
	    append_arg (argv[i]);	/* "-" == Standard input. */

	  else if (opt == OPTION_x)
	    {
	      /* Track input language. */
	      char *lang;

	      append_arg (argv[i]);

	      if (arg == NULL)
		lang = argv[i+1];
	      else
		lang = arg;

	      saw_speclang = (strcmp (lang, "none") != 0);
	    }
	  else if (opt == OPTION_driver)
	    {
	      if (arg == NULL)
		gcc = argv[i+1];
	      else
		gcc = arg;
	      i += skip;
	      continue;		/* Don't append args to new list. */
	    }
	  append_arg (argv[i]);
	  for (; skip != 0; --skip)
	    append_arg (argv[++i]);
	}
      else
	{			/* A filename/library, not an option. */
	  int len;
	  int want_speclang;

	  /* Here, always append the arg _after_ other stuff, possibly.  */

	  if (saw_speclang == 1)
	    saw_library = 0;	/* -xfoo currently active. */
	  /* Put -xf77 and -xnone around list of filenames ending in
	     .F or .f, but don't include other filenames or libraries
	     in that list.  */
	  else if ((argv[i][0] != '-')	/* Not a library. */
		   && (len = strlen (argv[i])) > 2
		   && ((argv[i][len - 1] == 'F')
		       || (argv[i][len - 1] == 'f'))
		   && argv[i][len - 2] == '.')
	    {			/* filename.f or filename.F. */
	      if (saw_library == 1)	/* -l<library>. */
		append_arg ("-lm");
	      saw_library = 0;
	      want_speclang = (argv[i][len - 1] == 'F') + 2;

	      if (saw_speclang != want_speclang)
		{
		  if (want_speclang == 2)
		    append_arg ("-xf77");
		  else
		    append_arg ("-xf77-cpp-input");
		  saw_speclang = want_speclang;
		}
	    }
	  else
	    {			/* -lfoo or "alien" filename. */
	      if (saw_speclang)
		append_arg ("-xnone");
	      saw_speclang = 0;

	      if (strcmp (argv[i], "-lm") == 0
		  || strcmp (argv[i], "-lmath") == 0)
		{
		  if (saw_library == 1)
		    saw_library = 2;	/* -l<library> -lm. */
		  else if (library)
		    {
		      append_arg (library);
		      saw_library = 2;	/* -l<library> -lm. */
		    }
		}
	      else if ((library != NULL)
		       && (strcmp (argv[i], library) == 0))
		saw_library = 1;	/* -l<library>. */
	      else
		{		/* "Alien" library or filename. */
		  if (saw_library == 1)
		    append_arg ("-lm");
		  saw_library = 0;
		}
	    }
	  append_arg (argv[i]);
	}
    }

  /* Add -lf2c -lm as necessary.  */

  if (library)
    {				/* Doing a link and no -nostdlib. */
      if (saw_speclang)
	append_arg ("-xnone");
      switch (saw_library)
	{
	case 0:
	  append_arg (library);
	case 1:
	  append_arg ("-lm");
	default:
	  break;
	}
    }
  else if (verbose && add_version_magic)
    {
      choose_temp_base ();

      append_arg ("-fnull-version");
      append_arg ("-o");
      append_arg (temp_filename);
      append_arg ("-xf77-cpp-input");
      append_arg ("/dev/null");
      append_arg ("-xnone");
      if (library)
	{
	  append_arg (library);
	  append_arg ("-lm");
	}
    }

  append_arg (NULL);
  --newargc;			/* Don't count null arg at end. */

  newargv[0] = gcc;		/* This is safe even if newargv == xargv. */

  if (verbose)
    {
#if 0
      if (newargv == xargv)
	fprintf (stderr, "[Original:]");
#endif

      for (i = 0; i < newargc; i++)
	fprintf (stderr, " %s", newargv[i]);
      fprintf (stderr, "\n");
    }
#if !defined(OS2) && !defined (_WIN32)
#ifdef __MSDOS__
  run_dos (gcc, newargv);
#else /* !__MSDOS__ */
  if (execvp (gcc, newargv) < 0)
    pfatal_with_name (gcc);
#endif /* __MSDOS__ */
#else /* OS2 or _WIN32 */
  if (spawnvp (1, gcc, newargv) < 0)
    pfatal_with_name (gcc);
#endif

  return 0;
}
