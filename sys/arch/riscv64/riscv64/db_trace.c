/*
 * Copyright (c) 2000, 2001 Ben Harris
 * Copyright (c) 1996 Scott K. Stevens
 *
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>

#include <sys/proc.h>
#include <sys/stacktrace.h>
#include <sys/user.h>
#include <machine/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_interface.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>

db_regs_t ddb_regs;

#define INKERNEL(va)	(((vaddr_t)(va)) & (1ULL << 63))

void
db_stack_trace_print(db_expr_t addr, int have_addr, db_expr_t count,
    char *modif, int (*pr)(const char *, ...))
{
	u_int64_t	frame, lastframe, ra, lastra, sp;
	char		c, *cp = modif;
	db_expr_t	offset;
	Elf_Sym *	sym;
	char		*name;
	int		kernel_only = 1;

	while ((c = *cp++) != 0) {
		if (c == 'u')
			kernel_only = 0;
		if (c == 't') {
			db_printf("tracing threads not yet supported\n");
			return;
		}
	}

	if (!have_addr) {
		sp = ddb_regs.tf_sp;
		ra = ddb_regs.tf_ra;
		lastra = ddb_regs.tf_ra;
		frame = ddb_regs.tf_s[0];
	} else {
		sp = addr;
		db_read_bytes(sp - 16, sizeof(vaddr_t), (char *)&frame);
		db_read_bytes(sp - 8, sizeof(vaddr_t), (char *)&ra);
		lastra = 0;
	}

	while (count-- && frame != 0) {
		lastframe = frame;

		sym = db_search_symbol(lastra, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);

		if (name == NULL || strcmp(name, "end") == 0) {
			(*pr)("%llx at 0x%lx", lastra, ra - 4);
		} else {
			(*pr)("%s() at ", name);
			db_printsym(ra - 4, DB_STGY_PROC, pr);
		}
		(*pr)("\n");

		// can we detect traps ?
		db_read_bytes(frame - 16, sizeof(vaddr_t), (char *)&frame);
		if (frame == 0)
			break;
		lastra = ra;
		db_read_bytes(frame - 8, sizeof(vaddr_t), (char *)&ra);

#if 0
		if (name != NULL) {
			if ((strcmp (name, "handle_el0_irq") == 0) ||
			    (strcmp (name, "handle_el1_irq") == 0)) {
				(*pr)("--- interrupt ---\n");
			} else if (
			    (strcmp (name, "handle_el0_sync") == 0) ||
			    (strcmp (name, "handle_el1_sync") == 0)) {
				(*pr)("--- trap ---\n");
			}
		}
#endif
		if (INKERNEL(frame)) {
			if (frame <= lastframe) {
				(*pr)("Bad frame pointer: 0x%lx\n", frame);
				break;
			}
		} else {
			if (kernel_only)
				break;
		}

		--count;
	}
}
