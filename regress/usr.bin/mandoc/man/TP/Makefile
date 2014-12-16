# $OpenBSD: Makefile,v 1.9 2014/07/07 21:35:42 schwarze Exp $

REGRESS_TARGETS	 = badarg broken double eof literal longhead
REGRESS_TARGETS	+= macrotag manyargs sameline width

LINT_TARGETS	 = broken double eof

# groff-1.22.3 defects:
# - If .TP precedes .RE, the latter does not properly reset indentation.
# - If the last line of the file is .TP, groff does not print a page footer.

SKIP_GROFF	 = broken eof

.include <bsd.regress.mk>
