#	$OpenBSD: bsd.port.subdir.mk,v 1.14 1996/04/09 22:54:13 wosch Exp $

.MAIN: all

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif


ECHO_MSG?=	echo

_SUBDIRUSE: .USE
	@for entry in ${SUBDIR}; do \
		OK=""; \
		for dud in $$DUDS; do \
			if [ $${dud} = $${entry} ]; then \
				OK="false"; \
				${ECHO_MSG} "===> ${DIRPRFX}$${entry} skipped"; \
			fi; \
		done; \
		if [ "$$OK" = "" ]; then \
			if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
				${ECHO_MSG} "===> ${DIRPRFX}$${entry}.${MACHINE}"; \
				edir=$${entry}.${MACHINE}; \
				cd ${.CURDIR}/$${edir}; \
			else \
				${ECHO_MSG} "===> ${DIRPRFX}$$entry"; \
				edir=$${entry}; \
				cd ${.CURDIR}/$${edir}; \
			fi; \
			${MAKE} ${.TARGET:realinstall=install} \
				DIRPRFX=${DIRPRFX}$$edir/; \
		fi; \
	done

${SUBDIR}::
	@if test -d ${.TARGET}.${MACHINE}; then \
		cd ${.CURDIR}/${.TARGET}.${MACHINE}; \
	else \
		cd ${.CURDIR}/${.TARGET}; \
	fi; \
	${MAKE} all

.for __target in all fetch fetch-list package extract configure \
		 build clean depend describe reinstall tags checksum
.if !target(__target)
${__target}: _SUBDIRUSE
.endif
.endfor

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif
install: afterinstall
afterinstall: realinstall
realinstall: beforeinstall _SUBDIRUSE
.endif

.if !target(readmes)
readmes: readme _SUBDIRUSE
.endif

.if !target(readme)
readme:
	@rm -f README.html
	@make README.html
.endif

PORTSDIR ?= /usr/ports
TEMPLATES ?= ${PORTSDIR}/templates
.if defined(PORTSTOP)
README=	${TEMPLATES}/README.top
.else
README=	${TEMPLATES}/README.category
.endif

README.html:
	@echo "===>  Creating README.html"
	@> $@.tmp
.for entry in ${SUBDIR}
.if defined(PORTSTOP)
	@echo -n '<a href="'${entry}/README.html'">${entry}</a>: ' >> $@.tmp
.else
	@echo -n '<a href="'${entry}/README.html'">'"`cd ${entry}; make package-name`</a>: " >> $@.tmp
.endif
.if exists(${entry}/pkg/COMMENT)
	@cat ${entry}/pkg/COMMENT >> $@.tmp
.else
	@echo "(no description)" >> $@.tmp
.endif
.endfor
	@sort -t '>' +1 -2 $@.tmp > $@.tmp2
	@cat ${README} | \
		sed -e 's%%CATEGORY%%'`echo ${.CURDIR} | sed -e 's.*/\([^/]*\)$$\1'`'g' \
			-e '/%%DESCR%%/r${.CURDIR}/pkg/DESCR' \
			-e '/%%DESCR%%/d' \
			-e '/%%SUBDIR%%/r$@.tmp2' \
			-e '/%%SUBDIR%%/d' \
		> $@
	@rm -f $@.tmp $@.tmp2
