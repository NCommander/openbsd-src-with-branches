/*	$OpenBSD: start.S,v 1.4.2.5 1996/10/28 12:14:29 mickey Exp $	*/

	.file	"bootbios.S"

#include <machine/asm.h>
#include <machine/specialreg.h>

#define addr32  .byte 0x67
#define data32  .byte 0x66

#define	BLKCNT	48

#define BOOTSEG		0x6000	/* boot placement */
#define BOOTSTACK	0xfffc	/* boot stack */

#ifdef DEBUG
#define DBGMSG(msg)		\
	movb    $msg, %al;	\
	data32;			\
	call    chr
#else /* !DEBUG */
#define DBGMSG(msg)
#endif /* !DEBUG */

	.text

	.globl	start
start:
	# start (aka boot1) is loaded at 0x0:0x7c00 but we want 0x6c0:0x1000
	# ljmp to the next instruction to adjust %cs
	data32
	ljmp $0x7c0, $2f
1:	.asciz	"loading /boot"

2:	/* set up stack(%ss:%esp) */
	cli			/* disable interrupts w/o stack */
	data32
	movl	$BOOTSEG, %ax
	movl	%ax, %ss
	data32
	movl	$BOOTSTACK, %esp
	sti			/* we have stack, do ints */

	/* set up %es, (where we will load /boot to) */
	movl	%ax, %es

	/* set up %ds */
	pushl	%cs
	popl	%ds


#ifdef SERIAL
	# Initialize the serial port to 9600 baud, 8N1.
	pushl	%dx
	movl	$0x00e3, %ax
	movl	SERIAL, %dx
	int	$0x14
	popl	%dx
#endif

	data32
	movl	$1b, %esi
	data32
	call	message

	data32
	xorl	%ebx, %ebx		/* put it at %es:0 */
	addr32
	movb	_block_count, %cl	/* how many to read */
	movzbl	%cl, %ecx
	decl	%ecx			/* loop requires sizeof()-1 */
	data32
	movl	$_block_table, %esi

1:
	pushl	%ecx
/*
# BIOS call "INT 0x13 Function 0x2" to read sectors from disk into memory
#       Call with       %ah = 0x2
#                       %al = number of sectors
#                       %ch = cylinder
#                       %cl = sector
#                       %dh = head
#                       %dl = drive (0x80 for hard disk, 0x0 for floppy disk)
#                       %es:%bx = segment:offset of buffer
#       Return:
#                       %al = 0x0 on success; err code on failure
*/
	movb	$'.', %al
	data32
	call	chr		/* show progress indicator */
	# movb	(%si), %al
	.byte   0x8a, 0x44, 0	/* # of sectors to load */
	incl	%esi
	# movb	(%si), %dh
	.byte   0x8a, 0x74, 0	/* head */
	incl	%esi
	# movw	(%si), %cx
	.byte   0x8b, 0x4c, 0	/* cylinder/sector */
	incl	%esi
	incl	%esi
	movb	$0x2, %ah
	int	$0x13
	jnc	4f
	/* read error */
       	data32
	movl    $2f, %esi
	data32
	call	message
	cli
	hlt
2:	.ascii	"Read error"
3:	.asciz	"\r\n"
4:	/* read next block */
	shll	$1, %ax
	addb	%al, %bh
	popl	%ecx
	loop	1b

       	data32
	movl    $3b, %esi	/* new line */
	data32
	call	message

	/* change to protected mode */
	/* guarantee that interrupt is disabled when in prot mode */
	cli

	/* load the gdtr */
	addr32
	data32
	lgdt	Gdtr

	/* set the PE bit of CR0 */
	movl	%cr0, %eax
	data32
	orl	$CR0_PE, %eax
	movl	%eax, %cr0 

	/*
	 * make intrasegment jump to flush the processor pipeline and
	 * reload CS register
	 */
	data32
	ljmp	$0x18, $1f

1:	/*
	 * USE16 mode
	 * set up the protected mode segment registers : DS, SS, ES
	 */
	movb	$0x10, %al
	movzbl	%al, %eax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	data32
	movzbl	%dl, %edx	/* discard head (%dh) and random high bits */
	data32
	pushl	%edx

	/* jump to the /boot and switch in USE32 mode */
	/* ljmp $0x08, _entry_point */
	data32
	.byte	0xea
	.globl	_entry_point
_entry_point:
	.long	0	/* /boot entry point */
	.word	0x0008	/* flat code */

#
#	chr: write the character in %al to console
#
chr:
/*
#ifndef SERIAL
# BIOS call "INT 10H Function 0Eh" to write character to console
#	Call with	%ah = 0x0e
#			%al = character
#			%bh = page
#			%bl = foreground color
#else
# BIOS call "INT 14H Function 01h" to write character to console
#	Call with	%ah = 0x01
#			%al = character
#			%dx = port number
#endif
*/
	data32
	pushl	%eax

#ifndef SERIAL
	data32
	pushl	%ebx
	movb	$0x0e, %ah
	xorl	%bx, %bx
	incl	%bx		/* movw $0x01, %bx */
	int	$0x10
	data32
	popl	%ebx
#else
	data32
	pushl	%edx
	movb	$0x01, %ah
	xorl	%dx, %dx
	movb	SERIAL, %dl
	int	$0x14
	data32
	popl	%edx
#endif
	data32
	popl	%eax
	data32
	ret

/*
 * Display string
 */
message:
	data32
	pushl	%eax
	cld
1:
	lodsb			# load a byte into %al
	testb	%al, %al
	jz	1f
	data32
	call	chr
	jmp	1b
1:
	data32
	popl	%eax
	data32
	ret

Gdtr:
	.word	2f + 4 * 8 - 1
	.long	2f
2:	/* Gdt */
	.word	0, 0
	.byte	0, 0, 0, 0		/* 0x0 : null */
	.word	0xFFFF, 0
	.byte	0, 0x9E, 0xCF, 0	/* 0x08 : flat code */
		/* 0x9F? */
	.word	0xFFFF, 0
	.byte	0, 0x92, 0xCF, 0	/* 0x10 : flat data */
		/* 0x93? */
	.word	0xFFFF, 0x7c00
	.byte	0, 0x9E, 0x00, 0	/* 0x18 : boot code 16 bits */

	.align	2
	.globl	_block_table
_block_table:
	.byte	0	/* nsect */
	.byte	0	/* head */
	.word	0	/* cyllinder/sector */
	. = _block_table + BLKCNT*4
	.globl	_block_count
_block_count:
	.byte	BLKCNT	/* entries in _block_table */

	/* a little signature */
	. = 0x200 - 4
	.ascii	"boot"

