/* $OpenBSD: vmparam.h,v 1.27 2004/04/26 14:31:11 miod Exp $ */
/* public domain */

/*
 * Physical memory is mapped 1:1 at the bottom of the supervisor address
 * space. Kernel virtual memory space starts from the end of physical memory,
 * up to the on-board devices appearing all over the last 8MB of address space.
 */
#define VM_MIN_KERNEL_ADDRESS	((vaddr_t)0x00000000)
#define VM_MAX_KERNEL_ADDRESS	((vaddr_t)0xff800000)

#include <m88k/vmparam.h>
