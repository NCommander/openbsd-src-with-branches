/*	$OpenBSD: biosboot.S,v 1.1.2.3 1996/12/16 15:19:29 mickey Exp $	*/

	.file	"bootbios.S"

#include <machine/asm.h>
#include <machine/specialreg.h>
#define _LOCORE
#include <machine/segments.h>
#undef _LOCORE

#define addr32  .byte 0x67
#define data32  .byte 0x66

#define	BLKCNT	40

#define BIOSSEG		0x07c0	/* boot loaded here */
#define BOOTSEG		0x1000	/* /boot placement */
#define BOOTSTACK	0xfffc	/* /boot stack */
#define OMAGIC		0x0701	/* OMAGIC */

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
	ljmp $BIOSSEG, $2f
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
#                       %cl = sector/cylinder(2 high bits)
#                       %dh = head
#                       %dl = drive (0x80 for hard disk, 0x0 for floppy disk)
#                       %es:%bx = segment:offset of buffer
#       Return:
#                       %al = 0x0 on success; err code on failure
*/
	movb	$'.', %al
	data32
	call	chr		/* show progress indicator */

	lodsl	/* word */	/* cylinder/sector */
	movl	%ax, %cx
	lodsb			/* head */
	movb	%al, %dh
	lodsb			/* # of sectors to load */
	movb	$0x2, %ah
	int	$0x13
	jnc	3f

	/* read error */
       	data32
	movl    $2f, %esi
	jmp	halt
2:	.asciz	"Read error\r\n"

3:	/* read next block */
	shll	$9, %ax		/* 512 bytes sectors */
	addl	%ax, %bx
	popl	%ecx
	loop	1b

	data32
	xorl	%esi, %esi
	data32
	xorl	%eax, %eax

	/* check /boot integrity */
	es;lodsl;es;lodsl	/* no need for high word */
	# cmpw	$OMAGIC, %ax
	.byte	0x3d
	.word	OMAGIC
	je	3f

	data32
	movl	$1f, %esi
	jmp	halt
1:	.ascii	"Bad magic"
2:	.asciz	"\r\n"

3:	data32
	movl    $2b, %esi	/* new line */
	data32
	call	message

#if 0
	data32;es;lodsl	/* text size */
	data32;es;lodsl	/* data size */
	data32;es;lodsl	/* bss  size */
	data32;es;lodsl	/* syms size */
#else
	data32
	addl	$16, %esi
#endif
	data32
	es;lodsl		/* entry */
	data32
	addl	$0x20, %eax	/* struct exec */
	data32
	addr32
	movl	%eax, entry_point

	/* change to protected mode */
	/* guarantee that interrupts are disabled when in prot mode */
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
	ljmp	$8, $1f+(BIOSSEG << 4)

1:	/*
	 * USE32 mode
	 * set up the protected mode segment registers : DS, SS, ES
	 */
	movl	$0x10, %eax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es

	movb	%dl, %al	/* discard head (%dh) and random high bits */
	pushl	%eax

	/* call	/boot */
	.byte	0x9a
entry_point:
	.long	0
	.word	8
halt:
	call	message
	cli
	hlt

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
	.word	3f - 1
	.long	(BIOSSEG << 4) + 2f
2:	/* Gdt */
		/* 0x00 : null */
	.long	0, 0
		/* 0x08 : flat code */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMERAC | 0 | 0x80	# RWXAC, dpl = 0, present
	.byte	0xf | 0 | 0x40 | 0x80	# hilimit, xx, 32bit, 4k granularity
	.byte	0			# hibase
		/* 0x10 : flat data */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMRWA | 0 | 0x80	# RWA, dpl = 0, present
	.byte	0xf | 0 | 0x40 | 0x80	# hilimit, xx, 32bit, 4k granularity
	.byte	0			# hibase
3:
	.align	2
	.globl	_block_table
_block_table:
	.word	0	/* cyllinder/sector */
	.byte	0	/* head */
	.byte	0	/* nsect */
	. = _block_table + BLKCNT*4
	.globl	_block_count
_block_count:
	.byte	BLKCNT	/* entries in _block_table */

	. = 0x200 - 4
	/* a little signature */
	.ascii	"boot"

