/*	$OpenBSD: crtend.c,v 1.10 2012/12/05 23:19:57 deraadt Exp $	*/
/*	$NetBSD: crtend.c,v 1.1 1996/09/12 16:59:04 cgd Exp $	*/

#include <sys/types.h>
#include "md_init.h"
#include "extern.h"

static init_f __CTOR_LIST__[1]
    __used __attribute__((section(".ctors"))) = { (void *)0 };	/* XXX */
static init_f __DTOR_LIST__[1]
    __used __attribute__((section(".dtors"))) = { (void *)0 };	/* XXX */

static const int __EH_FRAME_END__[]
    __used __attribute__((section(".eh_frame"), aligned(4))) = { 0 };

static void * __JCR_END__[]
    __used __attribute__((section(".jcr"), aligned(sizeof(void*)))) = { 0 };

MD_SECTION_EPILOGUE(".init");
MD_SECTION_EPILOGUE(".fini");
