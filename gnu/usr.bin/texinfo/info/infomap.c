/* infomap.c -- Keymaps for Info.
   $Id: infomap.c,v 1.7 1997/07/31 20:37:32 karl Exp $

   Copyright (C) 1993, 97 Free Software Foundation, Inc.

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
#include "infomap.h"
#include "funs.h"
#include "terminal.h"

/* Return a new keymap which has all the uppercase letters mapped to run
   the function info_do_lowercase_version (). */
Keymap
keymap_make_keymap ()
{
  register int i;
  Keymap keymap;

  keymap = (Keymap)xmalloc (256 * sizeof (KEYMAP_ENTRY));

  for (i = 0; i < 256; i++)
    {
      keymap[i].type = ISFUNC;
      keymap[i].function = (VFunction *)NULL;
    }

  for (i = 'A'; i < ('Z' + 1); i++)
    {
      keymap[i].type = ISFUNC;
      keymap[i].function = info_do_lowercase_version;
    }

  return (keymap);
}

/* Return a new keymap which is a copy of MAP. */
Keymap
keymap_copy_keymap (map)
     Keymap map;
{
  register int i;
  Keymap keymap;

  keymap = keymap_make_keymap ();

  for (i = 0; i < 256; i++)
    {
      keymap[i].type = map[i].type;
      keymap[i].function = map[i].function;
    }
  return (keymap);
}

/* Free the keymap and it's descendents. */
void
keymap_discard_keymap (map)
     Keymap (map);
{
  register int i;

  if (!map)
    return;

  for (i = 0; i < 256; i++)
    {
      switch (map[i].type)
        {
        case ISFUNC:
          break;

        case ISKMAP:
          keymap_discard_keymap ((Keymap)map[i].function);
          break;

        }
    }
}

/* Conditionally bind key sequence. */
int
keymap_bind_keyseq (map, keyseq, keyentry)
     Keymap map;
     const unsigned char *keyseq;
     KEYMAP_ENTRY *keyentry;
{
  register Keymap m = map;
  register const unsigned char *s = keyseq;
  register int c;

  if (s == NULL || *s == '\0') return 0;

  while ((c = *s++) != '\0')
    {
      switch (m[c].type)
        {
        case ISFUNC:
          if (!(m[c].function == NULL ||
                (m != map && m[c].function == info_do_lowercase_version)))
            return 0;

          if (*s != '\0')
            {
              m[c].type = ISKMAP;
              m[c].function = (VFunction *)keymap_make_keymap ();
            }
          break;

        case ISKMAP:
          if (*s == '\0')
            return 0;
          break;
        }
      if (*s != '\0')
        {
          m = (Keymap)m[c].function;
        }
      else
        {
          m[c] = *keyentry;
        }
    }

  return 1;
}

/* Initialize the standard info keymaps. */

Keymap info_keymap = (Keymap)NULL;
Keymap echo_area_keymap = (Keymap)NULL;

void
initialize_info_keymaps ()
{
  register int i;
  Keymap map;

  if (!info_keymap)
    {
      info_keymap = keymap_make_keymap ();
      info_keymap[ESC].type = ISKMAP;
      info_keymap[ESC].function = (VFunction *)keymap_make_keymap ();
      info_keymap[Control ('x')].type = ISKMAP;
      info_keymap[Control ('x')].function = (VFunction *)keymap_make_keymap ();
      echo_area_keymap = keymap_make_keymap ();
      echo_area_keymap[ESC].type = ISKMAP;
      echo_area_keymap[ESC].function = (VFunction *)keymap_make_keymap ();
      echo_area_keymap[Control ('x')].type = ISKMAP;
      echo_area_keymap[Control ('x')].function =
        (VFunction *)keymap_make_keymap ();
    }

  /* Bind numeric arg functions for both echo area and info window maps. */
  for (i = '0'; i < '9' + 1; i++)
    {
      ((Keymap) info_keymap[ESC].function)[i].function =
        ((Keymap) echo_area_keymap[ESC].function)[i].function =
          info_add_digit_to_numeric_arg;
    }
  ((Keymap) info_keymap[ESC].function)['-'].function =
    ((Keymap) echo_area_keymap[ESC].function)['-'].function =
      info_add_digit_to_numeric_arg;

  /* Bind the echo area routines. */
  map = echo_area_keymap;

  /* Bind the echo area insert routines. */
  for (i = 0; i < 160; i++)
    if (isprint (i))
      map[i].function = ea_insert;

  map[Control ('a')].function = ea_beg_of_line;
  map[Control ('b')].function = ea_backward;
  map[Control ('d')].function = ea_delete;
  map[Control ('e')].function = ea_end_of_line;
  map[Control ('f')].function = ea_forward;
  map[Control ('g')].function = ea_abort;
  map[Control ('h')].function = ea_rubout;
  map[Control ('k')].function = ea_kill_line;
  map[Control ('l')].function = info_redraw_display;
  map[Control ('q')].function = ea_quoted_insert;
  map[Control ('t')].function = ea_transpose_chars;
  map[Control ('u')].function = info_universal_argument;
  map[Control ('y')].function = ea_yank;

  map[LFD].function = ea_newline;
  map[RET].function = ea_newline;
  map[SPC].function = ea_complete;
  map[TAB].function = ea_complete;
  map['?'].function = ea_possible_completions;
  map[DEL].function = ea_rubout;

  /* Bind the echo area ESC keymap. */
  map = (Keymap)echo_area_keymap[ESC].function;

  map[Control ('g')].function = ea_abort;
  map[Control ('v')].function = ea_scroll_completions_window;
  map['b'].function = ea_backward_word;
  map['d'].function = ea_kill_word;
  map['f'].function = ea_forward_word;
#if defined (NAMED_FUNCTIONS)
  /* map['x'].function = info_execute_command; */
#endif /* NAMED_FUNCTIONS */
  map['y'].function = ea_yank_pop;
  map['?'].function = ea_possible_completions;
  map[TAB].function = ea_tab_insert;
  map[DEL].function = ea_backward_kill_word;

  /* Bind the echo area Control-x keymap. */
  map = (Keymap)echo_area_keymap[Control ('x')].function;

  map['o'].function = info_next_window;
  map[DEL].function = ea_backward_kill_line;

  /* Arrow key bindings for echo area keymaps.  It seems that some
     terminals do not match their termcap entries, so it's best to just
     define everything with both of the usual prefixes.  */
  map = echo_area_keymap;
  keymap_bind_keyseq (map, term_ku, &map[Control ('p')]); /* up */
  keymap_bind_keyseq (map, "\033OA", &map[Control ('p')]);
  keymap_bind_keyseq (map, "\033[A", &map[Control ('p')]);
  keymap_bind_keyseq (map, term_kd, &map[Control ('n')]); /* down */
  keymap_bind_keyseq (map, "\033OB", &map[Control ('n')]);
  keymap_bind_keyseq (map, "\033[B", &map[Control ('n')]);
  keymap_bind_keyseq (map, term_kr, &map[Control ('f')]); /* right */
  keymap_bind_keyseq (map, "\033OC", &map[Control ('f')]);
  keymap_bind_keyseq (map, "\033[C", &map[Control ('f')]);
  keymap_bind_keyseq (map, term_kl, &map[Control ('b')]); /* left */
  keymap_bind_keyseq (map, "\033OD", &map[Control ('b')]);
  keymap_bind_keyseq (map, "\033[D", &map[Control ('b')]);

  map = (Keymap)echo_area_keymap[ESC].function;
  keymap_bind_keyseq (map, term_kl, &map['b']); /* left */
  keymap_bind_keyseq (map, "\033OA", &map['b']);
  keymap_bind_keyseq (map, "\033[A", &map['b']);
  keymap_bind_keyseq (map, term_kr, &map['f']); /* right */
  keymap_bind_keyseq (map, "\033OB", &map['f']);
  keymap_bind_keyseq (map, "\033[B", &map['f']);

  /* Bind commands for Info window keymaps. */
  map = info_keymap;
  map[TAB].function = info_move_to_next_xref;
  map[LFD].function = info_select_reference_this_line;
  map[RET].function = info_select_reference_this_line;
  map[SPC].function = info_scroll_forward;
  map[Control ('a')].function = info_beginning_of_line;
  map[Control ('b')].function = info_backward_char;
  map[Control ('e')].function = info_end_of_line;
  map[Control ('f')].function = info_forward_char;
  map[Control ('g')].function = info_abort_key;
  map[Control ('h')].function = info_get_help_window;
  map[Control ('l')].function = info_redraw_display;
  map[Control ('n')].function = info_next_line;
  map[Control ('p')].function = info_prev_line;
  map[Control ('r')].function = isearch_backward;
  map[Control ('s')].function = isearch_forward;
  map[Control ('u')].function = info_universal_argument;
  map[Control ('v')].function = info_scroll_forward;
  map[','].function = info_next_index_match;

  for (i = '1'; i < '9' + 1; i++)
    map[i].function = info_menu_digit;
  map['0'].function = info_last_menu_item;

  map['<'].function = info_first_node;
  map['>'].function = info_last_node;
  map['?'].function = info_get_help_window;
  map['['].function = info_global_prev_node;
  map[']'].function = info_global_next_node;

  map['b'].function = info_beginning_of_node;
  map['d'].function = info_dir_node;
  map['e'].function = info_end_of_node;
  map['f'].function = info_xref_item;
  map['g'].function = info_goto_node;
  map['h'].function = info_get_info_help_node;
  map['i'].function = info_index_search;
  map['l'].function = info_history_node;
  map['m'].function = info_menu_item;
  map['n'].function = info_next_node;
  map['p'].function = info_prev_node;
  map['q'].function = info_quit;
  map['r'].function = info_xref_item;
  map['s'].function = info_search;
  map['t'].function = info_top_node;
  map['u'].function = info_up_node;
  map[DEL].function = info_scroll_backward;

  /* Bind members in the ESC map for Info windows. */
  map = (Keymap)info_keymap[ESC].function;
  map[Control ('f')].function = info_show_footnotes;
  map[Control ('g')].function = info_abort_key;
  map[TAB].function = info_move_to_prev_xref;
  map[Control ('v')].function = info_scroll_other_window;
  map['<'].function = info_beginning_of_node;
  map['>'].function = info_end_of_node;
  map['b'].function = info_backward_word;
  map['f'].function = info_forward_word;
  map['r'].function = info_move_to_window_line;
  map['v'].function = info_scroll_backward;
#if defined (NAMED_FUNCTIONS)
  map['x'].function = info_execute_command;
#endif /* NAMED_FUNCTIONS */

  /* Bind members in the Control-X map for Info windows. */
  map = (Keymap)info_keymap[Control ('x')].function;

  map[Control ('b')].function = list_visited_nodes;
  map[Control ('c')].function = info_quit;
  map[Control ('f')].function = info_view_file;
  map[Control ('g')].function = info_abort_key;
  map[Control ('v')].function = info_view_file;
  map['0'].function = info_delete_window;
  map['1'].function = info_keep_one_window;
  map['2'].function = info_split_window;
  map['^'].function = info_grow_window;
  map['b'].function = select_visited_node;
  map['k'].function = info_kill_node;
  map['o'].function = info_next_window;
  map['t'].function = info_tile_windows;
  map['w'].function = info_toggle_wrap;

  /* Arrow key bindings for Info windows keymap. */
  map = info_keymap;
  keymap_bind_keyseq (map, term_kN, &map[Control ('v')]); /* pagedown */
  keymap_bind_keyseq (map, term_ku, &map[Control ('p')]); /* up */
  keymap_bind_keyseq (map, "\033OA", &map[Control ('p')]);
  keymap_bind_keyseq (map, "\033[A", &map[Control ('p')]);
  keymap_bind_keyseq (map, term_kd, &map[Control ('n')]); /* down */
  keymap_bind_keyseq (map, "\033OB", &map[Control ('n')]);
  keymap_bind_keyseq (map, "\033[B", &map[Control ('n')]);
  keymap_bind_keyseq (map, term_kr, &map[Control ('f')]); /* right */
  keymap_bind_keyseq (map, "\033OC", &map[Control ('f')]);
  keymap_bind_keyseq (map, "\033[C", &map[Control ('f')]);
  keymap_bind_keyseq (map, term_kl, &map[Control ('b')]); /* left */
  keymap_bind_keyseq (map, "\033OD", &map[Control ('b')]);
  keymap_bind_keyseq (map, "\033[D", &map[Control ('b')]);

  map = (Keymap)info_keymap[ESC].function;
  keymap_bind_keyseq (map, term_kl, &map['b']); /* left */
  keymap_bind_keyseq (map, "\033OA", &map['b']);
  keymap_bind_keyseq (map, "\033[A", &map['b']);
  keymap_bind_keyseq (map, term_kr, &map['f']); /* right */
  keymap_bind_keyseq (map, "\033OB", &map['f']);
  keymap_bind_keyseq (map, "\033[B", &map['f']);
  keymap_bind_keyseq (map, term_kN, &map[Control ('v')]); /* pagedown */

  /* The alternative to this definition of a `main map' key in the
     `ESC map' section, is something like:
    keymap_bind_keyseq (map, term_kP, &((KeyMap)map[ESC].function).map['v']);
  */
  keymap_bind_keyseq (info_keymap/*sic*/, term_kP, &map['v']); /* pageup */
}
