#	$OpenBSD: Makefile.inc,v 1.4 1995/02/27 12:54:33 cgd Exp $

# gmon sources
.PATH: ${.CURDIR}/gmon

SRCS+=	gmon.c mcount.c
MAN+=	moncontrol.3

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
