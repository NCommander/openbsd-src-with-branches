//    This is part of the iostream library, providing -*- C++ -*- input/output.
//    Copyright (C) 1991 Per Bothner.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Library General Public License for more details.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//	$Id: parsestream.h,v 1.2 1993/08/02 17:22:37 mycroft Exp $

#ifndef PARSESTREAM_H
#define PARSESTREAM_H
#ifdef __GNUG__
#pragma interface
#endif
#include "streambuf.h"

// A parsebuf is a streambuf optimized for scanning text files.
// It keeps track of line and column numbers.
// It is guaranteed to remember the entire current line,
// as well the '\n'-s on either side of it (if they exist).
// You can arbitrarily seek (or unget) within this extended line.
// Other backward seeks are not supported.
// Normal read semantics are supported (and hence istream operators like >>).

class parsebuf : public backupbuf {
  protected:
    _G_fpos_t pos_at_line_start;
    long _line_length;
    unsigned long __line_number;
    char *buf_start;
    char *buf_end;

  public:
    parsebuf *chain;

    // Return column number (raw - don't handle tabs etc).
    // Retult can be -1, meaning: at '\n' before current line.
    virtual int tell_in_line();

    // seek to (raw) column I in current line.
    // Result is new (raw) column position - differs from I if unable to seek.
    // Seek to -1 tries to seek to before previous LF.
    virtual int seek_in_line(int i);

    // Note: there is no "current line" initially, until something is read.

    // Current line number, starting with 0.
    // If tell_in_line()==-1, then line number of next line.
    int line_number() { return __line_number; }

    // Length of current line, not counting either '\n'.
    int line_length() { return _line_length; }
    // Current line - not a copy, so file ops may trash it. 
    virtual char* current_line();
    virtual streampos seekoff(streamoff, _seek_dir, int mode=ios::in|ios::out);
    virtual streambuf* setbuf(char* p, int len);
  protected:
    parsebuf() : backupbuf() { chain= NULL;
	__line_number = 0; pos_at_line_start = 0; _line_length = -1; }
    virtual int pbackfail(int c);
};

// A string_parsebuf is a parsebuf whose source is a fixed string.

class string_parsebuf : public parsebuf {
  public:
    int do_delete;
    string_parsebuf(char *str, int len, int delete_at_close=0);
    virtual int underflow();
    virtual char* current_line();
    virtual int seek_in_line(int i);
    virtual int tell_in_line();
    char *left() const { return base(); }
    char *right() const { return ebuf(); }
//    streampos seekoff(streamoff, _seek_dir, int);
};

// A func_parsebuf calls a given function to get new input.
// Each call returns an entire NUL-terminated line (without the '\n').
// That line has been allocated with malloc(), not new.
// The interface is tailored to the GNU readline library.
// Example:
// char* DoReadLine(void* arg)
// {
//   char *line = readline((char*)arg); /* 'arg' is used as prompt. */
//   if line == NULL) { putc('\n', stderr); return NULL; }
//   if (line[0] != '\0') add_history(line);
//    return line;
// }
// char PromptBuffer[100] = "> ";
// func_parsebuf my_stream(DoReadLine, PromptBuffer);

typedef char *(*CharReader)(void *arg);
class istream;

class func_parsebuf : public parsebuf {
  public:
    void *arg;
    CharReader read_func;
    int backed_up_to_newline;
    func_parsebuf(CharReader func, void *argm = NULL);
    int underflow();
    virtual int tell_in_line();
    virtual int seek_in_line(int i);
    virtual char* current_line();
};

// A general_parsebuf is a parsebuf which gets its input from some
// other streambuf. It explicitly buffers up an entire line.

class general_parsebuf : public parsebuf {
  public:
    streambuf *sbuf;
    int delete_buf; // Delete sbuf when destroying this.
    general_parsebuf(streambuf *buf, int delete_arg_buf = 0);
    int underflow();
    virtual int tell_in_line();
    virtual int seek_in_line(int i);
    ~general_parsebuf();
    virtual char* current_line();
};

#if 0
class parsestream : public istream {
    streammarker marks[2];
    short _first; // of the two marks; either 0 or 1
    int _lineno;
    int first() { return _first; }
    int second() { return 1-_first; }
    int line_length() { marks[second].delta(marks[first]); }
    int line_length() { marks[second].delta(marks[first]); }
    int seek_in_line(int i);
    int tell_in_line();
    int line_number();
};
#endif
#endif /*!defined(PARSESTREAM_H)*/
