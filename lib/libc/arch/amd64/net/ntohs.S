/*	$OpenBSD: ntohs.S,v 1.3 2015/05/29 09:25:28 uebayasi Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(ntohs)
	RETGUARD_SETUP(ntohs, r11)
	movl    %edi,%eax
	xchgb   %ah,%al
	RETGUARD_CHECK(ntohs, r11)
	ret
END(ntohs)
