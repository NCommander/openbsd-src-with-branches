/*	$OpenBSD: htons.S,v 1.3 2015/05/29 09:25:28 uebayasi Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(htons)
	RETGUARD_SETUP(htons, r11)
	movl    %edi,%eax
	xchgb   %ah,%al
	RETGUARD_CHECK(htons, r11)
	ret
END(htons)
