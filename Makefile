#	$OpenBSD: Makefile,v 1.88 2002/09/17 16:19:49 miod Exp $

#
# For more information on building in tricky environments, please see
# the list of possible environment variables described in
# /usr/share/mk/bsd.README.
#
# Building recommendations:
#
# 1) If at all possible, put this source tree in /usr/src.  If /usr/src
# must be a symbolic link, setenv BSDSRCDIR to point to the real location.
#
# 2) It is also recommended that you compile with objects outside the
# source tree. To do this, ensure /usr/obj exists or points to some
# area of disk of sufficient size.  Then do "cd /usr/src; make obj".
# This will make a symbolic link called "obj" in each directory, as
# well as populate the /usr/obj properly with directories for the
# objects.
#
# 3) It is strongly recommended that you build and install a new kernel
# before rebuilding your system. Some of the new programs may use new
# functionality or depend on API changes that your old kernel doesn't have.
#
# 4) If you are reasonably sure that things will compile OK, use the
# "make build" target supplied here. Good luck.
#
# 5) If you want to setup a cross-build environment, there is a "cross-tools"
# target available which upon completion of "make TARGET=<target> cross-tools"
# (where <target> is one of the names in the /sys/arch directory) will produce
# a set of compilation tools along with the includes in the /usr/cross/<target>
# directory. The "cross-distrib" target will build cross-tools as well as
# binaries for a given <target>.
#

.include <bsd.own.mk>	# for NOMAN, if it's there.

SUBDIR+= lib include bin libexec sbin usr.bin usr.sbin share games
SUBDIR+= gnu

SUBDIR+= sys lkm

.if (${KERBEROS:L} == "yes")
SUBDIR+= kerberosIV
.endif

.if (${KERBEROS5:L} == "yes")
SUBDIR+= kerberosV
.endif

.if   make(clean) || make(cleandir) || make(obj)
SUBDIR+= distrib regress
.endif

.if exists(regress)
regression-tests:
	@echo Running regression tests...
	@cd ${.CURDIR}/regress && ${MAKE} depend && exec ${MAKE} regress
.endif

includes:
	cd ${.CURDIR}/include && ${MAKE} prereq && exec ${SUDO} ${MAKE} includes

beforeinstall:
	cd ${.CURDIR}/etc && exec ${MAKE} DESTDIR=${DESTDIR} distrib-dirs
	cd ${.CURDIR}/include && exec ${MAKE} includes

afterinstall:
.ifndef NOMAN
	cd ${.CURDIR}/share/man && exec ${MAKE} makedb
.endif

build:
.ifdef GLOBAL_AUTOCONF_CACHE
	cp /dev/null ${GLOBAL_AUTOCONF_CACHE}
.endif
	cd ${.CURDIR}/share/mk && exec ${SUDO} ${MAKE} install
	cd ${.CURDIR}/include && ${MAKE} prereq && exec ${SUDO} ${MAKE} includes
	${SUDO} ${MAKE} cleandir
	cd ${.CURDIR}/lib && ${MAKE} depend && ${MAKE} && \
	    NOMAN=1 exec ${SUDO} ${MAKE} install
	cd ${.CURDIR}/gnu/lib && ${MAKE} depend && ${MAKE} && \
	    NOMAN=1 exec ${SUDO} ${MAKE} install
.if (${KERBEROS:L} == "yes")
	cd ${.CURDIR}/kerberosIV/lib && ${MAKE} depend && ${MAKE} && \
	    NOMAN=1 exec ${SUDO} ${MAKE} install
.endif
.if (${KERBEROS5:L} == "yes")
	cd ${.CURDIR}/kerberosV/lib && ${MAKE} depend && ${MAKE} && \
	    NOMAN=1 exec ${SUDO} ${MAKE} install
.endif
	cd ${.CURDIR}/gnu/usr.bin/perl && \
	    ${MAKE} -f Makefile.bsd-wrapper depend && \
	    ${MAKE} -f Makefile.bsd-wrapper perl.lib && \
	    exec ${SUDO} ${MAKE} -f Makefile.bsd-wrapper install.lib
	${MAKE} depend && ${MAKE} && exec ${SUDO} ${MAKE} install

.if !defined(TARGET)
cross-tools cross-distrib:
	echo "TARGET must be set"; exit 1
.else
cross-tools:	cross-includes cross-binutils cross-gcc cross-lib
cross-distrib:	cross-tools cross-bin cross-etc-root-var

CROSSCPPFLAGS?=	-nostdinc -I${CROSSDIR}/usr/include
CROSSLDFLAGS?=	-nostdlib -L${CROSSDIR}/usr/lib -static
CROSSCFLAGS?=	${CROSSCPPFLAGS}
CROSSCXXFLAGS?=	${CROSSCPPFLAGS}
LDSTATIC?=	-static

CROSSDIR=	${DESTDIR}/usr/cross/${TARGET}
CROSSENV=	AR=${CROSSDIR}/usr/bin/ar AS=${CROSSDIR}/usr/bin/as \
		CC=${CROSSDIR}/usr/bin/cc CPP=${CROSSDIR}/usr/bin/cpp \
		CXX=${CROSSDIR}/usr/bin/c++ \
		LD=${CROSSDIR}/usr/bin/ld NM=${CROSSDIR}/usr/bin/nm \
		LORDER=/usr/bin/lorder RANLIB=${CROSSDIR}/usr/bin/ranlib \
		SIZE=${CROSSDIR}/usr/bin/size STRIP=${CROSSDIR}/usr/bin/strip \
		HOSTCC=\"${CC}\" HOSTCXX=\"${CXX}\" NOMAN= DESTDIR=${CROSSDIR} \
		HOSTCFLAGS=\"${CFLAGS}\" HOSTCXXFLAGS=\"${CXXFLAGS}\" \
		HOSTLDFLAGS=\"${LDFLAGS} \" \
		CFLAGS=\"${CROSSCFLAGS}\" CPPFLAGS=\"${CROSSCPPFLAGS}\" \
		CXXFLAGS=\"${CROSSCXXFLAGS}\" \
		LDFLAGS=\"${CROSSLDFLAGS}\"
CROSSPATH=	${PATH}:${CROSSDIR}/usr/bin
CROSSLANGS?=	c c++

CROSSDIRS=	${CROSSDIR}/.dirs_done
CROSSOBJ=	${CROSSDIR}/usr/obj/.obj_done
CROSSINCLUDES=	${CROSSDIR}/usr/include/.includes_done
CROSSBINUTILS=	${CROSSDIR}/usr/bin/.binutils_done
CROSSGCC=	${CROSSDIR}/usr/bin/.gcc_done
NO_CROSS=	isakmpd tn3270 less sudo openssl libkeynote libssl \
		photurisd keynote sectok ssh

cross-dirs:	${CROSSDIRS}
cross-obj:	${CROSSOBJ}
cross-includes:	${CROSSINCLUDES}
cross-binutils:	${CROSSBINUTILS}
cross-gcc:	${CROSSGCC}

cross-env:	.PHONY
	@echo ${CROSSENV} MACHINE=${TARGET} \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`

${CROSSDIRS}:
	@-mkdir -p ${CROSSDIR}
	@case ${TARGET} in \
		sparc|i386|m68k|alpha|hppa|powerpc|sparc64|m88k|vax) \
			echo ${TARGET} ;;\
		amiga|sun3|mac68k|hp300|mvme68k) \
			echo m68k ;;\
		mvme88k) \
			echo m88k ;;\
		mvmeppc|macppc) \
			echo powerpc ;;\
		sgi) \
			echo mips ;;\
		*) \
			(echo Unknown arch ${TARGET} >&2) ; exit 1;; \
	esac > ${CROSSDIR}/TARGET_ARCH
	@echo TARGET_ARCH is `cat ${CROSSDIR}/TARGET_ARCH`
	@eval `grep '^osr=' sys/conf/newvers.sh`; \
	   sed "s/\$$/-unknown-openbsd$$osr/" ${CROSSDIR}/TARGET_ARCH > \
	   ${CROSSDIR}/TARGET_CANON
	@-mkdir -p ${CROSSDIR}
	@-mkdir -p ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`
	@ln -sf ${CROSSDIR}/usr/include \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/include
	@ln -sf ${CROSSDIR}/usr/lib \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/lib
	@-mkdir -p ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin
	@(cd ${.CURDIR}/etc && DESTDIR=${CROSSDIR} ${MAKE} distrib-dirs)
	@touch ${CROSSDIRS}

${CROSSOBJ}:	${CROSSDIRS}
	@-mkdir -p ${CROSSDIR}/usr/obj
	@(cd ${.CURDIR} && \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    MACHINE=${TARGET} \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} obj)
	@touch ${CROSSOBJ}

${CROSSINCLUDES}:	${CROSSOBJ}
	@-mkdir -p ${CROSSDIR}/usr/include
	@(cd ${.CURDIR}/include && \
	    MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    ${MAKE} prereq && \
	    MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    ${MAKE} DESTDIR=${CROSSDIR} includes)
	@touch ${CROSSINCLUDES}

.if ${TARGET} == "alpha" || ${TARGET} == "hppa" || ${TARGET} == "macppc" || \
    ${TARGET} == "mvmeppc" || ${TARGET} == "sgi" || ${TARGET} == "sparc" || \
    ${TARGET} == "sparc64"
BINUTILS=	ar as gasp ld nm objcopy objdump ranlib readelf size \
		strings strip
NEW_BINUTILS?=	Yes
.else
BINUTILS=	ar as ld nm ranlib objcopy objdump size strings strip
NEW_BINUTILS?=	No
.endif

${CROSSBINUTILS}:	${CROSSINCLUDES}
.if ${NEW_BINUTILS:L} == "yes"
	export BSDSRCDIR=${.CURDIR}; \
	    (cd ${CROSSDIR}/usr/obj/gnu/usr.bin/binutils; \
	    /bin/sh ${BSDSRCDIR}/gnu/usr.bin/binutils/configure \
	    --prefix ${CROSSDIR}/usr \
	    --disable-nls --disable-gdbtk --disable-commonbfdlib \
	    --target `cat ${CROSSDIR}/TARGET_CANON` && \
	    ${MAKE} CFLAGS="${CFLAGS}" && ${MAKE} install )
.else
	(cd ${.CURDIR}/gnu/usr.bin/gas; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} depend all; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/as \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/as
	(cd ${.CURDIR}/gnu/usr.bin/ld; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOPIC= NOMAN= depend all; \
	    TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOPIC= NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ld \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ld
	(cd ${.CURDIR}/usr.bin/ar; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ar \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ar
	(cd ${.CURDIR}/usr.bin/ranlib; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} ${MAKE} NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/ranlib \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/ranlib
	(cd ${.CURDIR}/usr.bin/strip; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/strip \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/strip
	(cd ${.CURDIR}/usr.bin/size; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/size \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/size
	(cd ${.CURDIR}/usr.bin/nm; \
	    MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} TARGET_MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH` \
	    NOMAN= depend all; \
	    DESTDIR=${CROSSDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} NOMAN= install)
	ln -sf ${CROSSDIR}/usr/bin/nm \
	    ${CROSSDIR}/usr/`cat ${CROSSDIR}/TARGET_CANON`/bin/nm
.endif
	@for cmd in ${BINUTILS}; do \
	 if [ ! -e ${CROSSDIR}/usr/bin/$$cmd -a \
	 -e ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd ]; then \
	    ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd \
	        ${CROSSDIR}/usr/bin/$$cmd ;\
	 elif [ -e ${CROSSDIR}/usr/bin/$$cmd -a \
	 ! -e ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd ]; then \
	    ln -sf ${CROSSDIR}/usr/bin/$$cmd \
	        ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-$$cmd; \
	 fi ;\
	done
	@touch ${CROSSBINUTILS}

${CROSSGCC}:		${CROSSBINUTILS}
	(cd ${CROSSDIR}/usr/obj/gnu/egcs/gcc; \
	    /bin/sh ${.CURDIR}/gnu/egcs/gcc/configure \
	    --prefix ${CROSSDIR}/usr \
	    --target `cat ${CROSSDIR}/TARGET_CANON` \
	    --with-gxx-include-dir=${CROSSDIR}/usr/include/g++ && \
	    PATH=${CROSSPATH} ${MAKE} BISON=yacc LANGUAGES="${CROSSLANGS}" \
	    LDFLAGS="${LDSTATIC}" build_infodir=. \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" && \
	    ${MAKE} BISON=yacc LANGUAGES="${CROSSLANGS}" LDFLAGS="${LDSTATIC}" \
	    GCC_FOR_TARGET="./xgcc -B./ -I${CROSSDIR}/usr/include" \
	    build_infodir=. INSTALL_MAN= INSTALL_HEADERS_DIR= install)
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-gcc \
	    ${CROSSDIR}/usr/bin/cc
	ln -sf ${CROSSDIR}/usr/bin/`cat ${CROSSDIR}/TARGET_CANON`-g++ \
	    ${CROSSDIR}/usr/bin/c++
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${CROSSDIR}/usr/obj/gnu/egcs/gcc/cpp \
	    ${CROSSDIR}/usr/libexec/cpp
	sed -e 's#/usr/libexec/cpp#${CROSSDIR}/usr/libexec/cpp#' \
	    -e 's#/usr/include#${CROSSDIR}/usr/include#' \
	    ${.CURDIR}/usr.bin/cpp/cpp.sh > ${CROSSDIR}/usr/bin/cpp
	chmod ${BINMODE} ${CROSSDIR}/usr/bin/cpp
	chown ${BINOWN}:${BINGRP} ${CROSSDIR}/usr/bin/cpp
	@touch ${CROSSGCC}

# XXX MAKEOBJDIR maybe should be obj.${TARGET} here, revisit later
cross-lib:	${CROSSGCC}
	MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`; \
	export MACHINE MACHINE_ARCH; \
	(cd ${.CURDIR}/lib; \
	    for lib in csu libc; do \
	    (cd $$lib; \
	        ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
		    ${MAKE} depend all install); \
	    done; \
	    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	        SKIPDIR="${NO_CROSS} libocurses/PSD.doc" \
	        ${MAKE} depend all install)
.if (${KERBEROS:L} == "yes")
	MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`; \
	export MACHINE MACHINE_ARCH; \
	cd kerberosIV/lib; \
	${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} depend all install
.endif
.if (${KERBEROS5:L} == "yes")
	MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`; \
	export MACHINE MACHINE_ARCH; \
	cd kerberosV/lib; \
	${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    ${MAKE} depend all install
.endif

cross-bin:	${CROSSOBJ}
	MACHINE=${TARGET} MACHINE_ARCH=`cat ${CROSSDIR}/TARGET_ARCH`; \
	export MACHINE MACHINE_ARCH; \
	for i in libexec bin sbin usr.bin usr.sbin; do \
	(cd ${.CURDIR}/$$i; \
	    ${CROSSENV} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	        SKIPDIR="${BINUTILS} ${NO_CROSS}" \
	        ${MAKE} depend all install); \
	done

cross-etc-root-var:	${CROSSOBJ}
	(cd ${.CURDIR}/etc && \
	    DESTDIR=${CROSSDIR} ${MAKE} distribution-etc-root-var)

cross-depend:	.PHONY
	@(cd ${.CURDIR} && \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    SKIPDIR="${NO_CROSS}" \
	    ${MAKE} depend)

cross-clean:	.PHONY
	@(cd ${.CURDIR} && \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    SKIPDIR="${NO_CROSS}" \
	    ${MAKE} clean)

cross-cleandir:	.PHONY
	@(cd ${.CURDIR} && \
	    BSDOBJDIR=${CROSSDIR}/usr/obj \
	    BSDSRCDIR=${.CURDIR} MAKEOBJDIR=obj.${MACHINE}.${TARGET} \
	    SKIPDIR="${NO_CROSS}" \
	    ${MAKE} cleandir)

.endif # defined(TARGET)
 
.include <bsd.subdir.mk>
