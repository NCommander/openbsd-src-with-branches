/* infodoc.c -- Functions which build documentation nodes.
   $Id: infodoc.c,v 1.23 1999/09/25 16:10:04 karl Exp $

   Copyright (C) 1993, 97, 98, 99 Free Software Foundation, Inc.

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

/* HELP_NODE_GETS_REGENERATED is always defined now that keys may get
   rebound, or other changes in the help text may occur.  */
#define HELP_NODE_GETS_REGENERATED 1

/* **************************************************************** */
/*                                                                  */
/*                        Info Help Windows                         */
/*                                                                  */
/* **************************************************************** */

/* The name of the node used in the help window. */
static char *info_help_nodename = "*Info Help*";

/* A node containing printed key bindings and their documentation. */
static NODE *internal_info_help_node = (NODE *)NULL;

/* A pointer to the contents of the help node. */
static char *internal_info_help_node_contents = (char *)NULL;

/* The static text which appears in the internal info help node. */
static char *info_internal_help_text[] = {
  N_("Basic Commands in Info Windows\n"),
  N_("******************************\n"),
  "\n",
  N_("  %-10s  Quit this help.\n"),
  N_("  %-10s  Quit Info altogether.\n"),
  N_("  %-10s  Invoke the Info tutorial.\n"),
  "\n",
  N_("Moving within a node:\n"),
  N_("---------------------\n"),
  N_("  %-10s  Scroll forward a page.\n"),
  N_("  %-10s  Scroll backward a page.\n"),
  N_("  %-10s  Go to the beginning of this node.\n"),
  N_("  %-10s  Go to the end of this node.\n"),
  N_("  %-10s  Scroll forward 1 line.\n"),
  N_("  %-10s  Scroll backward 1 line.\n"),
  "\n",
  N_("Selecting other nodes:\n"),
  N_("----------------------\n"),
  N_("  %-10s  Move to the `next' node of this node.\n"),
  N_("  %-10s  Move to the `previous' node of this node.\n"),
  N_("  %-10s  Move `up' from this node.\n"),
  N_("  %-10s  Pick menu item specified by name.\n"),
  N_("              Picking a menu item causes another node to be selected.\n"),
  N_("  %-10s  Follow a cross reference.  Reads name of reference.\n"),
  N_("  %-10s  Move to the last node seen in this window.\n"),
  N_("  %-10s  Skip to next hypertext link within this node.\n"),
  N_("  %-10s  Follow the hypertext link under cursor.\n"),
  N_("  %-10s  Move to the `directory' node.  Equivalent to `g (DIR)'.\n"),
  N_("  %-10s  Move to the Top node.  Equivalent to `g Top'.\n"),
  "\n",
  N_("Other commands:\n"),
  N_("---------------\n"),
  N_("  %-10s  Pick first ... ninth item in node's menu.\n"),
  N_("  %-10s  Pick last item in node's menu.\n"),
  N_("  %-10s  Search for a specified string in the index entries of this Info\n"),
  N_("              file, and select the node referenced by the first entry found.\n"),
  N_("  %-10s  Move to node specified by name.\n"),
  N_("              You may include a filename as well, as in (FILENAME)NODENAME.\n"),
  N_("  %-10s  Search forward through this Info file for a specified string,\n"),
  N_("              and select the node in which the next occurrence is found.\n"),
  N_("  %-10s  Search backward in this Info file for a specified string,\n"),
  N_("              and select the node in which the next occurrence is found.\n"),
  NULL
};

static char *info_help_keys_text[][2] = {
  { "", "" },
  { "", "" },
  { "", "" },
  { "CTRL-x 0", "CTRL-x 0" },
  { "q", "q" },
  { "h", "ESC h" },
  { "", "" },
  { "", "" },
  { "", "" },
  { "SPC", "SPC" },
  { "DEL", "b" },
  { "b", "ESC b" },
  { "e", "ESC e" },
  { "ESC 1 SPC", "RET" },
  { "ESC 1 DEL", "y" },
  { "", "" },
  { "", "" },
  { "", "" },
  { "n", "CTRL-x n" },
  { "p", "CTRL-x p" },
  { "u", "CTRL-x u" },
  { "m", "ESC m" },
  { "", "" },
  { "f", "ESC f" },
  { "l", "l" },
  { "TAB", "TAB" },
  { "RET", "CTRL-x RET" },
  { "d", "ESC d" },
  { "t", "ESC t" },
  { "", "" },
  { "", "" },
  { "", "" },
  { "1-9", "ESC 1-9" },
  { "0", "ESC 0" },
  { "i", "CTRL-x i" },
  { "", "" },
  { "g", "CTRL-x g" },
  { "", "" },
  { "s", "/" },
  { "", "" },
  { "ESC - s", "?" },
  { "", "" },
  NULL
};

static char *where_is (), *where_is_internal ();

void
dump_map_to_message_buffer (prefix, map)
     char *prefix;
     Keymap map;
{
  register int i;

  for (i = 0; i < 256; i++)
    {
      if (map[i].type == ISKMAP)
        {
          char *new_prefix, *keyname;

          keyname = pretty_keyname (i);
          new_prefix = (char *)
            xmalloc (3 + strlen (prefix) + strlen (keyname));
          sprintf (new_prefix, "%s%s%s ", prefix, *prefix ? " " : "", keyname);

          dump_map_to_message_buffer (new_prefix, (Keymap)map[i].function);
          free (new_prefix);
        }
      else if (map[i].function)
        {
          register int last;
          char *doc, *name;

          doc = function_documentation (map[i].function);
          name = function_name (map[i].function);

          if (!*doc)
            continue;

          /* Find out if there is a series of identical functions, as in
             ea_insert (). */
          for (last = i + 1; last < 256; last++)
            if ((map[last].type != ISFUNC) ||
                (map[last].function != map[i].function))
              break;

          if (last - 1 != i)
            {
              printf_to_message_buffer
                ("%s%s .. ", prefix, pretty_keyname (i));
              printf_to_message_buffer
                ("%s%s\t", prefix, pretty_keyname (last - 1));
              i = last - 1;
            }
          else
            printf_to_message_buffer ("%s%s\t", prefix, pretty_keyname (i));

#if defined (NAMED_FUNCTIONS)
          /* Print the name of the function, and some padding before the
             documentation string is printed. */
          {
            int length_so_far;
            int desired_doc_start = 40; /* Must be multiple of 8. */

            printf_to_message_buffer ("(%s)", name);
            length_so_far = message_buffer_length_this_line ();

            if ((desired_doc_start + strlen (doc)) >= the_screen->width)
              printf_to_message_buffer ("\n     ");
            else
              {
                while (length_so_far < desired_doc_start)
                  {
                    printf_to_message_buffer ("\t");
                    length_so_far += character_width ('\t', length_so_far);
                  }
              }
          }
#endif /* NAMED_FUNCTIONS */
          printf_to_message_buffer ("%s\n", doc);
        }
    }
}

/* How to create internal_info_help_node.  HELP_IS_ONLY_WINDOW_P says
   whether we're going to end up in a second (or more) window of our
   own, or whether there's only one window and we're going to usurp it.
   This determines how to quit the help window.  Maybe we should just
   make q do the right thing in both cases.  */

static void
create_internal_info_help_node (help_is_only_window_p)
     int help_is_only_window_p;
{
  register int i;
  NODE *node;
  char *contents = NULL;

#ifndef HELP_NODE_GETS_REGENERATED
  if (internal_info_help_node_contents)
    contents = internal_info_help_node_contents;
#endif /* !HELP_NODE_GETS_REGENERATED */

  if (!contents)
    {
      int printed_one_mx = 0;

      initialize_message_buffer ();

      for (i = 0; info_internal_help_text[i]; i++)
        {
          /* Don't translate blank lines, gettext outputs the po file
             header in that case.  We want a blank line.  */
          char *msg = *(info_internal_help_text[i])
                      ? _(info_internal_help_text[i])
                      : info_internal_help_text[i];
          char *key = info_help_keys_text[i][vi_keys_p];
          
          /* If we have only one window (because the window size was too
             small to split it), CTRL-x 0 doesn't work to `quit' help.  */
          if (STREQ (key, "CTRL-x 0") && help_is_only_window_p)
            key = "l";

          printf_to_message_buffer (msg, key);
        }

      printf_to_message_buffer ("---------------------\n\n");
      printf_to_message_buffer (_("The current search path is:\n"));
      printf_to_message_buffer ("  %s\n", infopath);
      printf_to_message_buffer ("---------------------\n\n");
      printf_to_message_buffer (_("Commands available in Info windows:\n\n"));
      dump_map_to_message_buffer ("", info_keymap);
      printf_to_message_buffer ("---------------------\n\n");
      printf_to_message_buffer (_("Commands available in the echo area:\n\n"));
      dump_map_to_message_buffer ("", echo_area_keymap);

#if defined (NAMED_FUNCTIONS)
      /* Get a list of the M-x commands which have no keystroke equivs. */
      for (i = 0; function_doc_array[i].func; i++)
        {
          VFunction *func = function_doc_array[i].func;

          if ((!where_is_internal (info_keymap, func)) &&
              (!where_is_internal (echo_area_keymap, func)))
            {
              if (!printed_one_mx)
                {
                  printf_to_message_buffer ("---------------------\n\n");
                  printf_to_message_buffer
                    (_("The following commands can only be invoked via M-x:\n\n"));
                  printed_one_mx = 1;
                }

              printf_to_message_buffer
                ("M-x %s\n     %s\n",
                 function_doc_array[i].func_name,
                 replace_in_documentation (strlen (function_doc_array[i].doc)
                                           == 0
					   ? function_doc_array[i].doc
					   : _(function_doc_array[i].doc)));

            }
        }

      if (printed_one_mx)
        printf_to_message_buffer ("\n");
#endif /* NAMED_FUNCTIONS */

      printf_to_message_buffer
        ("%s", replace_in_documentation
         (_("--- Use `\\[history-node]' or `\\[kill-node]' to exit ---\n")));
      node = message_buffer_to_node ();
      internal_info_help_node_contents = node->contents;
    }
  else
    {
      /* We already had the right contents, so simply use them. */
      node = build_message_node ("", 0, 0);
      free (node->contents);
      node->contents = contents;
      node->nodelen = 1 + strlen (contents);
    }

  internal_info_help_node = node;

  /* Do not GC this node's contents.  It never changes, and we never need
     to delete it once it is made.  If you change some things (such as
     placing information about dynamic variables in the help text) then
     you will need to allow the contents to be gc'd, and you will have to
     arrange to always regenerate the help node. */
#if defined (HELP_NODE_GETS_REGENERATED)
  add_gcable_pointer (internal_info_help_node->contents);
#endif

  name_internal_node (internal_info_help_node, info_help_nodename);

  /* Even though this is an internal node, we don't want the window
     system to treat it specially.  So we turn off the internalness
     of it here. */
  internal_info_help_node->flags &= ~N_IsInternal;
}

/* Return a window which is the window showing help in this Info. */

/* If the eligible window's height is >= this, split it to make the help
   window.  Otherwise display the help window in the current window.  */
#define HELP_SPLIT_SIZE 24

static WINDOW *
info_find_or_create_help_window ()
{
  int help_is_only_window_p;
  WINDOW *eligible = NULL;
  WINDOW *help_window = get_window_of_node (internal_info_help_node);

  /* If we couldn't find the help window, then make it. */
  if (!help_window)
    {
      WINDOW *window;
      int max = 0;

      for (window = windows; window; window = window->next)
        {
          if (window->height > max)
            {
              max = window->height;
              eligible = window;
            }
        }

      if (!eligible)
        return NULL;
    }
#ifndef HELP_NODE_GETS_REGENERATED
  else
    /* help window is static, just return it.  */
    return help_window;
#endif /* not HELP_NODE_GETS_REGENERATED */

  /* Make sure that we have a node containing the help text.  The
     argument is false if help will be the only window (so l must be used
     to quit help), true if help will be one of several visible windows
     (so CTRL-x 0 must be used to quit help).  */
  help_is_only_window_p
     = (help_window && !windows->next
        || !help_window && eligible->height < HELP_SPLIT_SIZE);
  create_internal_info_help_node (help_is_only_window_p);

  /* Either use the existing window to display the help node, or create
     a new window if there was no existing help window. */
  if (!help_window)
    { /* Split the largest window into 2 windows, and show the help text
         in that window. */
      if (eligible->height >= HELP_SPLIT_SIZE)
        {
          active_window = eligible;
          help_window = window_make_window (internal_info_help_node);
        }
      else
        {
          set_remembered_pagetop_and_point (active_window);
          window_set_node_of_window (active_window, internal_info_help_node);
          help_window = active_window;
        }
    }
  else
    { /* Case where help node always gets regenerated, and we have an
         existing window in which to place the node. */
      if (active_window != help_window)
        {
          set_remembered_pagetop_and_point (active_window);
          active_window = help_window;
        }
      window_set_node_of_window (active_window, internal_info_help_node);
    }
  remember_window_and_node (help_window, help_window->node);
  return help_window;
}

/* Create or move to the help window. */
DECLARE_INFO_COMMAND (info_get_help_window, _("Display help message"))
{
  WINDOW *help_window;

  help_window = info_find_or_create_help_window ();
  if (help_window)
    {
      active_window = help_window;
      active_window->flags |= W_UpdateWindow;
    }
  else
    {
      info_error (msg_cant_make_help);
    }
}

/* Show the Info help node.  This means that the "info" file is installed
   where it can easily be found on your system. */
DECLARE_INFO_COMMAND (info_get_info_help_node, _("Visit Info node `(info)Help'"))
{
  NODE *node;
  char *nodename;

  /* If there is a window on the screen showing the node "(info)Help" or
     the node "(info)Help-Small-Screen", simply select that window. */
  {
    WINDOW *win;

    for (win = windows; win; win = win->next)
      {
        if (win->node && win->node->filename &&
            (strcasecmp
             (filename_non_directory (win->node->filename), "info") == 0) &&
            ((strcmp (win->node->nodename, "Help") == 0) ||
             (strcmp (win->node->nodename, "Help-Small-Screen") == 0)))
          {
            active_window = win;
            return;
          }
      }
  }

  /* If the current window is small, show the small screen help. */
  if (active_window->height < 24)
    nodename = "Help-Small-Screen";
  else
    nodename = "Help";

  /* Try to get the info file for Info. */
  node = info_get_node ("Info", nodename);

  if (!node)
    {
      if (info_recent_file_error)
        info_error (info_recent_file_error);
      else
        info_error (msg_cant_file_node, "Info", nodename);
    }
  else
    {
      /* If the current window is very large (greater than 45 lines),
         then split it and show the help node in another window.
         Otherwise, use the current window. */

      if (active_window->height > 45)
        active_window = window_make_window (node);
      else
        {
          set_remembered_pagetop_and_point (active_window);
          window_set_node_of_window (active_window, node);
        }

      remember_window_and_node (active_window, node);
    }
}

/* **************************************************************** */
/*                                                                  */
/*                   Groveling Info Keymaps and Docs                */
/*                                                                  */
/* **************************************************************** */

/* Return the documentation associated with the Info command FUNCTION. */
char *
function_documentation (function)
     VFunction *function;
{
  register int i;

  for (i = 0; function_doc_array[i].func; i++)
    if (function == function_doc_array[i].func)
      break;

  return replace_in_documentation ((strlen (function_doc_array[i].doc) == 0)
                                   ? function_doc_array[i].doc
                                   : _(function_doc_array[i].doc));
}

#if defined (NAMED_FUNCTIONS)
/* Return the user-visible name of the function associated with the
   Info command FUNCTION. */
char *
function_name (function)

     VFunction *function;
{
  register int i;

  for (i = 0; function_doc_array[i].func; i++)
    if (function == function_doc_array[i].func)
      break;

  return (function_doc_array[i].func_name);
}

/* Return a pointer to the function named NAME. */
VFunction *
named_function (name)
     char *name;
{
  register int i;

  for (i = 0; function_doc_array[i].func; i++)
    if (strcmp (function_doc_array[i].func_name, name) == 0)
      break;

  return (function_doc_array[i].func);
}
#endif /* NAMED_FUNCTIONS */

/* Return the documentation associated with KEY in MAP. */
char *
key_documentation (key, map)
     char key;
     Keymap map;
{
  VFunction *function = map[key].function;

  if (function)
    return (function_documentation (function));
  else
    return ((char *)NULL);
}

DECLARE_INFO_COMMAND (describe_key, _("Print documentation for KEY"))
{
  char keyname[50];
  int keyname_index = 0;
  unsigned char keystroke;
  char *rep;
  Keymap map;

  keyname[0] = 0;
  map = window->keymap;

  for (;;)
    {
      message_in_echo_area (_("Describe key: %s"), keyname);
      keystroke = info_get_input_char ();
      unmessage_in_echo_area ();

      if (Meta_p (keystroke))
        {
          if (map[ESC].type != ISKMAP)
            {
              window_message_in_echo_area
              (_("ESC %s is undefined."), pretty_keyname (UnMeta (keystroke)));
              return;
            }

          strcpy (keyname + keyname_index, "ESC ");
          keyname_index = strlen (keyname);
          keystroke = UnMeta (keystroke);
          map = (Keymap)map[ESC].function;
        }

      /* Add the printed representation of KEYSTROKE to our keyname. */
      rep = pretty_keyname (keystroke);
      strcpy (keyname + keyname_index, rep);
      keyname_index = strlen (keyname);

      if (map[keystroke].function == (VFunction *)NULL)
        {
          message_in_echo_area (_("%s is undefined."), keyname);
          return;
        }
      else if (map[keystroke].type == ISKMAP)
        {
          map = (Keymap)map[keystroke].function;
          strcat (keyname, " ");
          keyname_index = strlen (keyname);
          continue;
        }
      else
        {
          char *message, *fundoc, *funname = "";

#if defined (NAMED_FUNCTIONS)
          funname = function_name (map[keystroke].function);
#endif /* NAMED_FUNCTIONS */

          fundoc = function_documentation (map[keystroke].function);

          message = (char *)xmalloc
            (10 + strlen (keyname) + strlen (fundoc) + strlen (funname));

#if defined (NAMED_FUNCTIONS)
          sprintf (message, "%s (%s): %s.", keyname, funname, fundoc);
#else
          sprintf (message, _("%s is defined to %s."), keyname, fundoc);
#endif /* !NAMED_FUNCTIONS */

          window_message_in_echo_area ("%s", message);
          free (message);
          break;
        }
    }
}

/* How to get the pretty printable name of a character. */
static char rep_buffer[30];

char *
pretty_keyname (key)
     unsigned char key;
{
  char *rep;

  if (Meta_p (key))
    {
      char temp[20];

      rep = pretty_keyname (UnMeta (key));

      sprintf (temp, "ESC %s", rep);
      strcpy (rep_buffer, temp);
      rep = rep_buffer;
    }
  else if (Control_p (key))
    {
      switch (key)
        {
        case '\n': rep = "LFD"; break;
        case '\t': rep = "TAB"; break;
        case '\r': rep = "RET"; break;
        case ESC:  rep = "ESC"; break;

        default:
          sprintf (rep_buffer, "C-%c", UnControl (key));
          rep = rep_buffer;
        }
    }
  else
    {
      switch (key)
        {
        case ' ': rep = "SPC"; break;
        case DEL: rep = "DEL"; break;
        default:
          rep_buffer[0] = key;
          rep_buffer[1] = '\0';
          rep = rep_buffer;
        }
    }
  return (rep);
}

/* Replace the names of functions with the key that invokes them. */
char *
replace_in_documentation (string)
     char *string;
{
  register int i, start, next;
  static char *result = (char *)NULL;

  maybe_free (result);
  result = (char *)xmalloc (1 + strlen (string));

  i = next = start = 0;

  /* Skip to the beginning of a replaceable function. */
  for (i = start; string[i]; i++)
    {
      /* Is this the start of a replaceable function name? */
      if (string[i] == '\\' && string[i + 1] == '[')
        {
          char *fun_name, *rep;
          VFunction *function;

          /* Copy in the old text. */
          strncpy (result + next, string + start, i - start);
          next += (i - start);
          start = i + 2;

          /* Move to the end of the function name. */
          for (i = start; string[i] && (string[i] != ']'); i++);

          fun_name = (char *)xmalloc (1 + i - start);
          strncpy (fun_name, string + start, i - start);
          fun_name[i - start] = '\0';

          /* Find a key which invokes this function in the info_keymap. */
          function = named_function (fun_name);

          /* If the internal documentation string fails, there is a
             serious problem with the associated command's documentation.
             We croak so that it can be fixed immediately. */
          if (!function)
            abort ();

          rep = where_is (info_keymap, function);
          strcpy (result + next, rep);
          next = strlen (result);

          start = i;
          if (string[i])
            start++;
        }
    }
  strcpy (result + next, string + start);
  return (result);
}

/* Return a string of characters which could be typed from the keymap
   MAP to invoke FUNCTION. */
static char *where_is_rep = (char *)NULL;
static int where_is_rep_index = 0;
static int where_is_rep_size = 0;

static char *
where_is (map, function)
     Keymap map;
     VFunction *function;
{
  char *rep;

  if (!where_is_rep_size)
    where_is_rep = (char *)xmalloc (where_is_rep_size = 100);
  where_is_rep_index = 0;

  rep = where_is_internal (map, function);

  /* If it couldn't be found, return "M-x Foo". */
  if (!rep)
    {
      char *name;

      name = function_name (function);

      if (name)
        sprintf (where_is_rep, "M-x %s", name);

      rep = where_is_rep;
    }
  return (rep);
}

/* Return the printed rep of FUNCTION as found in MAP, or NULL. */
static char *
where_is_internal (map, function)
     Keymap map;
     VFunction *function;
{
  register int i;

  /* If the function is directly invokable in MAP, return the representation
     of that keystroke. */
  for (i = 0; i < 256; i++)
    if ((map[i].type == ISFUNC) && map[i].function == function)
      {
        sprintf (where_is_rep + where_is_rep_index, "%s", pretty_keyname (i));
        return (where_is_rep);
      }

  /* Okay, search subsequent maps for this function. */
  for (i = 0; i < 256; i++)
    {
      if (map[i].type == ISKMAP)
        {
          int saved_index = where_is_rep_index;
          char *rep;

          sprintf (where_is_rep + where_is_rep_index, "%s ",
                   pretty_keyname (i));

          where_is_rep_index = strlen (where_is_rep);
          rep = where_is_internal ((Keymap)map[i].function, function);

          if (rep)
            return (where_is_rep);

          where_is_rep_index = saved_index;
        }
    }

  return NULL;
}

extern char *read_function_name ();

DECLARE_INFO_COMMAND (info_where_is,
   _("Show what to type to execute a given command"))
{
  char *command_name;

  command_name = read_function_name (_("Where is command: "), window);

  if (!command_name)
    {
      info_abort_key (active_window, count, key);
      return;
    }

  if (*command_name)
    {
      VFunction *function;

      function = named_function (command_name);

      if (function)
        {
          char *location;

          location = where_is (active_window->keymap, function);

          if (!location)
            {
              info_error (_("`%s' is not on any keys"), command_name);
            }
          else
            {
              if (strncmp (location, "M-x ", 4) == 0)
                window_message_in_echo_area
                  (_("%s can only be invoked via %s."), command_name, location);
              else
                window_message_in_echo_area
                  (_("%s can be invoked via %s."), command_name, location);
            }
        }
      else
        info_error (_("There is no function named `%s'"), command_name);
    }

  free (command_name);
}
