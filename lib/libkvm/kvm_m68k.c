/*	$OpenBSD$ */
/*	$NetBSD: kvm_m68k.c,v 1.9 1996/05/07 06:09:11 leo Exp $	*/

/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_hp300.c	8.1 (Berkeley) 6/4/93";
#else
static char *rcsid = "$OpenBSD$";
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * m68k machine dependent routines for kvm.  Hopefully, the forthcoming 
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>

#include <sys/core.h>
#include <sys/exec_aout.h>
#include <sys/kcore.h>

#include <unistd.h>
#include <limits.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <db.h>

#include "kvm_private.h"

#include <machine/pte.h>
#include <machine/kcore.h>

#ifndef btop
#define	btop(x)		(((unsigned)(x)) >> PGSHIFT)	/* XXX */
#define	ptob(x)		((caddr_t)((x) << PGSHIFT))	/* XXX */
#endif

#define KREAD(kd, addr, p)\
	(kvm_read(kd, addr, (char *)(p), sizeof(*(p))) != sizeof(*(p)))

void
_kvm_freevtop(kd)
	kvm_t *kd;
{
	if (kd->vmst != 0)
		free(kd->vmst);
}

int
_kvm_initvtop(kd)
	kvm_t *kd;
{
	return (0);
}

static int
_kvm_vatop(kd, sta, va, pa)
	kvm_t *kd;
	st_entry_t *sta;
	u_long va;
	u_long *pa;
{
	register cpu_kcore_hdr_t *cpu_kh;
	register u_long addr;
	int p, ste, pte;
	int offset;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return((off_t)0);
	}
	offset = va & PGOFSET;
	cpu_kh = kd->cpu_data;
	/*
	 * If we are initializing (kernel segment table pointer not yet set)
	 * then return pa == va to avoid infinite recursion.
	 */
	if (cpu_kh->sysseg_pa == 0) {
		*pa = va + cpu_kh->kernel_pa;
		return (NBPG - offset);
	}
	if (cpu_kh->mmutype == -2) {
		st_entry_t *sta2;

		addr = (u_long)&sta[va >> SG4_SHIFT1];
		/*
		 * Can't use KREAD to read kernel segment table entries.
		 * Fortunately it is 1-to-1 mapped so we don't have to. 
		 */
		if (sta == cpu_kh->sysseg_pa) {
			if (lseek(kd->pmfd, _kvm_pa2off(kd, addr), 0) == -1 ||
			    read(kd->pmfd, (char *)&ste, sizeof(ste)) < 0)
				goto invalid;
		} else if (KREAD(kd, addr, &ste))
			goto invalid;
		if ((ste & SG_V) == 0) {
			_kvm_err(kd, 0, "invalid level 1 descriptor (%x)",
				 ste);
			return((off_t)0);
		}
		sta2 = (st_entry_t *)(ste & SG4_ADDR1);
		addr = (u_long)&sta2[(va & SG4_MASK2) >> SG4_SHIFT2];
		/*
		 * Address from level 1 STE is a physical address,
		 * so don't use kvm_read.
		 */
		if (lseek(kd->pmfd, _kvm_pa2off(kd, addr), 0) == -1 || 
		    read(kd->pmfd, (char *)&ste, sizeof(ste)) < 0)
			goto invalid;
		if ((ste & SG_V) == 0) {
			_kvm_err(kd, 0, "invalid level 2 descriptor (%x)",
				 ste);
			return((off_t)0);
		}
		sta2 = (st_entry_t *)(ste & SG4_ADDR2);
		addr = (u_long)&sta2[(va & SG4_MASK3) >> SG4_SHIFT3];
	} else {
		addr = (u_long)&sta[va >> SEGSHIFT];
		/*
		 * Can't use KREAD to read kernel segment table entries.
		 * Fortunately it is 1-to-1 mapped so we don't have to. 
		 */
		if (sta == cpu_kh->sysseg_pa) {
			if (lseek(kd->pmfd, _kvm_pa2off(kd, addr), 0) == -1 ||
			    read(kd->pmfd, (char *)&ste, sizeof(ste)) < 0)
				goto invalid;
		} else if (KREAD(kd, addr, &ste))
			goto invalid;
		if ((ste & SG_V) == 0) {
			_kvm_err(kd, 0, "invalid segment (%x)", ste);
			return((off_t)0);
		}
		p = btop(va & SG_PMASK);
		addr = (ste & SG_FRAME) + (p * sizeof(pt_entry_t));
	}
	/*
	 * Address from STE is a physical address so don't use kvm_read.
	 */
	if (lseek(kd->pmfd, _kvm_pa2off(kd, addr), 0) == -1 || 
	    read(kd->pmfd, (char *)&pte, sizeof(pte)) < 0)
		goto invalid;
	addr = pte & PG_FRAME;
	if (pte == PG_NV) {
		_kvm_err(kd, 0, "page not valid");
		return (0);
	}
	*pa = addr + offset;
	
	return (NBPG - offset);
invalid:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}

int
_kvm_kvatop(kd, va, pa)
	kvm_t *kd;
	u_long va;
	u_long *pa;
{
	register cpu_kcore_hdr_t *cpu_kh;

	cpu_kh = kd->cpu_data;
	return (_kvm_vatop(kd, (u_long)cpu_kh->sysseg_pa, va, pa));
}

/*
 * Translate a physical address to a file-offset in the crash-dump.
 */
off_t
_kvm_pa2off(kd, pa)
	kvm_t	*kd;
	u_long	pa;
{
	off_t		off;
	phys_ram_seg_t	*rsp;
	register cpu_kcore_hdr_t *cpu_kh;

	cpu_kh = kd->cpu_data;
	off = 0;
	for (rsp = cpu_kh->ram_segs; rsp->size; rsp++) {
		if (pa >= rsp->start && pa < rsp->start + rsp->size) {
			pa -= rsp->start;
			break;
		}
		off += rsp->size;
	}
	return(kd->dump_off + off + pa);
}
