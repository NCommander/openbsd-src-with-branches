/* info.c -- Display nodes of Info files in multiple windows.
   $Id: info.c,v 1.18 1998/02/27 21:37:27 karl Exp $

   Copyright (C) 1993, 96, 97, 98 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

   Written by Brian Fox (bfox@ai.mit.edu). */

#include "info.h"
#include "indices.h"
#include "dribble.h"
#include "getopt.h"
#if defined (HANDLE_MAN_PAGES)
#  include "man.h"
#endif /* HANDLE_MAN_PAGES */

/* The version numbers of this version of Info. */
int info_major_version = 2;
int info_minor_version = 18;

/* basename (argv[0]) */
static char *program_name = NULL;

/* Non-zero means search all indices for APROPOS_SEARCH_STRING. */
static int apropos_p = 0;

/* Variable containing the string to search for when apropos_p is non-zero. */
static char *apropos_search_string = (char *)NULL;

/* Non-zero means search all indices for INDEX_SEARCH_STRING.  Unlike
   apropos, this puts the user at the node, running info. */
static int index_search_p = 0;

/* Variable containing the string to search for when index_search_p is
   non-zero. */ 
static char *index_search_string = (char *)NULL;

/* Non-zero means print version info only. */
static int print_version_p = 0;

/* Non-zero means print a short description of the options. */
static int print_help_p = 0;

/* Array of the names of nodes that the user specified with "--node" on the
   command line. */
static char **user_nodenames = (char **)NULL;
static int user_nodenames_index = 0;
static int user_nodenames_slots = 0;

/* String specifying the first file to load.  This string can only be set
   by the user specifying "--file" on the command line. */
static char *user_filename = (char *)NULL;

/* String specifying the name of the file to dump nodes to.  This value is
   filled if the user speficies "--output" on the command line. */
static char *user_output_filename = (char *)NULL;

/* Non-zero indicates that when "--output" is specified, all of the menu
   items of the specified nodes (and their subnodes as well) should be
   dumped in the order encountered.  This basically can print a book. */
int dump_subnodes = 0;

/* Structure describing the options that Info accepts.  We pass this structure
   to getopt_long ().  If you add or otherwise change this structure, you must
   also change the string which follows it. */
#define APROPOS_OPTION 1
#define DRIBBLE_OPTION 2
#define RESTORE_OPTION 3
#define IDXSRCH_OPTION 4
static struct option long_options[] = {
  { "apropos", 1, 0, APROPOS_OPTION },
  { "directory", 1, 0, 'd' },
  { "node", 1, 0, 'n' },
  { "file", 1, 0, 'f' },
  { "subnodes", 0, &dump_subnodes, 1 },
  { "output", 1, 0, 'o' },
  { "help", 0, &print_help_p, 1 },
  { "version", 0, &print_version_p, 1 },
  { "dribble", 1, 0, DRIBBLE_OPTION },
  { "restore", 1, 0, RESTORE_OPTION },
  { "index-search", 1, 0, IDXSRCH_OPTION },
  {NULL, 0, NULL, 0}
};

/* String describing the shorthand versions of the long options found above. */
static char *short_options = "d:n:f:o:s";

/* When non-zero, the Info window system has been initialized. */
int info_windows_initialized_p = 0;

/* Some "forward" declarations. */
static void info_short_help (), remember_info_program_name ();


/* **************************************************************** */
/*                                                                  */
/*                Main Entry Point to the Info Program              */
/*                                                                  */
/* **************************************************************** */

int
main (argc, argv)
     int argc;
     char **argv;
{
  int getopt_long_index;        /* Index returned by getopt_long (). */
  NODE *initial_node;           /* First node loaded by Info. */

  remember_info_program_name (argv[0]);

#ifdef HAVE_SETLOCALE
  /* Set locale via LC_ALL.  */
  setlocale (LC_ALL, "");
#endif

  /* Set the text message domain.  */
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);

  while (1)
    {
      int option_character;

      option_character = getopt_long
        (argc, argv, short_options, long_options, &getopt_long_index);

      /* getopt_long () returns EOF when there are no more long options. */
      if (option_character == EOF)
        break;

      /* If this is a long option, then get the short version of it. */
      if (option_character == 0 && long_options[getopt_long_index].flag == 0)
        option_character = long_options[getopt_long_index].val;

      /* Case on the option that we have received. */
      switch (option_character)
        {
        case 0:
          break;

          /* User wants to add a directory. */
        case 'd':
          info_add_path (optarg, INFOPATH_PREPEND);
          break;

          /* User is specifying a particular node. */
        case 'n':
          add_pointer_to_array (optarg, user_nodenames_index, user_nodenames,
                                user_nodenames_slots, 10, char *);
          break;

          /* User is specifying a particular Info file. */
        case 'f':
          if (user_filename)
            free (user_filename);

          user_filename = xstrdup (optarg);
          break;

          /* User is specifying the name of a file to output to. */
        case 'o':
          if (user_output_filename)
            free (user_output_filename);
          user_output_filename = xstrdup (optarg);
          break;

          /* User is specifying that she wishes to dump the subnodes of
             the node that she is dumping. */
        case 's':
          dump_subnodes = 1;
          break;

          /* User has specified a string to search all indices for. */
        case APROPOS_OPTION:
          apropos_p = 1;
          maybe_free (apropos_search_string);
          apropos_search_string = xstrdup (optarg);
          break;

          /* User has specified a dribble file to receive keystrokes. */
        case DRIBBLE_OPTION:
          close_dribble_file ();
          open_dribble_file (optarg);
          break;

          /* User has specified an alternate input stream. */
        case RESTORE_OPTION:
          info_set_input_from_file (optarg);
          break;

          /* User has specified a string to search all indices for. */
        case IDXSRCH_OPTION:
          index_search_p = 1;
          maybe_free (index_search_string);
          index_search_string = xstrdup (optarg);
          break;

        default:
          fprintf (stderr, _("Try --help for more information."));
          exit (1);
        }
    }

  /* If the output device is not a terminal, and no output filename has been
     specified, make user_output_filename be "-", so that the info is written
     to stdout, and turn on the dumping of subnodes. */
  if ((!isatty (fileno (stdout))) && (user_output_filename == (char *)NULL))
    {
      user_output_filename = xstrdup ("-");
      dump_subnodes = 1;
    }

  /* If the user specified --version, then show the version and exit. */
  if (print_version_p)
    {
      printf ("%s (GNU %s %s) %s\n", program_name, PACKAGE, VERSION, 
              version_string ());
      printf (_("Copyright (C) %s Free Software Foundation, Inc.\n\
There is NO warranty.  You may redistribute this software\n\
under the terms of the GNU General Public License.\n\
For more information about these matters, see the files named COPYING.\n"),
		  "1998");
      exit (0);
    }

  /* If the `--help' option was present, show the help and exit. */
  if (print_help_p)
    {
      info_short_help ();
      exit (0);
    }
  
  /* If the user hasn't specified a path for Info files, default it.
     Lowest priority is our messy hardwired list in filesys.h.
     Then comes the user's INFODIR from the Makefile.
     Highest priority is the environment variable, if set.  */
  if (!infopath)
    {
      char *path_from_env = getenv ("INFOPATH");

      if (path_from_env)
        {
          unsigned len = strlen (path_from_env);
          /* Trailing : on INFOPATH means insert the default path.  */
          if (len && path_from_env[len - 1] == ':')
            {
              path_from_env[len - 1] = 0;
              info_add_path (DEFAULT_INFOPATH, INFOPATH_PREPEND);
            }
#ifdef INFODIR /* from the Makefile */
          info_add_path (INFODIR, INFOPATH_PREPEND);
#endif
          info_add_path (path_from_env, INFOPATH_PREPEND);
        }
      else
        {
          info_add_path (DEFAULT_INFOPATH, INFOPATH_PREPEND);
#ifdef INFODIR /* from the Makefile */
         info_add_path (INFODIR, INFOPATH_PREPEND);
#endif
        }
    }

  /* If the user specified a particular filename, add the path of that
     file to the contents of INFOPATH. */
  if (user_filename)
    {
      char *directory_name = xstrdup (user_filename);
      char *temp = filename_non_directory (directory_name);

      if (temp != directory_name)
        {
          *temp = 0;
          info_add_path (directory_name, INFOPATH_PREPEND);
        }

      free (directory_name);
    }

  /* If the user wants to search every known index for a given string,
     do that now, and report the results. */
  if (apropos_p)
    {
      info_apropos (apropos_search_string);
      exit (0);
    }

  /* Get the initial Info node.  It is either "(dir)Top", or what the user
     specifed with values in user_filename and user_nodenames. */
  initial_node = info_get_node (user_filename,
                                user_nodenames ? user_nodenames[0] : NULL);

  /* If we couldn't get the initial node, this user is in trouble. */
  if (!initial_node)
    {
      if (info_recent_file_error)
        info_error (info_recent_file_error);
      else
        info_error
          (CANT_FIND_NODE, user_nodenames ? user_nodenames[0] : "Top");
      exit (1);
    }

  /* Special cases for when the user specifies multiple nodes.  If we
     are dumping to an output file, dump all of the nodes specified.
     Otherwise, attempt to create enough windows to handle the nodes
     that this user wants displayed. */
  if (user_nodenames_index > 1)
    {
      free (initial_node);

      if (user_output_filename)
        dump_nodes_to_file
          (user_filename, user_nodenames, user_output_filename, dump_subnodes);
      else
        begin_multiple_window_info_session (user_filename, user_nodenames);

      exit (0);
    }

  /* If the user specified `--index-search=STRING', start the info
     session in the node corresponding to the first match. */
  if (index_search_p)
    {
      int status = 0;

      initialize_info_session (initial_node, 0);

      if (index_entry_exists (windows, index_search_string))
        {
          terminal_clear_screen ();
          terminal_prep_terminal ();
          display_update_display (windows);
          info_last_executed_command = (VFunction *)NULL;

          do_info_index_search (windows, 0, index_search_string);

          info_read_and_dispatch ();

          terminal_unprep_terminal ();

          /* On program exit, leave the cursor at the bottom of the
             window, and restore the terminal IO. */
          terminal_goto_xy (0, screenheight - 1);
          terminal_clear_to_eol ();
          fflush (stdout);
        }
      else
        {
          fputs (_("no entries found\n"), stderr);
          status = 2;
        }

      close_dribble_file (); 
      exit (status);
    }

  /* If there are arguments remaining, they are the names of menu items
     in sequential info files starting from the first one loaded.  That
     file name is either "dir", or the contents of user_filename if one
     was specified. */
  while (optind != argc)
    {
      REFERENCE **menu;
      REFERENCE *entry;
      NODE *node;
      char *arg;
      static char *first_arg = (char *)NULL;

      /* Remember the name of the menu entry we want. */
      arg = argv[optind++];

      if (!first_arg)
        first_arg = arg;

      /* Build and return a list of the menu items in this node. */
      menu = info_menu_of_node (initial_node);

      /* If there wasn't a menu item in this node, stop here, but let
         the user continue to use Info.  Perhaps they wanted this node
         and didn't realize it. */
      if (!menu)
        {
#if defined (HANDLE_MAN_PAGES)
          if (first_arg == arg)
            {
              node = make_manpage_node (first_arg);
              if (node)
                goto maybe_got_node;
            }
#endif /* HANDLE_MAN_PAGES */
          begin_info_session_with_error
            (initial_node, _("There is no menu in this node."));
          exit (0);
        }

      /* Find the specified menu item. */
      entry = info_get_labeled_reference (arg, menu);

      /* If the item wasn't found, search the list sloppily.  Perhaps this
         user typed "buffer" when they really meant "Buffers". */
      if (!entry)
        {
          register int i;
          int best_guess = -1;

          for (i = 0; (entry = menu[i]); i++)
            {
              if (strcasecmp (entry->label, arg) == 0)
                break;
              else
                if (strncasecmp (entry->label, arg, strlen (arg)) == 0)
                  best_guess = i;
            }

          if (!entry && best_guess != -1)
            entry = menu[best_guess];
        }

      /* If we failed to find the reference, start Info with the current
         node anyway.  It is probably a misspelling. */
      if (!entry)
        {
          char *error_message = _("There is no menu item \"%s\" in this node.");

#if defined (HANDLE_MAN_PAGES)
          if (first_arg == arg)
            {
              node = make_manpage_node (first_arg);
              if (node)
                goto maybe_got_node;
            }
#endif /* HANDLE_MAN_PAGES */

          info_free_references (menu);

          /* If we were supposed to dump this node, complain. */
          if (user_output_filename)
            info_error (error_message, arg);
          else
            begin_info_session_with_error (initial_node, error_message, arg);

          exit (0);
        }

      /* We have found the reference that the user specified.  Clean it
         up a little bit. */
      if (!entry->filename)
        {
          if (initial_node->parent)
            entry->filename = xstrdup (initial_node->parent);
          else
            entry->filename = xstrdup (initial_node->filename);
        }

      /* Find this node.  If we can find it, then turn the initial_node
         into this one.  If we cannot find it, try using the label of the
         entry as a file (i.e., "(LABEL)Top").  Otherwise the Info file is
         malformed in some way, and we will just use the current value of
         initial node. */
      node = info_get_node (entry->filename, entry->nodename);

#if defined (HANDLE_MAN_PAGES)
          if ((first_arg == arg) && !node)
            {
              node = make_manpage_node (first_arg);
              if (node)
                goto maybe_got_node;
            }
#endif /* HANDLE_MAN_PAGES */

      if (!node && entry->nodename &&
          (strcmp (entry->label, entry->nodename) == 0))
        node = info_get_node (entry->label, "Top");

    maybe_got_node:
      if (node)
        {
          free (initial_node);
          initial_node = node;
          info_free_references (menu);
        }
      else
        {
          char *temp = xstrdup (entry->label);
          char *error_message;

          error_message = _("Unable to find the node referenced by \"%s\".");

          info_free_references (menu);

          /* If we were trying to dump the node, then give up.  Otherwise,
             start the session with an error message. */
          if (user_output_filename)
            info_error (error_message, temp);
          else
            begin_info_session_with_error (initial_node, error_message, temp);

          exit (0);
        }
    }

  /* If the user specified that this node should be output, then do that
     now.  Otherwise, start the Info session with this node. */
  if (user_output_filename)
    dump_node_to_file (initial_node, user_output_filename, dump_subnodes);
  else
    begin_info_session (initial_node);

  exit (0);
}

/* Return a string describing the current version of Info. */
char *
version_string ()
{
  static char *vstring = (char *)NULL;

  if (!vstring)
    {
      vstring = (char *)xmalloc (50);
      sprintf (vstring, "%d.%d", info_major_version, info_minor_version);
    }
  return (vstring);
}


/* Error handling.  */

static void
remember_info_program_name (fullpath)
     char *fullpath;
{
  char *filename;

  filename = filename_non_directory (fullpath);
  program_name = xstrdup (filename);
}

/* Non-zero if an error has been signalled. */
int info_error_was_printed = 0;

/* Non-zero means ring terminal bell on errors. */
int info_error_rings_bell_p = 1;

/* Print FORMAT with ARG1 and ARG2.  If the window system was initialized,
   then the message is printed in the echo area.  Otherwise, a message is
   output to stderr. */
void
info_error (format, arg1, arg2)
     char *format;
     void *arg1, *arg2;
{
  info_error_was_printed = 1;

  if (!info_windows_initialized_p || display_inhibited)
    {
      fprintf (stderr, "%s: ", program_name);
      fprintf (stderr, format, arg1, arg2);
      fprintf (stderr, "\n");
      fflush (stderr);
    }
  else
    {
      if (!echo_area_is_active)
        {
          if (info_error_rings_bell_p)
            terminal_ring_bell ();
          window_message_in_echo_area (format, arg1, arg2);
        }
      else
        {
          NODE *temp;

          temp = build_message_node (format, arg1, arg2);
          if (info_error_rings_bell_p)
            terminal_ring_bell ();
          inform_in_echo_area (temp->contents);
          free (temp->contents);
          free (temp);
        }
    }
}

/* Produce a scaled down description of the available options to Info. */
static void
info_short_help ()
{
  printf (_("\
Usage: %s [OPTION]... [INFO-FILE [MENU-ITEM...]]\n\
\n\
Read documentation in Info format.\n\
For more complete documentation on how to use Info, run `info info options'.\n\
\n\
Options:\n\
--directory DIR              add DIR to INFOPATH.\n\
--dribble FILENAME           remember user keystrokes in FILENAME.\n\
--file FILENAME              specify Info file to visit.\n\
--node NODENAME              specify nodes in first visited Info file.\n\
--output FILENAME            output selected nodes to FILENAME.\n\
--restore FILENAME           read initial keystrokes from FILENAME.\n\
--subnodes                   recursively output menu items.\n\
--help                       display this help and exit.\n\
--version                    display version information and exit.\n\
\n\
The first argument, if present, is the name of the Info file to read.\n\
Any remaining arguments are treated as the names of menu\n\
items in the initial node visited.  For example, `info emacs buffers'\n\
moves to the node `buffers' in the info file `emacs'.\n\
\n\
Email bug reports to bug-texinfo@gnu.org."), program_name);

  exit (0);
}
