/* Manipulate directory entries of a particular file class. */
#include <std.h>
#include "entry.h"

/* Initially provide a medium-sized file entry size. */
const int Entry_Handler::default_entries = 50;

/* Print entries to standard output.  The algorithm used to format the files
   in columns is subtle, and worth studying in some detail. */

void
Entry_Handler::print_entries (char *class_name) 
{
  const int  width = Screen_Handler::screen_width ();
  const int  ncols = width / (max_entry_length + 1);
  const int  nrows = (entries + ncols - 1) / ncols;
  const int  max   = nrows * (entries - (nrows - 1) * ncols);
#if defined (__GNUC__) && ! defined (__STRICT_ANSI__)
  char buffer[width];
#else
  char *buffer = new char[width];
#endif
  int row;

  /* Print out the file class name, nicely centered and in inverse video. */
  sprintf (buffer, "[ %d %s File%s ]", entries, class_name, entries == 1 ? "" : "s");
  Screen_Handler::print_inverse_centered (buffer);

#ifndef __GNUC__
  delete [] buffer;
#endif

  /* Take care of everything but the (possibly non-existent) final row. */
  
  for (row = 0; row < nrows - 1; row++)
    {
      /* This loop is subtle, since we don't want to process too many entries.... */
      
      for (int col = row; col < entries; col += col < max ? nrows : nrows - 1)
        printf ("%-*s", max_entry_length + 1, buf[col]);
      
      putchar ('\n');
    }
  /* Treat the final row specially, if it exists. */
  
  for (; row < max; row += nrows) 
    printf ("%-*s", max_entry_length + 1, buf[row]);
  
  putchar ('\n');
}

/* Only compile these functions if -O is *not* enabled. */

#ifndef __OPTIMIZE__

/* Initialize a new file class. */

Entry_Handler::Entry_Handler (void)
{
  entries = max_entry_length = 0;
  total_entries = default_entries;
  buf = new char *[default_entries];
}

/* Current number of file entries. */

int 
Entry_Handler::entry_number (void)
{
  return entries;
}

/* Add an entry to the file class. */

void 
Entry_Handler::add_entry (char *entry_name, int length)
{
  /* Grow the buffer on overflow. */
  if (entries >= total_entries)
    buf = new (buf, total_entries *= 2) char *;
  if (max_entry_length < length)
    max_entry_length = length;
  buf[entries++] = strcpy (new char[length + 1], entry_name);
}

/* Sort entries by filename. */

void 
Entry_Handler::sort_entries (void)
{
  sort (buf, entries);
}

#endif                          // __OPTIMIZE__
