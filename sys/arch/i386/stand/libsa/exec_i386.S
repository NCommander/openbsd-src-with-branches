/*	$OpenBSD$	*/

#include <machine/asm.h>

/*
 * machdep_start(phyaddr)
 *	start the program on protected mode where phyaddr is the entry point
 */
ENTRY(machdep_exec)
	pushl	%ebp
	movl	%esp, %ebp

	# get things we need into registers
	movl	8(%ebp), %ecx		# entry offset 
	movl	12(%ebp), %edi		# howto
	movl	16(%ebp), %edx		# loadaddr
	movl	24(%ebp), %esi		# esym

	# make a new stack at 0:0x80000 (big segs)
	movl	$0x10, %eax
	movw	%ax, %ss
	movl	$0x90000, %eax
	movl	%eax, %esp
	
	# push some number of args onto the stack
	pushl	%esi		# esym
	movl	$_cyloffset, %eax
	pushl	%eax		# cyloff
	movl	$_bootdev, %eax
	pushl	%eax		# bootdev
	pushl	%edi		# howto
	pushl	$0		# dummy 'return' address
	
	# push on our entry address
	movl	0x8, %eax		# segment
	pushw	%ax
	pushl	%ecx
	movl	fmt, %eax
	pushl	%eax
	call	_printf
	popl	%eax

	# convert over the other data segs
	movw	$0x10, %bx
	movl	%bx, %ds
	movl	%bx, %es

	# convert the PC (and code seg)
	lret
fmt:
	.asciz	"%x\n"
