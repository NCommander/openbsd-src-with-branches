/*	$OpenBSD: htonl.S,v 1.2 2015/05/29 09:25:28 uebayasi Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(htonl)
	RETGUARD_SETUP(htonl, r11)
	movl    %edi,%eax
	bswapl  %eax
	RETGUARD_CHECK(htonl, r11)
	ret
END(htonl)
