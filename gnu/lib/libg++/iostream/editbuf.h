//    This is part of the iostream library, providing input/output for C++.
//    Copyright (C) 1991 Per Bothner.
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU Library General Public
//    License as published by the Free Software Foundation; either
//    version 2 of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//    You should have received a copy of the GNU Library General Public
//    License along with this library; if not, write to the Free
//    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//
//	$Id: editbuf.h,v 1.2 1993/08/02 17:22:30 mycroft Exp $

#ifndef _EDITBUF_H
#define _EDITBUF_H
#ifdef __GNUG__
#pragma interface
#endif
#include <stdio.h>
#include <fstream.h>

typedef unsigned long mark_pointer;
// At some point, it might be nice to parameterize this code
// in terms of buf_char.
typedef /*unsigned*/ char buf_char;

// Logical pos from start of buffer (does not count gap).
typedef long buf_index;
			
// Pos from start of buffer, possibly including gap_size.
typedef long buf_offset; 

#if 0
struct buf_cookie {
    FILE *file;
    struct edit_string *str;
    struct buf_cookie *next;
    buf_index tell();
};
#endif

struct edit_buffer;
struct edit_mark;

// A edit_string is defined as the region between the 'start' and 'end' marks.
// Normally (always?) 'start->insert_before()' should be false,
// and 'end->insert_before()' should be true.

struct edit_string {
    struct edit_buffer *buffer; // buffer that 'start' and 'end' belong to
    struct edit_mark *start, *end;
    int length() const; // count of buf_chars currently in string
    edit_string(struct edit_buffer *b,
		      struct edit_mark *ms, struct edit_mark *me)
	{ buffer = b; start = ms; end = me; }
/* Make a fresh, contiguous copy of the data in STR.
   Assign length of STR to *LENP.
   (Output has extra NUL at out[*LENP].) */
    buf_char *copy_bytes(int *lenp) const;
//    FILE *open_file(char *mode);
    void assign(struct edit_string *src); // copy bytes from src to this
};

struct edit_streambuf : public streambuf {
    friend edit_buffer;
    edit_string *str;
    edit_streambuf* next; // Chain of edit_streambuf's for a edit_buffer.
    short _mode;
    edit_streambuf(edit_string* bstr, int mode);
    ~edit_streambuf();
    virtual int underflow();
    virtual int overflow(int c = EOF);
    virtual streampos seekoff(streamoff, _seek_dir, int mode=ios::in|ios::out);
    void flush_to_buffer();
    void flush_to_buffer(edit_buffer* buffer);
    int _inserting;
    int inserting() { return _inserting; }
    void inserting(int i) { _inserting = i; }
//    int delete_chars(int count, char* cut_buf); Not implemented.
    int truncate();
    int is_reading() { return gptr() != NULL; }
    buf_char* current() { return is_reading() ? gptr() : pptr(); }
    void set_current(char *p, int is_reading);
  protected:
    void disconnect_gap_from_file(edit_buffer* buffer);
};

// A 'edit_mark' indicates a position in a buffer.
// It is "attached" the text (rather than the offset).
// There are two kinds of mark, which have different behavior
// when text is inserted at the mark:
// If 'insert_before()' is true the mark will be adjusted to be
// *after* the new text.

struct edit_mark {
    struct edit_mark *chain;
    mark_pointer _pos;
    inline int insert_before() { return _pos & 1; }
    inline unsigned long index_in_buffer(struct edit_buffer *buffer)
	{ return _pos >> 1; }
    inline buf_char *ptr(struct edit_buffer *buf);
    buf_index tell();
    edit_mark() { }
    edit_mark(struct edit_string *str, long delta);
    edit_buffer *buffer();
    ~edit_mark();
};

// A 'edit_buffer' consists of a sequence of buf_chars (the data),
// a list of edit_marks pointing into the data, and a list of FILEs
// also pointing into the data.
// A 'edit_buffer' coerced to a edit_string is the string of
// all the buf_chars in the buffer.

// This implementation uses a conventional buffer gap (as in Emacs).
// The gap start is defined by de-referencing a (buf_char**).
// This is because sometimes a FILE is inserting into the buffer,
// so rather than having each putc adjust the gap, we use indirection
// to have the gap be defined as the write pointer of the FILE.
// (This assumes that putc adjusts a pointer (as in GNU's libc), not an index.)

struct edit_buffer {
    buf_char *data; /* == emacs buffer_text.p1+1 */
    buf_char *_gap_start;
    edit_streambuf* _writer; // If non-NULL, currently writing stream
    inline buf_char *gap_start()
	{ return _writer ? _writer->pptr() : _gap_start; }
    buf_offset __gap_end_pos; // size of part 1 + size of gap
    /* int gap; implicit: buf_size - size1 - size2 */
    int buf_size;
    struct edit_streambuf *files;
    struct edit_mark start_mark;
    struct edit_mark end_mark;
    edit_buffer();
    inline buf_offset gap_end_pos() { return __gap_end_pos; }
    inline struct edit_mark *start_marker() { return &start_mark; }
    inline struct edit_mark *end_marker() { return &end_mark; }
/* these should be protected, ultimately */
    buf_index tell(edit_mark*);
    buf_index tell(buf_char*);
    inline buf_char *gap_end() { return data + gap_end_pos(); }
    inline int gap_size() { return gap_end() - gap_start(); }
    inline int size1() { return gap_start() - data; }
    inline int size2() { return buf_size - gap_end_pos(); }
    inline struct edit_mark * mark_list() { return &start_mark; }
    void make_gap (buf_offset);
    void move_gap (buf_offset pos);
    void move_gap (buf_char *pos) { move_gap(pos - data); }
    void gap_left (int pos);
    void gap_right (int pos);
    void adjust_markers(mark_pointer low, mark_pointer high,
			int amount, buf_char *old_data);
    void delete_range(buf_index from, buf_index to);
    void delete_range(struct edit_mark *start, struct edit_mark *end);
};

extern buf_char * bstr_copy(struct edit_string *str, int *lenp);

// Convert a edit_mark to a (buf_char*)

inline buf_char *edit_mark::ptr(struct edit_buffer *buf)
	{ return buf->data + index_in_buffer(buf); }

inline void edit_streambuf::flush_to_buffer()
{
    edit_buffer* buffer = str->buffer;
    if (buffer->_writer == this) flush_to_buffer(buffer);
}
#endif /* !_EDITBUF_H*/

