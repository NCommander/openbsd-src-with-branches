#	$OpenBSD$

PROG=	biosboot
SRCS=	biosboot.S
AFLAGS=	-DDEBUG -Wa,-a
LDFLAGS=-Wl,-T0,-N,-x -nostdlib -e start
STRIP=
MAN=	biosboot.8
BINDIR=/usr/mdec
LINKS=	${BINDIR}/biosboot ${BINDIR}/fdboot \
	${BINDIR}/biosboot ${BINDIR}/wdboot \
	${BINDIR}/biosboot ${BINDIR}/sdboot \

# Uncomment this to make the boot block talk to a serial port.
# CPPFLAGS+=-DSERIAL=0

.include <bsd.prog.mk>
