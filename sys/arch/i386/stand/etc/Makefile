#	$OpenBSD: Makefile,v 1.8 1998/05/25 19:04:54 mickey Exp $

S=	${.CURDIR}/../../../..
I386=	${.CURDIR}/../..

all depend:: assym.h

assym.h: $S/kern/genassym.sh ${.CURDIR}/genassym.cf
	sh $S/kern/genassym.sh ${CC} ${CFLAGS} ${CPPFLAGS} \
	    ${PARAM} < ${.CURDIR}/genassym.cf > assym.h.tmp && \
	    mv -f assym.h.tmp assym.h

cleandir: clean

clean:
	@rm -f isa.h apm.h assym.h machine

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.include "../Makefile.inc"
CFLAGS+=${SACFLAGS}
