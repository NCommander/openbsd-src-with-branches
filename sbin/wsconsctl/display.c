/*	$OpenBSD: display.c,v 1.1 2000/07/01 23:52:45 mickey Exp $	*/
/*	$NetBSD: display.c,v 1.1 1998/12/28 14:01:16 hannken Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Juergen Hannken-Illjes.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/ioctl.h>
#include <sys/time.h>
#include <dev/wscons/wsconsio.h>
#include <err.h>
#include "wsconsctl.h"

int dpytype;
int burnon, burnoff, vblank, kbdact, msact, outact;

struct field display_field_tab[] = {
    { "type",			&dpytype,	FMT_DPYTYPE,	FLG_RDONLY },
    { "screen_on",		&burnon,	FMT_UINT,	FLG_MODIFY },
    { "screen_off",		&burnoff,	FMT_UINT,	FLG_MODIFY },
    { "vblank",			&vblank,	FMT_BOOL,	FLG_MODIFY },
    { "kbdact",			&kbdact,	FMT_BOOL,	FLG_MODIFY },
    { "msact",			&msact,		FMT_BOOL,	FLG_MODIFY },
    { "outact",			&outact,	FMT_BOOL,	FLG_MODIFY },
};

int display_field_tab_len = sizeof(display_field_tab)/
			     sizeof(display_field_tab[0]);

void
display_get_values(fd)
	int fd;
{
	if (field_by_value(&dpytype)->flags & FLG_GET)
		if (ioctl(fd, WSDISPLAYIO_GTYPE, &dpytype) < 0)
			err(1, "WSDISPLAYIO_GTYPE");
	if (field_by_value(&burnon)->flags & FLG_GET ||
	    field_by_value(&burnoff)->flags & FLG_GET ||
	    field_by_value(&vblank)->flags & FLG_GET ||
	    field_by_value(&kbdact)->flags & FLG_GET ||
	    field_by_value(&msact )->flags & FLG_GET ||
	    field_by_value(&outact)->flags & FLG_GET) {

		struct wsdisplay_burner burners;

		if (ioctl(fd, WSDISPLAYIO_GBURNER, &burners) < 0)
			err(1, "WSDISPLAYIO_GBURNER");

		if (field_by_value(&burnon)->flags & FLG_GET)
			burnon = burners.on;

		if (field_by_value(&burnoff)->flags & FLG_GET)
			burnoff = burners.off;

		if (field_by_value(&vblank)->flags & FLG_GET)
			vblank = burners.flags & WSDISPLAY_BURN_VBLANK;

		if (field_by_value(&kbdact)->flags & FLG_GET)
			kbdact = burners.flags & WSDISPLAY_BURN_KBD;

		if (field_by_value(&msact )->flags & FLG_GET)
			msact = burners.flags & WSDISPLAY_BURN_MOUSE;

		if (field_by_value(&outact)->flags & FLG_GET)
			outact = burners.flags & WSDISPLAY_BURN_OUTPUT;
	}
}

void
display_put_values(fd)
	int fd;
{
	if (field_by_value(&burnon)->flags & FLG_SET ||
	    field_by_value(&burnoff)->flags & FLG_SET ||
	    field_by_value(&vblank)->flags & FLG_SET ||
	    field_by_value(&kbdact)->flags & FLG_SET ||
	    field_by_value(&msact )->flags & FLG_SET ||
	    field_by_value(&outact)->flags & FLG_SET) {

		struct wsdisplay_burner burners;

		if (ioctl(fd, WSDISPLAYIO_GBURNER, &burners) < 0)
			err(1, "WSDISPLAYIO_GBURNER");

		if (field_by_value(&burnon)->flags & FLG_SET)
			burners.on = burnon;

		if (field_by_value(&burnoff)->flags & FLG_SET)
			burners.off = burnoff;

		if (field_by_value(&vblank)->flags & FLG_SET) {
			if (vblank)
				burners.flags |= WSDISPLAY_BURN_VBLANK;
			else
				burners.flags &= ~WSDISPLAY_BURN_VBLANK;
		}

		if (field_by_value(&kbdact)->flags & FLG_SET) {
			if (kbdact)
				burners.flags |= WSDISPLAY_BURN_KBD;
			else
				burners.flags &= ~WSDISPLAY_BURN_KBD;
		}

		if (field_by_value(&msact )->flags & FLG_SET) {
			if (msact)
				burners.flags |= WSDISPLAY_BURN_MOUSE;
			else
				burners.flags &= ~WSDISPLAY_BURN_MOUSE;
		}

		if (field_by_value(&outact)->flags & FLG_SET) {
			if (outact)
				burners.flags |= WSDISPLAY_BURN_OUTPUT;
			else
				burners.flags &= ~WSDISPLAY_BURN_OUTPUT;
		}

		if (ioctl(fd, WSDISPLAYIO_SBURNER, &burners) < 0)
			err(1, "WSDISPLAYIO_SBURNER");
	}
}
