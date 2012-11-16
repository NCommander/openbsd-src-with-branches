# $OpenBSD: Makefile,v 1.6 2012/07/07 14:10:55 schwarze Exp $

REGRESS_TARGETS = empty blank offset-empty spacing break broken

# groff-1.20.1 defect:    
# - a display breaking another block continues indefinitely

SKIP_GROFF = break

SKIP_TMAN = break broken

.include <bsd.regress.mk>
