
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
	movl	12(%ebp), %eax		# &argv

	# make a new stack at 0:0x90000 (big segs)
	movl	$0x10, %ebx
	movw	%bx, %ss
	movl	$0x90000, %ebx
	movl	%ebx, %esp
	
	# push some number of args onto the stack
	pushl	28(%eax)		# argv[7] = cnvmem
	pushl	32(%eax)		# argv[8] = extmem
	pushl	16(%eax)		# argv[4] = esym
	pushl	$0			# nominally a cyl offset in the boot.
	pushl	8(%eax)			# argv[2] = bootdev
	pushl	4(%eax)			# argv[1] = howto
	pushl	$0			# dummy 'return' address
	
	# push on our entry address
	pushl	$0x8			# segment
	pushl	%ecx

	# convert over the other data segs
	movl	$0x10, %ebx
	movl	%bx, %ds
	movl	%bx, %es

	# convert the PC (and code seg)
	lret

