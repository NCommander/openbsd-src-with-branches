/* Native definitions for alpha running OSF/1.
   Copyright (C) 1993, 1994 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   argument regs.  A0_REGNUM points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
extern int
get_longjmp_target PARAMS ((CORE_ADDR *));

/* #include "nm-obsd.h" */
#include <machine/alpha_cpu.h>

#define ATTACH_DETACH

/* unnneeded here. */
#define PTRACE_ARG3_TYPE char*

#define PTRACE_XFER_TYPE int

/* The alpha does not step over a breakpoint, the manpage is lying again.  */

#define CANNOT_STEP_BREAKPOINT /* ??? */

#define	FETCH_INFERIOR_REGISTERS
#define	CHILD_RESUME
