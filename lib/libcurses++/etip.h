// * This makes emacs happy -*-Mode: C++;-*-
/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *   Author: Juergen Pfeifer <juergen.pfeifer@gmx.net> 1997                 *
 ****************************************************************************/

// $From: etip.h.in,v 1.10 1999/05/16 17:29:47 juergen Exp $

#ifndef _ETIP_H
#define _ETIP_H

// These are substituted at configure/build time
#ifndef HAVE_BUILTIN_H
#  define HAVE_BUILTIN_H 0
#endif

#ifndef HAVE_TYPEINFO
#  define HAVE_TYPEINFO 1
#endif

#ifndef HAVE_VALUES_H
#  define HAVE_VALUES_H 0
#endif

#ifndef ETIP_NEEDS_MATH_H
#define ETIP_NEEDS_MATH_H 0
#endif

#ifndef ETIP_NEEDS_MATH_EXCEPTION
#define ETIP_NEEDS_MATH_EXCEPTION 0
#endif

#ifdef __GNUG__
#  if ((__GNUG__ <= 2) && (__GNUC_MINOR__ < 8))
#    if HAVE_TYPEINFO
#      include <typeinfo>
#    endif
#  endif
#endif

#if defined(__GNUG__)
#  if HAVE_BUILTIN_H
#    if ETIP_NEEDS_MATH_H
#      if ETIP_NEEDS_MATH_EXCEPTION
#        undef exception
#        define exception math_exception
#      endif
#      include <math.h>
#    endif
#    undef exception
#    define exception builtin_exception
#    include <builtin.h>
#    undef exception
#  endif
#elif defined (__SUNPRO_CC)
#  include <generic.h>
#  include <string.h>
#else
#  include <string.h>
#endif

extern "C" {
#if HAVE_VALUES_H
#  include <values.h>
#endif

#include <assert.h>
#include <eti.h>
#include <errno.h>
}

// Forward Declarations
class NCursesPanel;
class NCursesMenu;
class NCursesForm;

class NCursesException
{
public:
  const char *message;
  int errorno;

  NCursesException (const char* msg, int err)
    : message(msg), errorno (err)
    {};

  NCursesException (const char* msg)
    : message(msg), errorno (E_SYSTEM_ERROR)
    {};

  virtual const char *classname() const {
    return "NCursesWindow";
  }
};

class NCursesPanelException : public NCursesException
{
public:
  const NCursesPanel* p;

  NCursesPanelException (const char *msg, int err) : 
    NCursesException (msg, err),
    p ((NCursesPanel*)0)
    {};

  NCursesPanelException (const NCursesPanel* panel,
			 const char *msg,
			 int err) : 
    NCursesException (msg, err),
    p (panel)
    {};

  NCursesPanelException (int err) : 
    NCursesException ("panel library error", err),
    p ((NCursesPanel*)0)
    {};

  NCursesPanelException (const NCursesPanel* panel,
			 int err) : 
    NCursesException ("panel library error", err),
    p (panel)
    {};

  virtual const char *classname() const {
    return "NCursesPanel";
  }

};

class NCursesMenuException : public NCursesException
{
public:
  const NCursesMenu* m;

  NCursesMenuException (const char *msg, int err) : 
    NCursesException (msg, err),
    m ((NCursesMenu *)0)
    {};

  NCursesMenuException (const NCursesMenu* menu,
			const char *msg,
			int err) : 
    NCursesException (msg, err),
    m (menu)
    {};

  NCursesMenuException (int err) : 
    NCursesException ("menu library error", err),
    m ((NCursesMenu *)0)
    {};

  NCursesMenuException (const NCursesMenu* menu,
			int err) : 
    NCursesException ("menu library error", err),
    m (menu)
    {};

  virtual const char *classname() const {
    return "NCursesMenu";
  }

};

class NCursesFormException : public NCursesException
{
public:
  const NCursesForm* f;

  NCursesFormException (const char *msg, int err) : 
    NCursesException (msg, err),
    f ((NCursesForm*)0)
    {};

  NCursesFormException (const NCursesForm* form,
			const char *msg,
			int err) : 
    NCursesException (msg, err),
    f (form)
    {};

  NCursesFormException (int err) : 
    NCursesException ("form library error", err),
    f ((NCursesForm*)0)
    {};

  NCursesFormException (const NCursesForm* form,
			int err) : 
    NCursesException ("form library error", err),
    f (form)
    {};

  virtual const char *classname() const {
    return "NCursesForm";
  }

};

#if !(defined(__GNUG__)||defined(__SUNPRO_CC))
#  include <iostream.h>
   extern "C" void exit(int);
#endif

inline void THROW(const NCursesException *e) {
#if defined(__GNUG__)
#  if ((__GNUG__ <= 2) && (__GNUC_MINOR__ < 8))
      (*lib_error_handler)(e?e->classname():"",e?e->message:"");
#else
      throw *e;
#endif
#elif defined(__SUNPRO_CC)
  genericerror(1, ((e != 0) ? (char *)(e->message) : ""));
#else
  if (e)
    cerr << e->message << endl;
  exit(0);
#endif     
}

#define THROWS(s)

#endif // _ETIP_H
