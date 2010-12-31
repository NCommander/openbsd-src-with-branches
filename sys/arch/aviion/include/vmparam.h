/* $OpenBSD: vmparam.h,v 1.1.1.1 2006/04/18 10:56:58 miod Exp $ */
/* public domain */
/*
 * Physical memory is mapped 1:1 at the bottom of the supervisor address
 * space. Kernel virtual memory space starts from the end of physical memory,
 * up to the on-board devices appearing all over the last 4MB of address space.
 */
#define VM_MIN_KERNEL_ADDRESS	((vaddr_t)0x00000000)
#define VM_MAX_KERNEL_ADDRESS	((vaddr_t)0xffc00000)

#include <m88k/vmparam.h>
