
#include <machine/asm.h>

/*
 * machdep_start(phyaddr)
 *	start the program on protected mode where phyaddr is the entry point
 */
ENTRY(machdep_start)
	pushl	%ebp
	movl	%esp, %ebp

	# get things we need into registers
	movl	8(%ebp), %ecx		# entry offset 
	movl	12(%ebp), %edi		# howto
	movl	24(%ebp), %esi		# esym

	cli
	# make a new stack at 0:0x80000 (big segs)
	movl	$0x10, %ebx
	movw	%bx, %ss
	movl	$0x80000, %ebx
	movl	%ebx, %esp
	sti
	
	# push some number of args onto the stack
	pushl	%esi		# esym
	movl	$_cyloffset, %eax
	pushl	%eax		# cyloff
	movl	$_bootdev, %eax
	pushl	%eax		# bootdev
	pushl	%edi		# howto
	pushl	$0		# dummy 'return' address
	
	# push on our entry address
	pushw	$0x8			# segment
	pushl	%ecx

	# convert over the other data segs
	movl	$0x10, %ebx
	movl	%bx, %ds
	movl	%bx, %es

	# convert the PC (and code seg)
	lret

