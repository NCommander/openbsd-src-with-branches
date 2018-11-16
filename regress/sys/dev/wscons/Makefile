# $OpenBSD$

PROG=	sigio

WARNINGS=	yes

.for d in wskbd0 wsmouse0 wskbd
.  for t in setown spgrp sigio
REGRESS_TARGETS+=	$d-$t
$d-$t: ${PROG}
	@echo "\n======== ${@} ========"
	if ./${PROG} -p -d /dev/$d $t; then \
		./${PROG} -d /dev/$d $t; \
	else \
		echo "SKIPPED"; \
	fi
.  endfor
.endfor

# SIGIO is limited to wsmux devices.
REGRESS_SKIP_TARGETS+=	wskbd0-sigio wsmouse0-sigio

.include <bsd.regress.mk>
