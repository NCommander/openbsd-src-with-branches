/*	$OpenBSD: pcb.h,v 1.3 2020/06/21 13:23:59 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _MACHINE_PCB_H_
#define _MACHINE_PCB_H_

#include <machine/reg.h>

struct pcb {
	register_t	pcb_sp;
	u_int		pcb_flags;
#define PCB_FP		0x000000001
#define PCB_VEC		0x000000002
#define PCB_VSX		0x000000004
	caddr_t		pcb_onfault;
	struct fpreg	pcb_fpstate;
};

#endif /* _MACHINE_PCB_H_ */
