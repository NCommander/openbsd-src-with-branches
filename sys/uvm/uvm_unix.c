/*	$OpenBSD: uvm_unix.c,v 1.61 2017/02/02 06:23:58 guenther Exp $	*/
/*	$NetBSD: uvm_unix.c,v 1.18 2000/09/13 15:00:25 thorpej Exp $	*/

/*
 * Copyright (c) 1997 Charles D. Cranor and Washington University.
 * Copyright (c) 1991, 1993 The Regents of the University of California.  
 * Copyright (c) 1988 University of Utah.
 *
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: vm_unix.c 1.1 89/11/07$
 *      @(#)vm_unix.c   8.1 (Berkeley) 6/11/93
 * from: Id: uvm_unix.c,v 1.1.2.2 1997/08/25 18:52:30 chuck Exp
 */

/*
 * uvm_unix.c: traditional sbrk/grow interface to vm.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/vnode.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm.h>

/*
 * sys_obreak: set break
 */

int
sys_obreak(struct proc *p, void *v, register_t *retval)
{
	struct sys_obreak_args /* {
		syscallarg(char *) nsize;
	} */ *uap = v;
	struct vmspace *vm = p->p_vmspace;
	vaddr_t new, old, base;
	int error;

	base = (vaddr_t)vm->vm_daddr;
	new = round_page((vaddr_t)SCARG(uap, nsize));
	if (new < base || (new - base) > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		return (ENOMEM);

	old = round_page(base + ptoa(vm->vm_dsize));

	if (new == old)
		return (0);

	/* grow or shrink? */
	if (new > old) {
		error = uvm_map(&vm->vm_map, &old, new - old, NULL,
		    UVM_UNKNOWN_OFFSET, 0,
		    UVM_MAPFLAG(PROT_READ | PROT_WRITE,
		    PROT_READ | PROT_WRITE | PROT_EXEC, MAP_INHERIT_COPY,
		    MADV_NORMAL, UVM_FLAG_FIXED|UVM_FLAG_COPYONW));
		if (error) {
			uprintf("sbrk: grow %ld failed, error = %d\n",
			    new - old, error);
			return (ENOMEM);
		}
		vm->vm_dsize += atop(new - old);
	} else {
		uvm_deallocate(&vm->vm_map, new, old - new);
		vm->vm_dsize -= atop(old - new);
	}

	return (0);
}

/*
 * uvm_grow: enlarge the "stack segment" to include sp.
 */
void
uvm_grow(struct proc *p, vaddr_t sp)
{
	struct vmspace *vm = p->p_vmspace;
	int si;

	/* For user defined stacks (from sendsig). */
	if (sp < (vaddr_t)vm->vm_maxsaddr)
		return;

	/* For common case of already allocated (from trap). */
#ifdef MACHINE_STACK_GROWS_UP
	if (sp < (vaddr_t)vm->vm_maxsaddr + ptoa(vm->vm_ssize))
#else
	if (sp >= (vaddr_t)vm->vm_minsaddr - ptoa(vm->vm_ssize))
#endif
		return;

	/* Really need to check vs limit and increment stack size if ok. */
#ifdef MACHINE_STACK_GROWS_UP
	si = atop(sp - (vaddr_t)vm->vm_maxsaddr) - vm->vm_ssize + 1;
#else
	si = atop((vaddr_t)vm->vm_minsaddr - sp) - vm->vm_ssize;
#endif
	if (vm->vm_ssize + si <= atop(p->p_rlimit[RLIMIT_STACK].rlim_cur))
		vm->vm_ssize += si;
}

#ifndef SMALL_KERNEL

/*
 * Common logic for whether a map entry should be included in a coredump
 */
static inline int
uvm_should_coredump(struct proc *p, struct vm_map_entry *entry)
{
	if (!(entry->protection & PROT_WRITE) &&
	    entry->aref.ar_amap == NULL &&
	    entry->start != p->p_p->ps_sigcode)
		return 0;

	/*
	 * Skip ranges marked as unreadable, as uiomove(UIO_USERSPACE)
	 * will fail on them.  Maybe this really should be a test of
	 * entry->max_protection, but doing
	 *	uvm_map_extract(UVM_EXTRACT_FIXPROT)
	 * on each such page would suck.
	 */
	if ((entry->protection & PROT_READ) == 0)
		return 0;

	/* Don't dump mmaped devices. */
	if (entry->object.uvm_obj != NULL &&
	    UVM_OBJ_IS_DEVICE(entry->object.uvm_obj))
		return 0;

	if (entry->start >= VM_MAXUSER_ADDRESS)
		return 0;

	return 1;
}

/*
 * Walk the VA space for a process to identify what to write to
 * a coredump.  First the number of contiguous ranges is counted,
 * then the 'setup' callback is invoked to prepare for actually
 * recording the ranges, then the VA is walked again, invoking
 * the 'walk' callback for each range.  The number of ranges walked
 * is guaranteed to match the count seen by the 'setup' callback.
 */

int
uvm_coredump_walkmap(struct proc *p, uvm_coredump_setup_cb *setup,
    uvm_coredump_walk_cb *walk, void *cookie)
{
	struct vmspace *vm = p->p_vmspace;
	struct vm_map *map = &vm->vm_map;
	struct vm_map_entry *entry;
	vaddr_t end;
	int nsegment, error;

	/*
	 * Walk the map once to count the segments.
	 */
	nsegment = 0;
	vm_map_lock_read(map);
	RBT_FOREACH(entry, uvm_map_addr, &map->addr) {
		/* should never happen for a user process */
		if (UVM_ET_ISSUBMAP(entry)) {
			panic("%s: user process with submap?", __func__);
		}

		if (! uvm_should_coredump(p, entry))
			continue;

		nsegment++;
	}

	/*
	 * Okay, we have a count in nsegment; invoke the setup callback.
	 */
	error = (*setup)(nsegment, cookie);
	if (error)
		goto cleanup;

	/*
	 * Setup went okay, so do the second walk, invoking the walk
	 * callback on the counted segments.
	 */
	nsegment = 0;
	RBT_FOREACH(entry, uvm_map_addr, &map->addr) {
		if (! uvm_should_coredump(p, entry))
			continue;

		end = entry->end;
		if (end > VM_MAXUSER_ADDRESS)
			end = VM_MAXUSER_ADDRESS;

		error = (*walk)(entry->start, end, end, entry->protection,
		    nsegment, cookie);
		if (error)
			break;
		nsegment++;
	}

cleanup:
	vm_map_unlock_read(map);

	return error;
}

#endif	/* !SMALL_KERNEL */
