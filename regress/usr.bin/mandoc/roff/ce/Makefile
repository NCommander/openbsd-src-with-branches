# $OpenBSD: Makefile,v 1.1 2019/01/04 01:06:44 schwarze Exp $

REGRESS_TARGETS	 = basic break

# mandoc defect:
# - For now, high level macros break .ce and .rj blocks.

SKIP_GROFF	 = break

.include <bsd.regress.mk>
