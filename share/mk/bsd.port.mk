#-*- mode: Fundamental; tab-width: 4; -*-
# ex:ts=4
#	$OpenBSD: bsd.port.mk,v 1.74 1999/03/03 04:16:03 marc Exp $
#
#	bsd.port.mk - 940820 Jordan K. Hubbard.
#	This file is in the public domain.
#
# FreeBSD Id: bsd.port.mk,v 1.264 1996/12/25 02:27:44 imp Exp
#	$NetBSD: bsd.port.mk,v 1.62 1998/04/09 12:47:02 hubertf Exp $
#
# Please view me with 4 column tabs!

# There are two different types of "maintainers" in the whole ports
# framework concept.  Maintainers of the bsd.port*.mk files
# are listed below in the ${OPSYS}_MAINTAINER entries (this file
# is used by multiple *BSD flavors).  You should consult them directly
# if you have any questions/suggestions regarding this file since only
# they are allowed to modify the master copies in the CVS repository!

# For each port, the MAINTAINER variable is what you should consult for
# contact information on the person(s) to contact if you have questions/
# suggestions about that specific port.  By default (if no MAINTAINER
# is listed), a port is maintained by the subscribers of the ports@openbsd.org
# mailing list, and any correspondence should be directed there.  
#
OpenBSD_MAINTAINER=	marc@OpenBSD.ORG

# NEED_VERSION: we need at least this version of bsd.port.mk for this 
# port  to build

FULL_REVISION=$$OpenBSD: bsd.port.mk,v 1.74 1999/03/03 04:16:03 marc Exp $$
.if defined(NEED_VERSION)
_VERSION_REVISION=${FULL_REVISION:M[0-9]*.*}

_VERSION=${_VERSION_REVISION:C/\..*//}
_REVISION=${_VERSION_REVISION:C/.*\.//}

_VERSION_NEEDED=${NEED_VERSION:C/\..*//}
_REVISION_NEEDED=${NEED_VERSION:C/.*\.//}

.BEGIN:
	@if [ ${_VERSION_NEEDED} -gt ${_VERSION} -o \
			${_VERSION_NEEDED} -eq ${_VERSION} -a \
				${_REVISION_NEEDED} -gt ${_REVISION} ]; \
	then \
		${ECHO} "Need version ${NEED_VERSION} of bsd.port.mk"; \
		${FALSE}; \
    fi; 

.endif

# Supported Variables and their behaviors:
#
# Variables that typically apply to all ports:
# 
# ONLY_FOR_ARCHS - If a port only makes sense to certain architectures, this
#				  is a list containing the names for them.  It is checked
#				  against the predefined ${MACHINE_ARCH} value
# ARCH			- The architecture (default: "uname -m").
# OPSYS			- The operating system (default: "uname -s").
# OPSYS_VER		- The current version of the operating system
#				  (default: "uname -r").
# PORTSDIR		- The root of the ports tree.  Defaults: /usr/ports
# DISTDIR 		- Where to get gzip'd, tarballed copies of original sources.
#				  (default: ${PORTSDIR}/distfiles).
# PREFIX		- Where to install things in general (default: /usr/local).
# MASTER_SITES	- Primary location(s) for distribution files if not found
#				  locally.
# MASTER_SITE_SUBDIR - Directory that "%SUBDIR%" in MASTER_SITES is
#				  replaced by.
# PATCH_SITES	- Primary location(s) for distribution patch files
#				  (see PATCHFILES below) if not found locally.
# PATCH_SITE_SUBDIR - Directory that "%SUBDIR%" in PATCH_SITES is
#				  replaced by.
#
# MASTER_SITE_BACKUP - Backup location(s) for distribution files and patch
#				  files if not found locally and ${MASTER_SITES}/${PATCH_SITES}
#				  (default:
#				  ftp://ftp.openbsd.org/pub/OpenBSD/distfiles/${DIST_SUBDIR}/
#				  ftp://ftp.openbsd.org/pub/OpenBSD/licensed/${DIST_SUBDIR}/
#				  ftp://ftp.freebsd.org/pub/FreeBSD/distfiles/${DIST_SUBDIR}/)
# MASTER_SITE_OVERRIDE - If set, override the MASTER_SITES setting with this
#				  value.
# MASTER_SITE_OPENBSD - If set, only use ftp.openbsd.org as the
#				  MASTER_SITE_OVERRIDE.
# PACKAGES		- A top level directory where all packages go (rather than
#				  going locally to each port). (default: ${PORTSDIR}/packages).
# GMAKE			- Set to path of GNU make if not in $PORTPATH (default: gmake).
# XMKMF			- Set to path of `xmkmf' if not in $PORTPATH 
#                 (default: xmkmf -a ).
# MAINTAINER	- The e-mail address of the contact person for this port
#				  Defaults: ports@OpenBSD.ORG
# CATEGORIES	- A list of descriptive categories into which this port falls.
# WRKOBJDIR		- A top level directory where, if defined, the separate working
#				  directories will get created, and symbolically linked to from
#				  ${WRKDIR} (see below).  This is useful for building ports on
#				  several architectures, then ${PORTSDIR} can be NFS-mounted
#				  while ${WRKOBJDIR} is local to every arch
# PREFERRED_CIPHERS
#				- a list of the form cipher.sig of programs to use to check
#				  recorded checksums, in order of decreasing trust.
#				  (default to using sha1, then rmd160, then md5).
#
# Variables that typically apply to an individual port.  Non-Boolean
# variables without defaults are *mandatory*.
#
# WRKDIR 		- A temporary working directory that gets *clobbered* on clean
#				  (default: ${.CURDIR}/work).
# WRKSRC		- A subdirectory of ${WRKDIR} where the distribution actually
#				  unpacks to.  (Default: ${WRKDIR}/${DISTNAME} unless
#				  NO_WRKSUBDIR is set, in which case simply ${WRKDIR}).
# WRKBUILD		- The directory where the port is actually built, useful for 
#                 ports that need a separate directory (default: ${WRKSRC}).
#				  This is intended for GNU configure.
# SEPARATE_BUILD
#               - define if the port can build in directory separate from
#                 WRKSRC. This redefines WRKBUILD to be arch-dependent,
#                 along with the configure, build and install cookies
# DISTNAME		- Name of port or distribution.
# DISTFILES		- Name(s) of archive file(s) containing distribution
#				  (default: ${DISTNAME}${EXTRACT_SUFX}).
# PATCHFILES	- Name(s) of additional files that contain distribution
#				  patches (default: none).  make will look for them at
#				  PATCH_SITES (see above).  They will automatically be
#				  uncompressed before patching if the names end with
#				  ".gz" or ".Z".
# DIST_SUBDIR	- Suffix to ${DISTDIR}.  If set, all ${DISTFILES} 
#				  and ${PATCHFILES} will be put in this subdirectory of
#				  ${DISTDIR}.  Also they will be fetched in this subdirectory 
#				  from FreeBSD mirror sites.
# ALLFILES		- All of ${DISTFILES} and ${PATCHFILES}.
# MIRROR_DISTFILE - Whether the distfile is redistributable without restrictions.
#				  Defaults to "yes", set this to "no" if restrictions exist.
# IGNOREFILES	- If some of the ${ALLFILES} are not checksum-able, set
#				  this variable to their names.
# PKGNAME		- Name of the package file to create if the DISTNAME 
#				  isn't really relevant for the port/package
#				  (default: ${DISTNAME}).
# EXTRACT_ONLY	- If defined, a subset of ${DISTFILES} you want to
#			  	  actually extract.
# PATCHDIR 		- A directory containing any additional patches you made
#				  to port this software to OpenBSD (default:
#				  ${.CURDIR}/patches)
# PATCH_LIST	- list of patches to apply, can include wildcards (default:
#                 patch-*)
# SCRIPTDIR 	- A directory containing any auxiliary scripts
#				  (default: ${.CURDIR}/scripts)
# FILESDIR 		- A directory containing any miscellaneous additional files.
#				  (default: ${.CURDIR}/files)
# PKGDIR 		- A direction containing any package creation files.
#				  (default: ${.CURDIR}/pkg)
# PKG_DBDIR		- Where package installation is recorded (default: /var/db/pkg)
# FORCE_PKG_REGISTER - If set, it will overwrite any existing package
#				  registration information in ${PKG_DBDIR}/${PKGNAME}.
# NO_MTREE		- If set, will not invoke mtree from bsd.port.mk from
#				  the "install" target.
# MTREE_FILE	- The name of the mtree file (default: /etc/mtree/BSD.x11.dist
#				  if USE_IMAKE or USE_X11 is set, /etc/mtree/BSD.local.dist
#				  otherwise.)
# COMES_WITH	- The first version that a port was made part of the
#				  standard OpenBSD distribution.  If the current OpenBSD
#				  version is >= this version then a notice will be
#				  displayed instead the port being generated.
#
# NO_BUILD		- Use a dummy (do-nothing) build target.
# NO_CONFIGURE	- Use a dummy (do-nothing) configure target.
# NO_CDROM		- Port may not go on CDROM.  Set this string to reason.
# NO_DESCRIBE	- Use a dummy (do-nothing) describe target.
# NO_EXTRACT	- Use a dummy (do-nothing) extract target.
# NO_INSTALL	- Use a dummy (do-nothing) install target.
# NO_PACKAGE	- Use a dummy (do-nothing) package target.
# NO_PKG_REGISTER - Don't register a port install as a package.
# NO_WRKSUBDIR	- Assume port unpacks directly into ${WRKDIR}.
# NO_WRKDIR		- There's no work directory at all; port does this someplace
#				  else.
# NO_DEPENDS	- Don't verify build of dependencies.
# NOCLEANDEPENDS - Don't nuke dependent dirs on make clean (Default: yes)
# BROKEN		- Port is broken.  Set this string to the reason why.
# RESTRICTED	- Port is restricted.  Set this string to the reason why.
# USE_GMAKE		- Says that the port uses gmake.
#
# XXX: cygnus products do NOT use autoconf for making its main 
#      configure from configure.in
# USE_AUTOCONF	- Says that the port uses autoconf (implies GNU_CONFIGURE).
# AUTOCONF_DIR  - Where to apply autoconf (default: ${WRKSRC}).
# USE_PERL5		- Says that the port uses perl5 for building and running.
# USE_IMAKE		- Says that the port uses imake.
# USE_X11		- Says that the port uses X11 (i.e., installs in ${X11BASE}).
# USE_EGCC		- Says that the port needs the egcs C compiler
# USE_EGXX		- Says that the port needs the egcs C++ compiler
# NO_INSTALL_MANPAGES - For imake ports that don't like the install.man
#						target.
# HAS_CONFIGURE	- Says that the port has its own configure script.
# GNU_CONFIGURE	- Set if you are using GNU configure (optional).
# CONFIGURE_SCRIPT - Name of configure script, defaults to 'configure'.
# CONFIGURE_ARGS - Pass these args to configure if ${HAS_CONFIGURE} is set.
# CONFIGURE_SHARED - An argument to GNU configure that expands to
#				  --enable-shared for those architectures that support
#				  shared libraries and --disable-shared for architectures
#				  that do not support shared libraries.
# CONFIGURE_ENV - Pass these env (shell-like) to configure if
#				  ${HAS_CONFIGURE} is set.
# SCRIPTS_ENV	- Additional environment vars passed to scripts in
#                 ${SCRIPTDIR} executed by bsd.port.mk.
# MAKE_ENV		- Additional environment vars passed to sub-make in build
#				  stage.
# IS_INTERACTIVE - Set this if your port needs to interact with the user
#				  during a build.  User can then decide to skip this port by
#				  setting ${BATCH}, or compiling only the interactive ports
#				  by setting ${INTERACTIVE}.
# FETCH_DEPENDS - A list of "path:dir" pairs of other ports this
#				  package depends in the "fetch" stage.  "path" is the
#				  name of a file if it starts with a slash (/), an
#				  executable otherwise.  make will test for the
#				  existence (if it is a full pathname) or search for
#				  it in $PORTPATH (if it is an executable) and go
#				  into "dir" to do a "make all install" if it's not
#				  found.
# BUILD_DEPENDS - A list of "path:dir" pairs of other ports this
#				  package depends to build (between the "extract" and
#				  "build" stages, inclusive).  The test done to
#				  determine the existence of the dependency is the
#				  same as FETCH_DEPENDS.
# RUN_DEPENDS	- A list of "path:dir" pairs of other ports this
#				  package depends to run.  The test done to determine
#				  the existence of the dependency is the same as
#				  FETCH_DEPENDS.  This will be checked during the
#				  "install" stage and the name of the dependency will
#				  be put into the package as well.
# LIB_DEPENDS	- A list of "lib:dir" pairs of other ports this package
#				  depends on.  "lib" is the name of a shared library.
#				  make will use "ldconfig -r" to search for the
#				  library.  Note that lib can be any regular expression.
#				  In older versions of this file, you need two backslashes
#				  in front of dots (.) to supress its special meaning (e.g.,
#				  use "foo\\.2\\.:${PORTSDIR}/utils/foo" to match "libfoo.2.*").
#				  No special backslashes are needed to escape regular
#				  expression metacharacters in OpenBSD, and the old backslash
#				  escapes are recognised for backwards compatibility.
# DEPENDS		- A list of other ports this package depends on being
#				  made first.  Use this for things that don't fall into
#				  the above two categories.
# EXTRACT_CMD	- Command for extracting archive (default: tar).
# EXTRACT_SUFX	- Suffix for archive names (default: .tar.gz).
# EXTRACT_BEFORE_ARGS -
#				  Arguments to ${EXTRACT_CMD} before filename
#				  (default: -xzf).
# EXTRACT_AFTER_ARGS -
#				  Arguments to ${EXTRACT_CMD} following filename
#				  (default: none).
#
# FETCH_CMD		  - Full path to ftp/http fetch command if not in $PORTPATH
#				  (default: /usr/bin/ftp).
# FETCH_BEFORE_ARGS -
#				  Arguments to ${FETCH_CMD} before filename (default: none).
# FETCH_AFTER_ARGS -
#				  Arguments to ${FETCH_CMD} following filename (default: none).
# NO_IGNORE     - Set this to YES (most probably in a "make fetch" in
#                 ${PORTSDIR}) if you want to fetch all distfiles,
#                 even for packages not built due to limitation by
#                 absent X or Motif or ONLY_FOR_ARCHS...
# NO_WARNINGS	- Set this to YES to disable warnings regarding variables
#				  to define to control the build.  Automatically set
#				  from the "mirror-distfiles" target.
# ALL_TARGET	- The target to pass to make in the package when building.
#				  (default: "all")
# INSTALL_TARGET- The target to pass to make in the package when installing.
#				  (default: "install")
#
# Motif support:
#
# USE_MOTIF		- Set this in your port if it requires Motif or Lesstif.
#				  It will be built using Lesstif port unless Motif libraries
#				  found or HAVE_MOTIF is defined. See also REQUIRES_MOTIF.
#
# REQUIRES_MOTIF- Set this in your port if it requires Motif.  It will  be
#				  built only if HAVE_MOTIF is set.
# HAVE_MOTIF	- If set, means system has Motif.  Typically set in /etc/mk.conf.
# MOTIF_STATIC	- If set, link libXm statically; otherwise, link it
#				  dynamically.  Typically set in /etc/mk.conf.
# MOTIFLIB		- Set automatically to appropriate value depending on
#				  ${MOTIF_STATIC}.  Substitute references to -lXm with 
#				  patches to make your port conform to our standards.
# MOTIF_ONLY	- If set, build Motif ports only.  (Not much use except for
#				  building packages.)
#
# Variables to change if you want a special behavior:
#
# ECHO_MSG		- Used to print all the '===>' style prompts - override this
#				  to turn them off (default: /bin/echo).
# DEPENDS_TARGET - The target to execute when a port is calling a
#				  dependency (default: "install").
# PATCH_DEBUG	- If set, print out more information about the patches as
#				  it attempts to apply them.
#
# Variables that serve as convenient "aliases" for your *-install targets.
# Use these like: "${INSTALL_PROGRAM} ${WRKSRC}/prog ${PREFIX}/bin".
#
# INSTALL_PROGRAM		- A command to install binary executables.
# INSTALL_SCRIPT		- A command to install executable scripts.
# INSTALL_DATA			- A command to install sharable data.
# INSTALL_MAN			- A command to install manpages (doesn't compress).
# INSTALL_PROGRAM_DIR	- Create a directory for storing programs
# INSTALL_SCRIPT_DIR	- Create a directory for storing scripts (alias for
#						  (INSTALL_PROGRAM_DIR)
# INSTALL_DATA_DIR		- Create a directory for storing arbitrary data
# INSTALL_MAN_DIR		- Create a directory for storing man pages
#
# It is assumed that the port installs manpages uncompressed. If this is
# not the case, set MANCOMPRESSED in the port and define MAN<sect> and
# CAT<sect> for the compressed pages.  The pages will then be automagically
# uncompressed.
#
# MANCOMPRESSED - Indicates that the port installs manpages in a compressed
#                 form (default: port installs manpages uncompressed).
# MAN<sect>		- A list of manpages, categorized by section.  For
#				  example, if your port has "man/man1/foo.1" and
#				  "man/mann/bar.n", set "MAN1=foo.1" and "MANN=bar.n".
#				  The available sections chars are "123456789LN".
# CAT<sect>     - The same as MAN<sect>, only for formatted manpages.
# MANPREFIX		 -The directory prefix for ${MAN<sect>} (default: ${PREFIX}).
# CATPREFIX     - The directory prefix for ${CAT<sect>} (default: ${PREFIX}).
#
# Other variables:
#
# NO_SHARED_LIBS - defined as "yes" for those machine architectures that do
#				  not support shared libraries.  WARNING: This value is
#				  NOT defined until AFTER ".include bsd.port.mk".  Thus
#				  you can NOT use something like ".if defined(NO_SHARED_LIBS)"
#				  before this file is included.
#
# Default targets and their behaviors:
#
# fetch			- Retrieves ${DISTFILES} (and ${PATCHFILES} if defined)
#				  into ${DISTDIR} as necessary.
# fetch-list	- Show list of files that would be retrieved by fetch
# extract		- Unpacks ${DISTFILES} into ${WRKDIR}.
# patch			- Apply any provided patches to the source.
# configure		- Runs either GNU configure, one or more local configure
#				  scripts or nothing, depending on what's available.
# build			- Actually compile the sources.
# install		- Install the results of a build.
# reinstall		- Install the results of a build, ignoring "already installed"
#				  flag.
# deinstall		- Remove the installation.  Alias: uninstall
# plist			- create a file suitable for use as a packing list.  This
#				  is for port maintainers.
# package		- Create a package from an _installed_ port.
# describe		- Try to generate a one-line description for each port for
#				  use in INDEX files and the like.
# checkpatch	- Do a "patch -C" instead of a "patch".  Note that it may
#				  give incorrect results if multiple patches deal with
#				  the same file.
# checksum		- Use ${CHECKSUM_FILE} to ensure that your distfiles are valid.
# makesum		- Generate ${CHECKSUM_FILE} (only do this for your own ports!).
# addsum		- update ${CHECKSUM_FILE} in a non-destructive way 
#				  (your own ports only!)
# readme		- Create a README.html file describing the category or package
# mirror-distfiles	- Mirror the distfile(s) if they are freely redistributable
#				  Setting MIRROR_DISTFILE to "no" in the package Makefile
#				  will override the default "yes", and the distfile will
#				  not be fetched.
#
# Default sequence for "all" is:  fetch checksum extract patch configure build
#
# Please read the comments in the targets section below, you
# should be able to use the pre-* or post-* targets/scripts
# (which are available for every stage except checksum) or
# override the do-* targets to do pretty much anything you want.
#
# NEVER override the "regular" targets unless you want to open
# a major can of worms.

# Get the architecture
ARCH!=	uname -m

# Get the operating system type and version
OPSYS!=	uname -s
OPSYS_VER!=	uname -r

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

# Define SUPPORT_SHARES for those machines that support shared libraries.
#
.if (${MACHINE_ARCH} == "alpha") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE_ARCH} == "vax") || (${MACHINE_ARCH} == "hppa")
NO_SHARED_LIBS=	yes
.endif

NOCLEANDEPENDS=	yes
NOMANCOMPRESS?=	yes
DEF_UMASK?=		022

.if exists(${.CURDIR}/Makefile.${ARCH}-${OPSYS})
.include "${.CURDIR}/Makefile.${ARCH}-${OPSYS}"
.elif exists(${.CURDIR}/Makefile.${OPSYS})
.include "${.CURDIR}/Makefile.${OPSYS}"
.elif exists(${.CURDIR}/Makefile.${ARCH})
.include "${.CURDIR}/Makefile.${ARCH}"
.endif

# These need to be absolute since we don't know how deep in the ports
# tree we are and thus can't go relative.  They can, of course, be overridden
# by individual Makefiles or local system make configuration.
PORTSDIR?=		/usr/ports
LOCALBASE?=		${DESTDIR}/usr/local
X11BASE?=		${DESTDIR}/usr/X11R6
DISTDIR?=		${PORTSDIR}/distfiles
_DISTDIR?=		${DISTDIR}/${DIST_SUBDIR}
PACKAGES?=		${PORTSDIR}/packages
TEMPLATES?=		${PORTSDIR}/templates

.if exists(${.CURDIR}/patches.${ARCH}-${OPSYS})
PATCHDIR?=		${.CURDIR}/patches.${ARCH}-${OPSYS}
.elif exists(${.CURDIR}/patches.${OPSYS})
PATCHDIR?=		${.CURDIR}/patches.${OPSYS}
.elif exists(${.CURDIR}/patches.${ARCH})
PATCHDIR?=		${.CURDIR}/patches.${ARCH}
.else
PATCHDIR?=		${.CURDIR}/patches
.endif

PATCH_LIST?=    patch-*

.if exists(${.CURDIR}/scripts.${ARCH}-${OPSYS})
SCRIPTDIR?=		${.CURDIR}/scripts.${ARCH}-${OPSYS}
.elif exists(${.CURDIR}/scripts.${OPSYS})
SCRIPTDIR?=		${.CURDIR}/scripts.${OPSYS}
.elif exists(${.CURDIR}/scripts.${ARCH})
SCRIPTDIR?=		${.CURDIR}/scripts.${ARCH}
.else
SCRIPTDIR?=		${.CURDIR}/scripts
.endif

.if exists(${.CURDIR}/files.${ARCH}-${OPSYS})
FILESDIR?=		${.CURDIR}/files.${ARCH}-${OPSYS}
.elif exists(${.CURDIR}/files.${OPSYS})
FILESDIR?=		${.CURDIR}/files.${OPSYS}
.elif exists(${.CURDIR}/files.${ARCH})
FILESDIR?=		${.CURDIR}/files.${ARCH}
.else
FILESDIR?=		${.CURDIR}/files
.endif

.if exists(${.CURDIR}/pkg.${ARCH}-${OPSYS})
PKGDIR?=		${.CURDIR}/pkg.${ARCH}-${OPSYS}
.elif exists(${.CURDIR}/pkg.${OPSYS})
PKGDIR?=		${.CURDIR}/pkg.${OPSYS}
.elif exists(${.CURDIR}/pkg.${ARCH})
PKGDIR?=		${.CURDIR}/pkg.${ARCH}
.else
PKGDIR?=		${.CURDIR}/pkg
.endif

.if defined(USE_IMAKE) || defined(USE_X11)
PREFIX?=		${X11BASE}
.else
PREFIX?=		${LOCALBASE}
.endif
# The following 4 lines should go away as soon as the ports are all updated
.if defined(EXEC_DEPENDS)
BUILD_DEPENDS+=	${EXEC_DEPENDS}
RUN_DEPENDS+=	${EXEC_DEPENDS}
.endif
.if defined(USE_GMAKE)
BUILD_DEPENDS+=		${GMAKE}:${PORTSDIR}/devel/gmake
MAKE_PROGRAM=		${GMAKE}
.else
MAKE_PROGRAM=		${MAKE}
.endif
.if defined(USE_AUTOCONF)
GNU_CONFIGURE= yes
BUILD_DEPENDS+=		${AUTOCONF}:${PORTSDIR}/devel/autoconf
AUTOCONF_DIR?=${WRKSRC}
# missing ?= not an oversight
AUTOCONF_ENV=PATH=${PORTPATH}
.endif
.if defined(USE_EGCC)
BUILD_DEPENDS+= 	${EGCC}:${PORTSDIR}/devel/egcs-stable
CC=${EGCC}
.endif
.if defined(USE_EGXX)
BUILD_DEPENDS+= 	${EGXX}:${PORTSDIR}/devel/egcs-stable
CXX=${EGXX}
.endif
.if defined(USE_MOTIF) && !defined(HAVE_MOTIF) && !defined(REQUIRES_MOTIF)
LIB_DEPENDS+=		Xm.:${PORTSDIR}/x11/lesstif
.endif

.if exists(${PORTSDIR}/../Makefile.inc)
.include "${PORTSDIR}/../Makefile.inc"
.endif

EXTRACT_COOKIE?=	${WRKDIR}/.extract_done
PATCH_COOKIE?=		${WRKDIR}/.patch_done
.if defined(SEPARATE_BUILD)
CONFIGURE_COOKIE?=	${WRKBUILD}/.configure_done
INSTALL_PRE_COOKIE?=${WRKBUILD}/.install_started
INSTALL_COOKIE?=	${WRKBUILD}/.install_done
BUILD_COOKIE?=		${WRKBUILD}/.build_done
PACKAGE_COOKIE?=	${WRKBUILD}/.package_done
.else
CONFIGURE_COOKIE?=	${WRKDIR}/.configure_done
INSTALL_PRE_COOKIE?=${WRKDIR}/.install_started
INSTALL_COOKIE?=	${WRKDIR}/.install_done
BUILD_COOKIE?=		${WRKDIR}/.build_done
PACKAGE_COOKIE?=	${WRKDIR}/.package_done
.endif

# Miscellaneous overridable commands:
GMAKE?=			gmake
AUTOCONF?=		autoconf
EGCC?=			egcc
EGXX?=			eg++
XMKMF?=			xmkmf -a

# be paranoid about which ciphers we trust
.if exists(/sbin/md5)
MD5?=			/sbin/md5
.elif exists(/bin/md5)
MD5?=			/bin/md5
.elif exists(/usr/bin/md5)
MD5?=			/usr/bin/md5
.else
MD5?=			md5
.endif

.if exists(/sbin/sha1)
SHA1?=			/sbin/sha1
.elif exists(/bin/sha1)
SHA1?=			/bin/sha1
.elif exists(/usr/bin/sha1)
SHA1?=			/usr/bin/sha1
.else
SHA1?=			sha1
.endif

.if exists(/sbin/rmd160)
RMD160?=		/sbin/rmd160
.elif exists(/bin/rmd160)
RMD160?=		/bin/rmd160
.elif exists(/usr/bin/rmd160)
RMD160?=		/usr/bin/rmd160
.else
RMD160?=		rmd160
.endif

# Compatibility game
MD5_FILE?=		${FILESDIR}/md5
CHECKSUM_FILE?=	${MD5_FILE}

# Don't touch !!! Used for generating checksums.
CIPHERS=		${SHA1}.SHA1 ${RMD160}.RMD160 ${MD5}.MD5 

# This is the one you can override
PREFERRED_CIPHERS?= ${CIPHERS}

PORTPATH?= /usr/bin:/bin:/usr/sbin:/sbin:${LOCALBASE}/bin:${X11BASE}/bin

MAKE_FLAGS?=	-f
MAKEFILE?=		Makefile
MAKE_ENV+=		PATH=${PORTPATH} PREFIX=${PREFIX} LOCALBASE=${LOCALBASE} X11BASE=${X11BASE} MOTIFLIB="${MOTIFLIB}" CFLAGS="${CFLAGS}"

.if exists(/usr/bin/fetch)
FETCH_CMD?=		/usr/bin/fetch
.else
FETCH_CMD?=		/usr/bin/ftp
.endif

# By default, distfiles have no restrictions placed on them
MIRROR_DISTFILE?=	yes

TOUCH?=			/usr/bin/touch
TOUCH_FLAGS?=	-f

PATCH?=			/usr/bin/patch
PATCH_STRIP?=	-p0
PATCH_DIST_STRIP?=	-p0
.if defined(PATCH_DEBUG)
PATCH_DEBUG_TMP=	yes
PATCH_ARGS?=	-d ${WRKSRC} -E ${PATCH_STRIP}
PATCH_DIST_ARGS?=	-d ${WRKSRC} -E ${PATCH_DIST_STRIP}
.else
PATCH_DEBUG_TMP=	no
PATCH_ARGS?=	-d ${WRKSRC} --forward --quiet -E ${PATCH_STRIP}
PATCH_DIST_ARGS?=	-d ${WRKSRC} --forward --quiet -E ${PATCH_DIST_STRIP}
.endif
.if defined(BATCH)
PATCH_ARGS+=		--batch
PATCH_DIST_ARGS+=	--batch
.endif

.if defined(PATCH_CHECK_ONLY)
PATCH_ARGS+=	-C
PATCH_DIST_ARGS+=	-C
.endif

.if exists(/bin/tar)
EXTRACT_CMD?=	/bin/tar
.else
EXTRACT_CMD?=	/usr/bin/tar
.endif
# Backwards compatability.
.if defined(EXTRACT_ARGS)
EXTRACT_BEFORE_ARGS?=   ${EXTRACT_ARGS}
.else
EXTRACT_BEFORE_ARGS?=   -xzf
.endif
EXTRACT_SUFX?=	.tar.gz

# Figure out where the local mtree file is
.if !defined(MTREE_FILE)
.if defined(USE_IMAKE) || defined(USE_X11)
MTREE_FILE=	/etc/mtree/BSD.x11.dist
.else
MTREE_FILE=	/etc/mtree/BSD.local.dist
.endif
.endif
MTREE_CMD?=	/usr/sbin/mtree
MTREE_ARGS?=	-U -f ${MTREE_FILE} -d -e -q -p

.include <bsd.own.mk>
MAKE_ENV+=	EXTRA_SYS_MK_INCLUDES="<bsd.own.mk>"

.if !defined(NO_WRKDIR)
.if defined(OBJMACHINE)
WRKDIR?=		${.CURDIR}/work.${MACHINE_ARCH}
.else
WRKDIR?=		${.CURDIR}/work
.endif
.else
WRKDIR?=		${.CURDIR}
.endif
.if defined(NO_WRKSUBDIR)
WRKSRC?=		${WRKDIR}
.else
WRKSRC?=		${WRKDIR}/${DISTNAME}
.endif

.if defined(SEPARATE_BUILD)
WRKBUILD?=		${WRKDIR}/build-${ARCH}
.else
WRKBUILD?=		${WRKSRC}
.endif

.if defined(WRKOBJDIR)
__canonical_PORTSDIR!=	cd ${PORTSDIR}; pwd -P
__canonical_CURDIR!=	cd ${.CURDIR}; pwd -P
PORTSUBDIR=		${__canonical_CURDIR:S,${__canonical_PORTSDIR}/,,}
.endif

# A few aliases for *-install targets
INSTALL_PROGRAM= \
	${INSTALL} ${INSTALL_COPY} ${INSTALL_STRIP} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}
INSTALL_SCRIPT= \
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}
INSTALL_DATA= \
	${INSTALL} ${INSTALL_COPY} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE}
INSTALL_MAN= \
	${INSTALL} ${INSTALL_COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE}
INSTALL_PROGRAM_DIR= \
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m ${BINMODE}
INSTALL_SCRIPT_DIR= \
	${INSTALL_PROGRAM_DIR}
INSTALL_DATA_DIR= \
	${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m ${BINMODE}
INSTALL_MAN_DIR= \
	${INSTALL} -d -o ${MANOWN} -g ${MANGRP} -m ${BINMODE}

INSTALL_MACROS=	BSD_INSTALL_PROGRAM="${INSTALL_PROGRAM}" \
			BSD_INSTALL_SCRIPT="${INSTALL_SCRIPT}" \
			BSD_INSTALL_DATA="${INSTALL_DATA}" \
			BSD_INSTALL_MAN="${INSTALL_MAN}" \
			BSD_INSTALL_PROGRAM_DIR="${INSTALL_PROGRAM_DIR}" \
			BSD_INSTALL_SCRIPT_DIR="${INSTALL_SCRIPT_DIR}" \
			BSD_INSTALL_DATA_DIR="${INSTALL_DATA_DIR}" \
			BSD_INSTALL_MAN_DIR="${INSTALL_MAN_DIR}"
MAKE_ENV+=	${INSTALL_MACROS}
SCRIPTS_ENV+=	${INSTALL_MACROS}

# The user can override the NO_PACKAGE by specifying this from
# the make command line
.if defined(FORCE_PACKAGE)
.undef NO_PACKAGE
.endif

# Support architecture dependent packing lists
#
COMMENT?=	${PKGDIR}/COMMENT
DESCR?=		${PKGDIR}/DESCR
.if exists(${PKGDIR}/PLIST.${ARCH})
PLIST?=		${PKGDIR}/PLIST.${ARCH}
.else
.if defined(NO_SHARED_LIBS) && exists(${PKGDIR}/PLIST.noshared)
PLIST?=		${PKGDIR}/PLIST.noshared
.else
PLIST?=		${PKGDIR}/PLIST
.endif
.endif

PKG_CMD?=		/usr/sbin/pkg_create
PKG_DELETE?=	/usr/sbin/pkg_delete
.if !defined(PKG_ARGS)
PKG_ARGS=		-v -c ${COMMENT} -d ${DESCR} -f ${PLIST} -p ${PREFIX} -P "`${MAKE} package-depends|sort -u`"
.if exists(${PKGDIR}/INSTALL)
PKG_ARGS+=		-i ${PKGDIR}/INSTALL
.endif
.if exists(${PKGDIR}/DEINSTALL)
PKG_ARGS+=		-k ${PKGDIR}/DEINSTALL
.endif
.if exists(${PKGDIR}/REQ)
PKG_ARGS+=		-r ${PKGDIR}/REQ
.endif
.if exists(${PKGDIR}/MESSAGE)
PKG_ARGS+=		-D ${PKGDIR}/MESSAGE
.endif
.if !defined(NO_MTREE)
PKG_ARGS+=		-m ${MTREE_FILE}
.endif
.endif
PKG_SUFX?=		.tgz
# where pkg_add records its dirty deeds.
PKG_DBDIR?=		/var/db/pkg

# shared/dynamic motif libs
.if defined(USE_MOTIF) || defined(HAVE_MOTIF)
.if defined(MOTIF_STATIC)
MOTIFLIB?=	${X11BASE}/lib/libXm.a
.else
MOTIFLIB?=	-L${X11BASE}/lib -lXm
.endif
.endif

AWK?=		/usr/bin/awk
BASENAME?=	/usr/bin/basename
CAT?=		/bin/cat
CP?=		/bin/cp
DIRNAME?=	/usr/bin/dirname
ECHO?=		/bin/echo
EXPR?=		/bin/expr
FALSE?=		/usr/bin/false
FILE?=		/usr/bin/file
GREP?=		/usr/bin/grep
GUNZIP_CMD?=	/usr/bin/gunzip -f
GZCAT?=		/usr/bin/gzcat
GZIP?=		-9
GZIP_CMD?=	/usr/bin/gzip -nf ${GZIP}
LDCONFIG?=	[ ! -x /sbin/ldconfig ] || /sbin/ldconfig
LN?=		/bin/ln
M4?=		/usr/bin/m4
MKDIR?=		/bin/mkdir -p
MV?=		/bin/mv
READLINK?=	/usr/bin/readlink
RM?=		/bin/rm
RMDIR?=		/bin/rmdir
SED?=		/usr/bin/sed

# XXX ${SETENV} is needed in front of var=value lists whenever the next
# command is expanded from a variable, as this could be a shell construct
SETENV?=	/usr/bin/env
SH?=		/bin/sh
TR?=		/usr/bin/tr
TRUE?=		/usr/bin/true

# Used to print all the '===>' style prompts - override this to turn them off.
ECHO_MSG?=		${ECHO}

# How to do nothing.  Override if you, for some strange reason, would rather
# do something.
DO_NADA?=		${TRUE}

ALL_TARGET?=		all
INSTALL_TARGET?=	install

.if defined(USE_IMAKE) && !defined(NO_INSTALL_MANPAGES)
INSTALL_TARGET+=	install.man
.endif

# Popular master sites
MASTER_SITE_XCONTRIB+=	\
	ftp://crl.dec.com/pub/X11/contrib/%SUBDIR%/ \
	ftp://ftp.eu.net/X11/contrib/%SUBDIR%/ \
	ftp://ftp.uni-paderborn.de/pub/X11/contrib/%SUBDIR%/ \
	ftp://ftp.x.org/contrib/%SUBDIR%/

MASTER_SITE_GNU+=	\
	ftp://prep.ai.mit.edu/pub/gnu/%SUBDIR%/ \
	ftp://wuarchive.wustl.edu/systems/gnu/%SUBDIR%/

MASTER_SITE_PERL_CPAN+=	\
	ftp://ftp.digital.com/pub/plan/perl/CPAN/modules/by-module/%SUBDIR%/ \
	ftp://ftp.cdrom.com/pub/perl/CPAN/modules/by-module/%SUBDIR%/

MASTER_SITE_TEX_CTAN+=  \
        ftp://ftp.cdrom.com/pub/tex/ctan/%SUBDIR%/  \
        ftp://wuarchive.wustl.edu/packages/TeX/%SUBDIR%/  \
        ftp://ftp.funet.fi/pub/TeX/CTAN/%SUBDIR%/  \
        ftp://ftp.tex.ac.uk/public/ctan/tex-archive/%SUBDIR%/  \
        ftp://ftp.dante.de/tex-archive/%SUBDIR%/

MASTER_SITE_SUNSITE+=	\
	ftp://metalab.unc.edu/pub/Linux/%SUBDIR%/ \
	ftp://ftp.infomagic.com/pub/mirrors/linux/sunsite/%SUBDIR%/ \
	ftp://ftp.funet.fi/pub/mirrors/sunsite.unc.edu/pub/Linux/%SUBDIR%/ \
	ftp://ftp.lip6.fr/pub/linux/sunsite/%SUBDIR%

# Empty declaration to avoid "variable MASTER_SITES recursive" error
MASTER_SITES?=
PATCH_SITES?=

# Substitute subdirectory names
_MASTER_SITES:=	${MASTER_SITES:S/%SUBDIR%/${MASTER_SITE_SUBDIR}/}
PATCH_SITES:=	${PATCH_SITES:S/%SUBDIR%/${PATCH_SITE_SUBDIR}/}
MASTER_SITES:= ${_MASTER_SITES}

# Two backup master sites, First one at ftp.openbsd.org
#
_MASTER_SITE_OPENBSD?=	\
	ftp://ftp.openbsd.org/pub/OpenBSD/distfiles/${DIST_SUBDIR}/ \
	ftp://ftp.openbsd.org/pub/OpenBSD/licensed/${DIST_SUBDIR}/

# set the backup master sites.
#
MASTER_SITE_BACKUP?=	\
	${_MASTER_SITE_OPENBSD} \
	ftp://ftp.freebsd.org/pub/FreeBSD/distfiles/${DIST_SUBDIR}/

# If the user has this set, go to the OpenBSD repository for everything.
#
.if defined(MASTER_SITE_OPENBSD)
MASTER_SITE_OVERRIDE=  ${_MASTER_SITE_OPENBSD}
.endif

# Where to put distfiles that don't have any other master site
# ;;; This is referenced in a few Makefiles -- I'd like to get rid of it
#
MASTER_SITE_LOCAL?= \
	ftp://ftp.netbsd.org/pub/NetBSD/packages/distfiles/LOCAL_PORTS/ \
	ftp://ftp.freebsd.org/pub/FreeBSD/distfiles/LOCAL_PORTS/

# I guess we're in the master distribution business! :)  As we gain mirror
# sites for distfiles, add them to this list.
.if !defined(MASTER_SITE_OVERRIDE)
MASTER_SITES+=	${MASTER_SITE_BACKUP}
PATCH_SITES+=	${MASTER_SITE_BACKUP}
.else
MASTER_SITES:=	${MASTER_SITE_OVERRIDE} ${MASTER_SITES}
PATCH_SITES:=	${MASTER_SITE_OVERRIDE} ${PATCH_SITES}
.endif

# OpenBSD code to handle ports distfiles on a CDROM.  The distfiles
# are located in /cdrom/distfiles/${DIST_SUBDIR}/ (assuming that the
# CDROM is mounted on /cdrom).
#
.if exists(/cdrom/distfiles)
CDROM_SITE:=	/cdrom/distfiles/${DIST_SUBDIR}
.if defined(FETCH_SYMLINK_DISTFILES)
CDROM_COPY:=	${LN}
CDROM_OPT=		-s
.else
CDROM_COPY:=	${CP}
CDROM_OPT=		-f
.endif
.endif

# Derived names so that they're easily overridable.
DISTFILES?=		${DISTNAME}${EXTRACT_SUFX}
PKGNAME?=		${DISTNAME}

ALLFILES?=	${DISTFILES} ${PATCHFILES}

.if defined(IGNOREFILES)
CKSUMFILES!=	\
	for file in ${ALLFILES}; do \
		ignore=0; \
		for tmp in ${IGNOREFILES}; do \
			if [ "$$file" = "$$tmp" ]; then \
				ignore=1; \
			fi; \
		done; \
		if [ "$$ignore" = 0 ]; then \
			echo "$$file"; \
		else \
			echo ""; \
		fi; \
	done
.else
CKSUMFILES=		${ALLFILES}
.endif

# List of all files, with ${DIST_SUBDIR} in front.  Used for checksum.
.if defined(DIST_SUBDIR)
_CKSUMFILES?=	${CKSUMFILES:S/^/${DIST_SUBDIR}\//}
_IGNOREFILES?=	${IGNOREFILES:S/^/${DIST_SUBDIR}\//}
.else
_CKSUMFILES?=	${CKSUMFILES}
_IGNOREFILES?=	${IGNOREFILES}
.endif

# This is what is actually going to be extracted, and is overridable
#  by user.
EXTRACT_ONLY?=	${DISTFILES}

# Documentation
MAINTAINER?=	ports@OpenBSD.ORG

.if !defined(CATEGORIES)
.BEGIN:
	@${ECHO_MSG} "CATEGORIES is mandatory."
	@${FALSE}
.endif

# Note this has to start with a capital letter (or more accurately, it
#  shouldn't match "[a-z]*"), see the target "delete-package-links" below.
PKGREPOSITORYSUBDIR?=	All
PKGREPOSITORY?=		${PACKAGES}/${PKGREPOSITORYSUBDIR}
.if exists(${PACKAGES})
PKGFILE?=		${PKGREPOSITORY}/${PKGNAME}${PKG_SUFX}
.else
PKGFILE?=		${PKGNAME}${PKG_SUFX}
.endif

CONFIGURE_SCRIPT?=	configure
.if defined(SEPARATE_BUILD)
_CONFIGURE_SCRIPT=${WRKSRC}/${CONFIGURE_SCRIPT}
.else
_CONFIGURE_SCRIPT=./${CONFIGURE_SCRIPT}
.endif
CONFIGURE_ENV+=		PATH=${PORTPATH}

.if defined(GNU_CONFIGURE)
CONFIGURE_ARGS+=	--prefix=${PREFIX}
HAS_CONFIGURE=		yes
.endif

.if defined(NO_SHARED_LIBS)
CONFIGURE_SHARED?=	--disable-shared
.else
CONFIGURE_SHARED?=	--enable-shared
.endif

# Passed to most of script invocations
SCRIPTS_ENV+= CURDIR=${.CURDIR} DISTDIR=${DISTDIR} \
          PATH=${PORTPATH} \
		  WRKDIR=${WRKDIR} WRKSRC=${WRKSRC} WRKBUILD=${WRKBUILD} \
		  PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} FILESDIR=${FILESDIR} \
		  PORTSDIR=${PORTSDIR} DEPENDS="${DEPENDS}" \
		  PREFIX=${PREFIX} LOCALBASE=${LOCALBASE} X11BASE=${X11BASE}

.if defined(BATCH)
SCRIPTS_ENV+=	BATCH=yes
.endif

MANPREFIX?=	${PREFIX}
CATPREFIX?=	${PREFIX}

.for sect in 1 2 3 4 5 6 7 8 9
MAN${sect}PREFIX?=	${MANPREFIX}
CAT${sect}PREFIX?=	${CATPREFIX}
.endfor
MANLPREFIX?=	${MANPREFIX}
MANNPREFIX?=	${MANPREFIX}
CATLPREFIX?=	${CATPREFIX}
CATNPREFIX?=	${CATPREFIX}

MANLANG?=	""	# english only by default

.for lang in ${MANLANG}

.for sect in 1 2 3 4 5 6 7 8 9
.if defined(MAN${sect})
_MANPAGES+=	${MAN${sect}:S%^%${MAN${sect}PREFIX}/man/${lang}/man${sect}/%}
.endif
.if defined(CAT${sect})
_CATPAGES+=	${CAT${sect}:S%^%${CAT${sect}PREFIX}/man/${lang}/cat${sect}/%}
.endif
.endfor

.if defined(MANL)
_MANPAGES+=	${MANL:S%^%${MANLPREFIX}/man/${lang}/manl/%}
.endif

.if defined(MANN)
_MANPAGES+=	${MANN:S%^%${MANNPREFIX}/man/${lang}/mann/%}
.endif

.if defined(CATL)
_CATPAGES+=	${CATL:S%^%${CATLPREFIX}/man/${lang}/catl/%}
.endif

.if defined(CATN)
_CATPAGES+=	${CATN:S%^%${CATNPREFIX}/man/${lang}/catn/%}
.endif

.endfor

.MAIN: all

################################################################
# Many ways to disable a port.
#
# If we're in BATCH mode and the port is interactive, or we're
# in interactive mode and the port is non-interactive, skip all
# the important targets.  The reason we have two modes is that
# one might want to leave a build in BATCH mode running
# overnight, then come back in the morning and do _only_ the
# interactive ones that required your intervention.
#
# Don't attempt to build ports that require Motif if you don't
# have Motif.
#
# Ignore ports that can't be resold if building for a CDROM.
#
# Don't build a port if it's restricted and we don't want to get
# into that.
#
# Don't build a port if it's broken.
#
# Don't build a port if it comes with the base system.
################################################################

.if !defined(NO_IGNORE)
.if (defined(IS_INTERACTIVE) && defined(BATCH))
IGNORE=	"is an interactive port"
.elif (!defined(IS_INTERACTIVE) && defined(INTERACTIVE))
IGNORE=	"is not an interactive port"
.elif (defined(REQUIRES_MOTIF) && !defined(HAVE_MOTIF))
IGNORE=	"requires Motif"
.elif (defined(MOTIF_ONLY) && !defined(REQUIRES_MOTIF))
IGNORE=	"does not require Motif"
.elif (defined(NO_CDROM) && defined(FOR_CDROM))
IGNORE=	"may not be placed on a CDROM: ${NO_CDROM}"
.elif (defined(RESTRICTED) && defined(NO_RESTRICTED))
IGNORE=	"is restricted: ${RESTRICTED}"
.elif ((defined(USE_IMAKE) || defined(USE_X11)) && !exists(${X11BASE}))
IGNORE=	"uses X11, but ${X11BASE} not found"
.elif defined(BROKEN)
IGNORE=	"is marked as broken: ${BROKEN}"
.elif defined(ONLY_FOR_ARCHS)
.for __ARCH in ${ONLY_FOR_ARCHS}
.if (${MACHINE_ARCH} == "${__ARCH}") || (${ARCH} == "${__ARCH}")
__ARCH_OK=	1
.endif
.endfor
.if !defined(__ARCH_OK)
.if ${MACHINE_ARCH} == "${ARCH}"
IGNORE= "is only for ${ONLY_FOR_ARCHS}, not ${MACHINE_ARCH}"
.else
IGNORE= "is only for ${ONLY_FOR_ARCHS}, not ${MACHINE_ARCH} \(${ARCH}\)"
.endif
.endif
.elif defined(COMES_WITH)
.if ( ${OPSYS_VER} >= ${COMES_WITH} )
IGNORE= "-- ${PKGNAME:C/-[0-9].*//g} comes with ${OPSYS} as of release ${COMES_WITH}"
.endif
.endif

.if defined(IGNORE)
.if defined(IGNORE_SILENT)
IGNORECMD=	${DO_NADA}
.else
IGNORECMD=	${ECHO_MSG} "===>  ${PKGNAME} ${IGNORE}."
.endif
fetch:
	@${IGNORECMD}
checksum:
	@${IGNORECMD}
extract:
	@${IGNORECMD}
patch:
	@${IGNORECMD}
configure:
	@${IGNORECMD}
all:
	@${IGNORECMD}
build:
	@${IGNORECMD}
install:
	@${IGNORECMD}
uninstall deinstall:
	@${IGNORECMD}
package:
	@${IGNORECMD}
.endif # IGNORE
.endif # !NO_IGNORE

.if defined(ALL_HOOK)
all:
	@cd ${.CURDIR} && ${SETENV} CURDIR=${.CURDIR} DISTNAME=${DISTNAME} \
	  DISTDIR=${DISTDIR} WRKDIR=${WRKDIR} WRKSRC=${WRKSRC} WRKBUILD=${WRKBUILD}\
	  PATCHDIR=${PATCHDIR} SCRIPTDIR=${SCRIPTDIR} \
	  FILESDIR=${FILESDIR} PORTSDIR=${PORTSDIR} PREFIX=${PREFIX} \
	  DEPENDS="${DEPENDS}" BUILD_DEPENDS="${BUILD_DEPENDS}" \
	  RUN_DEPENDS="${RUN_DEPENDS}" X11BASE=${X11BASE} \
	${ALL_HOOK}
.endif

.if !target(all)
all: build
.endif

.if !defined(DEPENDS_TARGET)
.if make(reinstall)
DEPENDS_TARGET=	reinstall
.else
DEPENDS_TARGET=	install
.endif
.endif

################################################################
# The following are used to create easy dummy targets for
# disabling some bit of default target behavior you don't want.
# They still check to see if the target exists, and if so don't
# do anything, since you might want to set this globally for a
# group of ports in a Makefile.inc, but still be able to
# override from an individual Makefile.
################################################################

# Disable checksum
.if defined(NO_CHECKSUM) && !target(checksum)
checksum: fetch
	@${DO_NADA}
.endif

# Disable extract
.if defined(NO_EXTRACT) && !target(extract)
extract: 
	@${TOUCH} ${TOUCH_FLAGS} ${EXTRACT_COOKIE}
checksum: fetch
	@${DO_NADA}
makesum:
	@${DO_NADA}
addsum:
	@${DO_NADA}
.endif

# Disable patch
.if defined(NO_PATCH) && !target(patch)
patch: extract
	@${TOUCH} ${TOUCH_FLAGS} ${PATCH_COOKIE}
.endif

# Disable configure
.if defined(NO_CONFIGURE) && !target(configure)
configure: patch
	@${TOUCH} ${TOUCH_FLAGS} ${CONFIGURE_COOKIE}
.endif

# Disable build
.if defined(NO_BUILD) && !target(build)
build: configure
	@${TOUCH} ${TOUCH_FLAGS} ${BUILD_COOKIE}
.endif

# Disable install
.if defined(NO_INSTALL) && !target(install)
install: build
	@${TOUCH} ${TOUCH_FLAGS} ${INSTALL_COOKIE}
.endif

# Disable package
.if defined(NO_PACKAGE) && !target(package)
package:
.if defined(IGNORE_SILENT)
	@${DO_NADA}
.else
	@${ECHO_MSG} "===>  ${PKGNAME} may not be packaged: ${NO_PACKAGE}."
.endif
.endif

# Disable describe
.if defined(NO_DESCRIBE) && !target(describe)
describe:
	@${DO_NADA}
.endif

################################################################
# More standard targets start here.
#
# These are the body of the build/install framework.  If you are
# not happy with the default actions, and you can't solve it by
# adding pre-* or post-* targets/scripts, override these.
################################################################

# Fetch

.if !target(do-fetch)
do-fetch:
	@${MKDIR} ${_DISTDIR}
	@(cd ${_DISTDIR}; \
	 for file in ${DISTFILES}; do \
		if [ ! -f $$file -a ! -f `${BASENAME} $$file` ]; then \
			if [ -h $$file -o -h `${BASENAME} $$file` ]; then \
				${ECHO_MSG} ">> ${_DISTDIR}/$$file is a broken symlink."; \
				${ECHO_MSG} ">> Perhaps a filesystem (most likely a CD) isn't mounted?"; \
				${ECHO_MSG} ">> Please correct this problem and try again."; \
				exit 1; \
			fi ; \
			if [ ! -z ${CDROM_COPY} ]; then \
				if ${CDROM_COPY} ${CDROM_OPT} ${CDROM_SITE}/$$file .; then \
					continue; \
				fi ; \
			fi ; \
			${ECHO_MSG} ">> $$file doesn't seem to exist on this system."; \
			if [ ! -w ${_DISTDIR}/. ]; then \
				${ECHO_MSG} ">> Can't download to ${_DISTDIR} (permission denied?)."; \
				exit 1; \
			fi; \
			for site in ${MASTER_SITES}; do \
			    ${ECHO_MSG} ">> Attempting to fetch from $${site}."; \
				if ${FETCH_CMD} ${FETCH_BEFORE_ARGS} $${site}$${file} ${FETCH_AFTER_ARGS}; then \
					continue 2; \
				fi \
			done; \
			${ECHO_MSG} ">> Couldn't fetch it - please try to retrieve this";\
			${ECHO_MSG} ">> port manually into ${_DISTDIR} and try again."; \
			exit 1; \
	    fi \
	 done)
.if defined(PATCHFILES)
	@(cd ${_DISTDIR}; \
	 for file in ${PATCHFILES}; do \
		if [ ! -f $$file -a ! -f `${BASENAME} $$file` ]; then \
			if [ -h $$file -o -h `${BASENAME} $$file` ]; then \
				${ECHO_MSG} ">> ${_DISTDIR}/$$file is a broken symlink."; \
				${ECHO_MSG} ">> Perhaps a filesystem (most likely a CD) isn't mounted?"; \
				${ECHO_MSG} ">> Please correct this problem and try again."; \
				exit 1; \
			fi ; \
			${ECHO_MSG} ">> $$file doesn't seem to exist on this system."; \
			for site in ${PATCH_SITES}; do \
			    ${ECHO_MSG} ">> Attempting to fetch from $${site}."; \
				if ${FETCH_CMD} ${FETCH_BEFORE_ARGS} $${site}$${file} ${FETCH_AFTER_ARGS}; then \
					continue 2; \
				fi \
			done; \
			${ECHO_MSG} ">> Couldn't fetch it - please try to retrieve this";\
			${ECHO_MSG} ">> port manually into ${_DISTDIR} and try again."; \
			exit 1; \
	    fi \
	 done)
.endif
.endif

# This is for the use of sites which store distfiles which others may
# fetch - only fetch the distfile if it is allowed to be
# re-distributed freely
mirror-distfiles:
.if (${MIRROR_DISTFILE} == "yes")
	@make fetch __ARCH_OK=yes NO_IGNORE=yes NO_WARNINGS=yes
.endif

# Extract

.if !target(do-extract)
do-extract:
.if !defined(NO_WRKDIR)
.if defined(WRKOBJDIR)
	@${RM} -rf ${WRKOBJDIR}/${PORTSUBDIR}
	@${MKDIR} -p ${WRKOBJDIR}/${PORTSUBDIR}
	@if [ ! -L ${WRKDIR} ] || \
	  [ X`${READLINK} ${WRKDIR}` != X${WRKOBJDIR}/${PORTSUBDIR} ]; then \
		echo "${WRKDIR} -> ${WRKOBJDIR}/${PORTSUBDIR}"; \
		${RM} -f ${WRKDIR}; \
		${LN} -sf ${WRKOBJDIR}/${PORTSUBDIR} ${WRKDIR}; \
	fi
.else
	@${RM} -rf ${WRKDIR}
	@${MKDIR} ${WRKDIR}
.endif
.endif
	@for file in ${EXTRACT_ONLY}; do \
		if ! (cd ${WRKDIR} && ${EXTRACT_CMD} ${EXTRACT_BEFORE_ARGS} ${_DISTDIR}/$$file ${EXTRACT_AFTER_ARGS});\
		then \
			exit 1; \
		fi \
	done
.endif

# Patch

.if !target(do-patch)
do-patch:
.if defined(PATCHFILES)
	@${ECHO_MSG} "===>  Applying distribution patches for ${PKGNAME}"
	@(cd ${_DISTDIR}; \
	  for i in ${PATCHFILES}; do \
		if [ ${PATCH_DEBUG_TMP} = yes ]; then \
			${ECHO_MSG} "===>   Applying distribution patch $$i" ; \
		fi; \
		case $$i in \
			*.Z|*.gz) \
				${GZCAT} $$i | ${PATCH} ${PATCH_DIST_ARGS}; \
				;; \
			*) \
				${PATCH} ${PATCH_DIST_ARGS} < $$i; \
				;; \
		esac; \
	  done)
.endif
	@if [ -d ${PATCHDIR} ]; then \
		(cd ${PATCHDIR}; \
		for i in ${PATCH_LIST}; do \
			case $$i in \
				*.orig|*.rej|*~) \
					${ECHO_MSG} "===>   Ignoring patchfile $$i" ; \
					;; \
				*) \
				    if [ -e $$i ]; then \
						if [ ${PATCH_DEBUG_TMP} = yes ]; then \
							${ECHO_MSG} "===>   Applying ${OPSYS} patch $$i" ; \
						fi; \
						${PATCH} ${PATCH_ARGS} < $$i; \
					else \
						${ECHO_MSG} "===>   Can't find patch matching $$i"; \
						if [ -d ${PATCHDIR}/CVS -a "$$i" = \
							"${PATCHDIR}/patch-*" ]; then \
								${ECHO_MSG} "===>   Perhaps you forgot the -P flag to cvs co or update?"; \
						fi; \
					fi; \
					;; \
			esac; \
		done) \
	fi
.endif

# Configure

.if !target(do-configure)
do-configure: ${WRKBUILD}
.if defined(USE_AUTOCONF)
	@cd ${AUTOCONF_DIR} && ${SETENV} ${AUTOCONF_ENV} ${AUTOCONF}
.endif
	@if [ -f ${SCRIPTDIR}/configure ]; then \
		cd ${.CURDIR} && ${SETENV} ${SCRIPTS_ENV} ${SH} \
		  ${SCRIPTDIR}/configure; \
	fi
.if defined(HAS_CONFIGURE)
	@(cd ${WRKBUILD} && CC="${CC}" ac_cv_path_CC="${CC}" CFLAGS="${CFLAGS}" \
		CXX="${CXX}" ac_cv_path_CXX="${CXX}" CXXFLAGS="${CXXFLAGS}" \
		INSTALL="/usr/bin/install -c -o ${BINOWN} -g ${BINGRP}" \
		INSTALL_PROGRAM="${INSTALL_PROGRAM}" INSTALL_MAN="${INSTALL_MAN}" \
		INSTALL_SCRIPT="${INSTALL_SCRIPT}" INSTALL_DATA="${INSTALL_DATA}" \
		${CONFIGURE_ENV} ${_CONFIGURE_SCRIPT} ${CONFIGURE_ARGS})
.endif
.if defined(USE_IMAKE)
	@(cd ${WRKSRC} && ${SETENV} ${MAKE_ENV} ${XMKMF})
.endif
.endif

${WRKBUILD}:
	${MKDIR} ${WRKBUILD}

# Build

.if !target(do-build)
do-build:
	@(cd ${WRKBUILD}; ${SETENV} ${MAKE_ENV} ${MAKE_PROGRAM} ${MAKE_FLAGS} ${MAKEFILE} ${ALL_TARGET})
.endif

# Install

.if !target(do-install)
do-install:
	@(cd ${WRKBUILD} && ${SETENV} ${MAKE_ENV} ${MAKE_PROGRAM} ${MAKE_FLAGS} ${MAKEFILE} ${INSTALL_TARGET})
.endif

# Package

.if !target(do-package)
do-package:
	@if [ -e ${PLIST} ]; then \
		${ECHO_MSG} "===>  Building package for ${PKGNAME}"; \
		if [ -d ${PACKAGES} ]; then \
			if [ ! -d ${PKGREPOSITORY} ]; then \
				if ! ${MKDIR} ${PKGREPOSITORY}; then \
					${ECHO_MSG} ">> Can't create directory ${PKGREPOSITORY}."; \
					exit 1; \
				fi; \
			fi; \
		fi; \
		if ${PKG_CMD} ${PKG_ARGS} ${PKGFILE}; then \
			if [ -d ${PACKAGES} ]; then \
				${MAKE} ${.MAKEFLAGS} package-links; \
			fi; \
		else \
			${MAKE} ${.MAKEFLAGS} delete-package; \
			exit 1; \
		fi; \
	fi
.endif

# Some support rules for do-package

.if !target(package-links)
package-links:
	@${MAKE} ${.MAKEFLAGS} delete-package-links
	@for cat in ${CATEGORIES}; do \
		if [ ! -d ${PACKAGES}/$$cat ]; then \
			if ! ${MKDIR} ${PACKAGES}/$$cat; then \
				${ECHO_MSG} ">> Can't create directory ${PACKAGES}/$$cat."; \
				exit 1; \
			fi; \
		fi; \
		ln -s ../${PKGREPOSITORYSUBDIR}/${PKGNAME}${PKG_SUFX} ${PACKAGES}/$$cat; \
	done;
.endif

.if !target(delete-package-links)
delete-package-links:
	@${RM} -f ${PACKAGES}/[a-z]*/${PKGNAME}${PKG_SUFX};
.endif

.if !target(delete-package)
delete-package:
	@${MAKE} ${.MAKEFLAGS} delete-package-links
	@${RM} -f ${PKGFILE}
.endif

################################################################
# This is the "generic" port target, actually a macro used from the
# six main targets.  See below for more.
################################################################

_PORT_USE: .USE
.if make(real-fetch)
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} fetch-depends
.endif
.if make(real-extract)
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} build-depends lib-depends misc-depends
.endif
.if make(real-install)
.if !defined(NO_PKG_REGISTER) && !defined(FORCE_PKG_REGISTER)
	@if [ -d ${PKG_DBDIR}/${PKGNAME} -o "X$$(ls -d ${PKG_DBDIR}/${PKGNAME:C/-[0-9].*//g}-* 2> /dev/null)" != "X" ]; then \
		${ECHO_MSG} "===>  ${PKGNAME} is already installed - perhaps an older version?"; \
		${ECHO_MSG} "      If so, you may wish to \`\`make deinstall'' and install"; \
		${ECHO_MSG} "      this port again by \`\`make reinstall'' to upgrade it properly."; \
		${ECHO_MSG} "      If you really wish to overwrite the old port of ${PKGNAME}"; \
		${ECHO_MSG} "      without deleting it first, set the variable \"FORCE_PKG_REGISTER\""; \
		${ECHO_MSG} "      in your environment or the \"make install\" command line."; \
		exit 1; \
	fi
.endif
	@if [ `${SH} -c umask` != ${DEF_UMASK} ]; then \
		${ECHO_MSG} "===>  Warning: your umask is \"`${SH} -c umask`"\".; \
		${ECHO_MSG} "      If this is not desired, set it to an appropriate value"; \
		${ECHO_MSG} "      and install this port again by \`\`make reinstall''."; \
	fi
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} run-depends lib-depends
.endif
.if make(real-install)
	@touch ${INSTALL_PRE_COOKIE}
.if !defined(NO_MTREE)
	@if [ `id -u` = 0 ]; then \
		if [ ! -f ${MTREE_FILE} ]; then \
			${ECHO_MSG} "Error: mtree file \"${MTREE_FILE}\" is missing."; \
			${ECHO_MSG} "Copy it from a suitable location (e.g., /usr/src/etc/mtree) and try again."; \
			exit 1; \
		else \
			if [ ! -d ${PREFIX} ]; then \
				mkdir -p ${PREFIX}; \
			fi; \
			${MTREE_CMD} ${MTREE_ARGS} ${PREFIX}/; \
		fi; \
	else \
		${ECHO_MSG} "Warning: not superuser, can't run mtree."; \
		${ECHO_MSG} "Become root and try again to ensure correct permissions."; \
	fi
.endif
.endif
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} ${.TARGET:S/^real-/pre-/}
	@if [ -f ${SCRIPTDIR}/${.TARGET:S/^real-/pre-/} ]; then \
		cd ${.CURDIR} && ${SETENV} ${SCRIPTS_ENV} ${SH} \
			${SCRIPTDIR}/${.TARGET:S/^real-/pre-/}; \
	fi
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} ${.TARGET:S/^real-/do-/}
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} ${.TARGET:S/^real-/post-/}
	@if [ -f ${SCRIPTDIR}/${.TARGET:S/^real-/post-/} ]; then \
		cd ${.CURDIR} && ${SETENV} ${SCRIPTS_ENV} ${SH} \
			${SCRIPTDIR}/${.TARGET:S/^real-/post-/}; \
	fi
.if make(real-install) && (defined(_MANPAGES) || defined(_CATPAGES))
.if defined(MANCOMPRESSED) && defined(NOMANCOMPRESS)
	@${ECHO_MSG} "===>   Uncompressing manual pages for ${PKGNAME}"
.for manpage in ${_MANPAGES} ${_CATPAGES}
	@${GUNZIP_CMD} ${manpage}.gz
.endfor
.elif !defined(MANCOMPRESSED) && !defined(NOMANCOMPRESS)
	@${ECHO_MSG} "===>   Compressing manual pages for ${PKGNAME}"
.for manpage in ${_MANPAGES} ${_CATPAGES}
	@if [ -L ${manpage} ]; then \
		set - `${FILE} ${manpage}`; \
		shift `${EXPR} $$# - 1`; \
		${LN} -sf $${1}.gz ${manpage}.gz; \
		${RM} ${manpage}; \
	else \
		${GZIP_CMD} ${manpage}; \
	fi
.endfor
.endif
.endif
.if make(real-install) && exists(${PKGDIR}/MESSAGE)
	@${CAT}	${PKGDIR}/MESSAGE
.endif
.if make(real-install) && !defined(NO_PKG_REGISTER)
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} fake-pkg
.endif
.if make(real-extract)
	@${TOUCH} ${TOUCH_FLAGS} ${EXTRACT_COOKIE}
.endif
.if make(real-patch) && !defined(PATCH_CHECK_ONLY)
	@${TOUCH} ${TOUCH_FLAGS} ${PATCH_COOKIE}
.endif
.if make(real-configure)
	@${TOUCH} ${TOUCH_FLAGS} ${CONFIGURE_COOKIE}
.endif
.if make(real-install)
	@${TOUCH} ${TOUCH_FLAGS} ${INSTALL_COOKIE}
.endif
.if make(real-build)
	@${TOUCH} ${TOUCH_FLAGS} ${BUILD_COOKIE}
.endif
.if make(real-package) && !defined(PACKAGE_NOINSTALL)
	@${TOUCH} ${TOUCH_FLAGS} ${PACKAGE_COOKIE}
.endif

################################################################
# Skeleton targets start here
# 
# You shouldn't have to change these.  Either add the pre-* or
# post-* targets/scripts or redefine the do-* targets.  These
# targets don't do anything other than checking for cookies and
# call the necessary targets/scripts.
################################################################

.if !target(fetch)
fetch:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-fetch
.endif

.if !target(extract)
extract: ${EXTRACT_COOKIE}
.endif

.if !target(patch)
patch: extract ${PATCH_COOKIE}
.endif

.if !target(configure)
configure: patch ${CONFIGURE_COOKIE}
.endif

.if !target(build)
build: configure ${BUILD_COOKIE}
.endif

.if !target(install)
install: build ${INSTALL_COOKIE}
.endif

.if !target(package)
package: install ${PACKAGE_COOKIE}
.endif

${EXTRACT_COOKIE}: 
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} checksum real-extract
${PATCH_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-patch
${CONFIGURE_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-configure
${BUILD_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-build
${INSTALL_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-install
${PACKAGE_COOKIE}:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} real-package

# And call the macros

real-fetch: _PORT_USE
real-extract: _PORT_USE
	@${ECHO_MSG} "===>  Extracting for ${PKGNAME}"
real-patch: _PORT_USE
	@${ECHO_MSG} "===>  Patching for ${PKGNAME}"
real-configure: _PORT_USE
	@${ECHO_MSG} "===>  Configuring for ${PKGNAME}"
real-build: _PORT_USE
	@${ECHO_MSG} "===>  Building for ${PKGNAME}"
real-install: _PORT_USE
	@${ECHO_MSG} "===>  Installing for ${PKGNAME}"
real-package: _PORT_USE

# Empty pre-* and post-* targets, note we can't use .if !target()
# in the _PORT_USE macro

.for name in fetch extract patch configure build install package

.if !target(pre-${name})
pre-${name}:
	@${DO_NADA}
.endif

.if !target(post-${name})
post-${name}:
	@${DO_NADA}
.endif

.endfor

# Checkpatch
#
# Special target to verify patches

.if !target(checkpatch)
checkpatch:
	@cd ${.CURDIR} && ${MAKE} PATCH_CHECK_ONLY=yes ${.MAKEFLAGS} patch
.endif

# Reinstall
#
# Special target to re-run install

.if !target(reinstall)
reinstall:
	@${RM} -f ${INSTALL_PRE_COOKIE} ${INSTALL_COOKIE} ${PACKAGE_COOKIE}
	@DEPENDS_TARGET=${DEPENDS_TARGET} ${MAKE} install
.endif

# Deinstall
#
# Special target to remove installation

.if !target(deinstall)
uninstall deinstall:
	@${ECHO_MSG} "===> Deinstalling for ${PKGNAME}"
	@${PKG_DELETE} -f ${PKGNAME}
	@${RM} -f ${INSTALL_COOKIE} ${PACKAGE_COOKIE}
.endif


################################################################
# Some more targets supplied for users' convenience
################################################################

# Cleaning up

.if !target(pre-clean)
pre-clean:
	@${DO_NADA}
.endif

.if !target(clean)
clean: pre-clean
.if !defined(NOCLEANDEPENDS)
	@${MAKE} clean-depends
.endif
	@${ECHO_MSG} "===>  Cleaning for ${PKGNAME}"
.if !defined(NO_WRKDIR)
.if  defined(WRKOBJDIR)
	@${RM} -rf ${WRKOBJDIR}/${PORTSUBDIR}
	@${RM} -f ${WRKDIR}
.else
	@if [ -d ${WRKDIR} ]; then \
		if [ -w ${WRKDIR} ]; then \
			${RM} -rf ${WRKDIR}; \
		else \
			${ECHO_MSG} "===>   ${WRKDIR} not writable, skipping"; \
		fi; \
	fi
.endif
.else
	@${RM} -f ${WRKDIR}/.*_done
.endif
.endif

.if !target(pre-distclean)
pre-distclean:
	@${DO_NADA}
.endif

.if !target(distclean)
distclean: pre-distclean clean
	@${ECHO_MSG} "===>  Dist cleaning for ${PKGNAME}"
	@(if [ -d ${_DISTDIR} ]; then \
		cd ${_DISTDIR}; \
		${RM} -f ${DISTFILES} ${PATCHFILES}; \
	fi)
.if defined(DIST_SUBDIR)
	-@${RMDIR} ${_DISTDIR}  
.endif
.endif

# Prints out a list of files to fetch (useful to do a batch fetch)

# are we called from bsd.port.subdir.mk (i.e. do we scan all dirs anyways)? XXX
.ifdef(DIRPRFX)
RECURSIVE_FETCH_LIST?=	NO
.else
RECURSIVE_FETCH_LIST?=	YES
.endif

.if !target(fetch-list)
fetch-list:
	@${MAKE} fetch-list-recursive RECURSIVE_FETCH_LIST=${RECURSIVE_FETCH_LIST} | sort -u
.endif # !target(fetch-list)

.if !target(fetch-list-recursive)
fetch-list-recursive:
	@${MAKE} fetch-list-one-pkg
.if ${RECURSIVE_FETCH_LIST} != "NO"
	@for dir in `${ECHO} ${FETCH_DEPENDS} ${BUILD_DEPENDS} ${LIB_DEPENDS}  ${RUN_DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/^[^:]*://' -e 's/:.*//' | sort -u` `${ECHO} ${DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/:.*//' | sort -u`; do \
		(cd $$dir; ${MAKE} fetch-list-recursive; ); \
	done
.endif # ${RECURSIVE_FETCH_LIST} != "NO"
.endif # !target(fetch-list-recursive)

.if !target(fetch-list-one-pkg)
fetch-list-one-pkg:
	@${MKDIR} ${_DISTDIR}
	@[ -z "${_DISTDIR}" ] || ${ECHO} "${MKDIR} ${_DISTDIR}"
	@(cd ${_DISTDIR}; \
	 for file in ${DISTFILES}; do \
		if [ ! -f $$file -a ! -f `${BASENAME} $$file` ]; then \
			${ECHO} -n "cd ${_DISTDIR} && [ -f $$file -o -f `${BASENAME} $$file` ] || " ; \
			for site in ${MASTER_SITES} ; do \
				${ECHO} -n ${FETCH_CMD} ${FETCH_BEFORE_ARGS} $${site}$${file} "${FETCH_AFTER_ARGS}" '|| ' ; \
			done; \
			${ECHO} "echo $${file} not fetched" ; \
		fi \
	done)
.if defined(PATCHFILES)
	@(cd ${_DISTDIR}; \
	 for file in ${PATCHFILES}; do \
		if [ ! -f $$file -a ! -f `${BASENAME} $$file` ]; then \
			${ECHO} -n "cd ${_DISTDIR} && [ -f $$file -o -f `${BASENAME} $$file` ] || " ; \
			for site in ${PATCH_SITES}; do \
				${ECHO} -n ${FETCH_CMD} ${FETCH_BEFORE_ARGS} $${site}$${file} "${FETCH_AFTER_ARGS}" '|| ' ; \
			done; \
			${ECHO} "echo $${file} not fetched" ; \
		fi \
	done)
.endif # defined(PATCHFILES)
.endif # !target(fetch-list-one-pkg)

# Checksumming utilities

.if !target(makesum)
makesum: fetch
	@${MKDIR} ${FILESDIR}
	@if [ -f ${CHECKSUM_FILE} ]; then ${RM} -f ${CHECKSUM_FILE}; fi
	@(cd ${DISTDIR}; \
	 for file in ${_CKSUMFILES}; do \
	 	for cipher in ${CIPHERS:R}; do \
			$$cipher $$file >> ${CHECKSUM_FILE}; \
		done; \
	 done)
	@for file in ${_IGNOREFILES}; do \
		${ECHO} "MD5 ($$file) = IGNORE" >> ${CHECKSUM_FILE}; \
	done
.endif

.if !target(addsum)
addsum: fetch
	@${MKDIR} ${FILESDIR}
	@touch ${CHECKSUM_FILE}
	@(cd ${DISTDIR}; \
	 for file in ${_CKSUMFILES}; do \
	 	for cipher in ${CIPHERS:R}; do \
			$$cipher $$file >> ${CHECKSUM_FILE}; \
		done; \
	 done)
	@for file in ${_IGNOREFILES}; do \
		${ECHO} "MD5 ($$file) = IGNORE" >> ${CHECKSUM_FILE}; \
	done
	@sort -u ${CHECKSUM_FILE} >${CHECKSUM_FILE}.new
	@${MV} -f ${CHECKSUM_FILE}.new ${CHECKSUM_FILE}
	@if [ `${SED} -e 's/\=.*$$//' ${CHECKSUM_FILE} | uniq -d | wc -l` -ne 0 ]; then \
		${ECHO} "Inconsistent checksum in ${CHECKSUM_FILE}"; \
	else \
		${ECHO} "${CHECKSUM_FILE} updated okay, don't forget to remove cruft"; \
	fi
.endif

.if !target(checksum)
checksum: fetch
	@if [ ! -f ${CHECKSUM_FILE} ]; then \
		${ECHO_MSG} ">> No checksum file."; \
	else \
		(cd ${DISTDIR}; OK="true"; \
		  for file in ${_CKSUMFILES}; do \
			for cipher_sig in ${PREFERRED_CIPHERS}; do \
				sig=`${EXPR} $$cipher_sig : '.*\.\(.*\)'`; \
				CKSUM2=`${GREP} "^$$sig ($$file)" ${CHECKSUM_FILE} | ${AWK} '{print $$4}'`; \
				if [ "$$CKSUM2" = "" ]; then \
					${ECHO_MSG} ">> No $$sig checksum recorded for $$file."; \
				else \
					cipher=`${EXPR} $$cipher_sig : '\(.*\)\.'`; \
					break; \
				fi; \
			done; \
			if [ "$$CKSUM2" = "" ]; then \
				${ECHO_MSG} ">> No checksum recorded for $$file."; \
				OK="false"; \
			elif [ "$$CKSUM2" = "IGNORE" ]; then \
				${ECHO_MSG} ">> Checksum for $$file is set to IGNORE in md5 file even though"; \
				${ECHO_MSG} "   the file is not in the "'$$'"{IGNOREFILES} list."; \
				OK="false"; \
			else \
				CKSUM=`$$cipher < $$file`; \
				if [ "$$CKSUM" = "$$CKSUM2" ]; then \
					${ECHO_MSG} ">> Checksum OK for $$file. ($$sig)"; \
				else \
					${ECHO_MSG} ">> Checksum mismatch for $$file. ($$sig)"; \
					OK="false"; \
				fi; \
			fi; \
		  done; \
		  for file in ${_IGNOREFILES}; do \
			CKSUM2=`${GREP} "($$file)" ${CHECKSUM_FILE} | ${AWK} '{print $$4}'`; \
			if [ "$$CKSUM2" = "" ]; then \
				${ECHO_MSG} ">> No checksum recorded for $$file, file is in "'$$'"{IGNOREFILES} list."; \
				OK="false"; \
			elif [ "$$CKSUM2" != "IGNORE" ]; then \
				${ECHO_MSG} ">> Checksum for $$file is not set to IGNORE in md5 file even though"; \
				${ECHO_MSG} "   the file is in the "'$$'"{IGNOREFILES} list."; \
				OK="false"; \
			fi; \
		  done; \
		  if [ "$$OK" != "true" ]; then \
			${ECHO_MSG} "Make sure the Makefile and checksum file (${CHECKSUM_FILE})"; \
			${ECHO_MSG} "are up to date.  If you want to override this check, type"; \
			${ECHO_MSG} "\"make NO_CHECKSUM=yes [other args]\"."; \
			exit 1; \
		  fi) ; \
	fi
.endif

# packing list utilities.  This generates a packing list from a recently
# installed port.  Not perfect, but pretty close.  The generated file
# will have to have some tweaks done by hand.
# Note: add @comment PACKAGE(arch=${ARCH}, opsys=${OPSYS}, vers=${OPSYS_VER})
# when port is installed or package created.
#
.if !target(plist)
plist: install
	@${MKDIR} ${PKGDIR}
	@(dirs=""; \
	  ld=""; \
	  for f in `${MAKE} package-depends|sort -u`; do ${ECHO} "@pkgdep $$f"; done; \
	  for f in `find ${PREFIX} -newer ${INSTALL_PRE_COOKIE} -print 2> /dev/null`; do \
	   ff=`${ECHO} $$f | ${SED} -e 's|^${PREFIX}/||'`; \
	   if [ -d $$f -a ! -h $$f ]; then dirs="$$ff $$dirs"; \
	   else \
	    ${ECHO} $$ff; \
	    if ${ECHO} $$f | ${GREP} -E -q -e '[^/]+\.so\.[0-9]+\.[0-9]+$$'; then \
	     ld="$$LDCONFIG `${DIRNAME} $$f`"; \
	    fi; \
	   fi; \
	  done; \
	  for f in $$dirs; do \
       if ${GREP} -q -e `${BASENAME} $$f` ${MTREE_FILE}; then \
        :; \
       else \
        ${ECHO} "@dirrm $$f"; \
       fi; \
      done; \
	  for f in $$ld; do ${ECHO} "@exec ${LDCONFIG} -m $$f"; done; \
	) > ${PLIST}-auto
.endif

################################################################
# The special package-building targets
# You probably won't need to touch these
################################################################

HTMLIFY=	${SED} -e 's/&/\&amp;/g' -e 's/>/\&gt;/g' -e 's/</\&lt;/g'

# Set to YES by the README.html target (and passed via depends-list
# and package-depends)
.ifndef PACKAGE_NAME_AS_LINK
PACKAGE_NAME_AS_LINK=NO
.endif # PACKAGE_NAME_AS_LINK


# Nobody should want to override this unless PKGNAME is simply bogus.

.if !target(package-name)
package-name:
.if (${PACKAGE_NAME_AS_LINK} == "YES")
	@${ECHO} '<A HREF="../../'`${MAKE} package-path | ${HTMLIFY}`'/README.html">'`echo ${PKGNAME} | ${HTMLIFY}`'</A>'
.else
	@${ECHO} '${PKGNAME}'
.endif # PACKAGE_NAME_AS_LINK != ""
.endif # !target(package-name)

.if !target(package-path)
package-path:
	@pwd | sed s@`cd ${PORTSDIR} ; pwd`/@@g
.endif

# Show (recursively) all the packages this package depends on.

.if !target(package-depends)
package-depends:
	@for dir in `${ECHO} ${LIB_DEPENDS} ${RUN_DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/^[^:]*://' -e 's/:.*//' | sort -u` `${ECHO} ${DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/:.*//' | sort -u`; do \
		if [ -d $$dir ]; then \
			(cd $$dir ; ${MAKE} package-name package-depends PACKAGE_NAME_AS_LINK=${PACKAGE_NAME_AS_LINK}); \
		else \
			${ECHO_MSG} "Warning: \"$$dir\" non-existent -- @pkgdep registration incomplete" >&2; \
		fi; \
	done
.endif

# Build a package but don't check the package cookie

.if !target(repackage)
repackage: pre-repackage package

pre-repackage:
	@${RM} -f ${PACKAGE_COOKIE}
.endif

# Build a package but don't check the cookie for installation, also don't
# install package cookie

.if !target(package-noinstall)
package-noinstall:
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} PACKAGE_NOINSTALL=yes real-package
.endif

################################################################
# Dependency checking
################################################################

.if !target(depends)
depends: lib-depends misc-depends
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} fetch-depends
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} build-depends
	@cd ${.CURDIR} && ${MAKE} ${.MAKEFLAGS} run-depends

.if make(fetch-depends)
DEPENDS_TMP+=	${FETCH_DEPENDS}
.endif

.if make(build-depends)
DEPENDS_TMP+=	${BUILD_DEPENDS}
.endif

.if make(run-depends)
DEPENDS_TMP+=	${RUN_DEPENDS}
.endif

_DEPENDS_USE:	.USE
.if defined(DEPENDS_TMP)
.if !defined(NO_DEPENDS)
	@PATH=${PORTPATH}; \
	for i in ${DEPENDS_TMP}; do \
		prog=`${ECHO} $$i | ${SED} -e 's/:.*//'`; \
		dir=`${ECHO} $$i | ${SED} -e 's/[^:]*://'`; \
		if ${EXPR} "$$dir" : '.*:' > /dev/null; then \
			target=`${ECHO} $$dir | ${SED} -e 's/.*://'`; \
			dir=`${ECHO} $$dir | ${SED} -e 's/:.*//'`; \
		else \
			target=${DEPENDS_TARGET}; \
		fi; \
		found=not; \
		if ${EXPR} "$$prog" : \\/ >/dev/null; then \
			if [ -e "$$prog" ]; then \
				${ECHO_MSG} "===>  ${PKGNAME} depends on file: $$prog - found"; \
				found=""; \
			else \
				${ECHO_MSG} "===>  ${PKGNAME} depends on file: $$prog - not found"; \
			fi; \
		else \
			for d in `echo $$PATH | tr ':' ' '`; do \
				if [ -x $$d/$$prog ]; then \
					found="$$d/$$prog"; \
					break; \
				fi \
			done; \
			${ECHO_MSG} "===>  ${PKGNAME} depends on executable: $$prog - $$found found"; \
		fi; \
		if [ X"$$found" = Xnot ]; then \
			${ECHO_MSG} "===>  Verifying $$target for $$prog in $$dir"; \
			if [ ! -d "$$dir" ]; then \
				${ECHO_MSG} ">> No directory for $$prog.  Skipping.."; \
			else \
				(cd $$dir; ${MAKE} ${.MAKEFLAGS} $$target) ; \
				${ECHO_MSG} "===>  Returning to build of ${PKGNAME}"; \
			fi; \
		fi; \
	done
.endif
.else
	@${DO_NADA}
.endif

fetch-depends:	_DEPENDS_USE
build-depends:	_DEPENDS_USE
run-depends:	_DEPENDS_USE

lib-depends:
.if defined(LIB_DEPENDS)
.if !defined(NO_DEPENDS)
.if defined(NO_SHARED_LIBS)
	@for i in ${LIB_DEPENDS}; do \
		lib=`${ECHO} $$i | ${SED} -e 's/:.*//' -e 's|\([^\\]\)\.|\1\\\\.|g'`; \
		dir=`${ECHO} $$i | ${SED} -e 's/[^:]*://'`; \
		if ${EXPR} "$$dir" : '.*:' > /dev/null; then \
			target=`${ECHO} $$dir | ${SED} -e 's/.*://'`; \
			dir=`${ECHO} $$dir | ${SED} -e 's/:.*//'`; \
		else \
			target=${DEPENDS_TARGET}; \
		fi; \
		tmp=`mktemp /tmp/bpmXXXXXXXXXX`; \
		if ${LD} -r -o $$tmp -L${LOCALBASE}/lib -L${X11BASE}/lib -l$$lib; then \
			${ECHO_MSG} "===>  ${PKGNAME} depends on library: $$lib - found"; \
		else \
			${ECHO_MSG} "===>  ${PKGNAME} depends on library: $$lib - not found"; \
			${ECHO_MSG} "===>  Verifying $$target for $$lib in $$dir"; \
			if [ ! -d "$$dir" ]; then \
				${ECHO_MSG} ">> No directory for $$lib.  Skipping.."; \
			else \
				(cd $$dir; ${MAKE} ${.MAKEFLAGS} $$target) ; \
				${ECHO_MSG} "===>  Returning to build of ${PKGNAME}"; \
			fi; \
		fi; \
		${RM} -f $$tmp; \
	done
.else
	@for i in ${LIB_DEPENDS}; do \
		lib=`${ECHO} $$i | ${SED} -e 's/:.*//' -e 's|\([^\\]\)\.|\1\\\\.|g'`; \
		dir=`${ECHO} $$i | ${SED} -e 's/[^:]*://'`; \
		if ${EXPR} "$$dir" : '.*:' > /dev/null; then \
			target=`${ECHO} $$dir | ${SED} -e 's/.*://'`; \
			dir=`${ECHO} $$dir | ${SED} -e 's/:.*//'`; \
		else \
			target=${DEPENDS_TARGET}; \
		fi; \
		libname=`${ECHO} $$lib | ${SED} -e 's|\\\\||g'`; \
		reallib=`${LDCONFIG} -r | ${GREP} -e "-l$$lib" | awk '{ print $$3 }'`; \
		if [ "X$$reallib" = X"" ]; then \
			${ECHO_MSG} "===>  ${PKGNAME} depends on shared library: $$libname - not found"; \
			${ECHO_MSG} "===>  Verifying $$target for $$libname in $$dir"; \
			if [ ! -d "$$dir" ]; then \
				${ECHO_MSG} ">> No directory for $$libname.  Skipping.."; \
			else \
				(cd $$dir; ${MAKE} ${.MAKEFLAGS} $$target) ; \
				${ECHO_MSG} "===>  Returning to build of ${PKGNAME}"; \
			fi; \
		else \
			${ECHO_MSG} "===>  ${PKGNAME} depends on shared library: $$libname - $$reallib found"; \
		fi; \
	done
.endif
.endif
.else
	@${DO_NADA}
.endif

misc-depends:
.if defined(DEPENDS)
.if !defined(NO_DEPENDS)
	@for dir in ${DEPENDS}; do \
		if ${EXPR} "$$dir" : '.*:' > /dev/null; then \
			target=`${ECHO} $$dir | ${SED} -e 's/.*://'`; \
			dir=`${ECHO} $$dir | ${SED} -e 's/:.*//'`; \
		else \
			target=${DEPENDS_TARGET}; \
		fi; \
		${ECHO_MSG} "===>  ${PKGNAME} depends on: $$dir"; \
		${ECHO_MSG} "===>  Verifying $$target for $$dir"; \
		if [ ! -d $$dir ]; then \
			${ECHO_MSG} ">> No directory for $$dir.  Skipping.."; \
		else \
			(cd $$dir; ${MAKE} ${.MAKEFLAGS} $$target) ; \
		fi \
	done
	@${ECHO_MSG} "===>  Returning to build of ${PKGNAME}"
.endif
.else
	@${DO_NADA}
.endif

.endif

.if !target(clean-depends)
clean-depends:
.if defined(FETCH_DEPENDS) || defined(BUILD_DEPENDS) || defined(LIB_DEPENDS) \
	|| defined(RUN_DEPENDS)
	@for dir in `${ECHO} ${FETCH_DEPENDS} ${BUILD_DEPENDS} ${LIB_DEPENDS} ${RUN_DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/^[^:]*://' -e 's/:.*//' | sort -u`; do \
		if [ -d $$dir ] ; then \
			(cd $$dir; ${MAKE} NOCLEANDEPENDS=yes clean clean-depends); \
		fi \
	done
.endif
.if defined(DEPENDS)
	@for dir in `${ECHO} ${DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/:.*//' | sort -u`; do \
		if [ -d $$dir ] ; then \
			(cd $$dir; ${MAKE} NOCLEANDEPENDS=yes clean clean-depends); \
		fi \
	done
.endif
.endif

.if !target(depends-list)
depends-list:
	@for dir in `${ECHO} ${FETCH_DEPENDS} ${BUILD_DEPENDS} ${LIB_DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/^[^:]*://' -e 's/:.*//' | sort -u` `${ECHO} ${DEPENDS} | ${TR} '\040' '\012' | ${SED} -e 's/:.*//' | sort -u`; do \
		(cd $$dir; ${MAKE} package-name depends-list PACKAGE_NAME_AS_LINK=${PACKAGE_NAME_AS_LINK}; ); \
	done
.endif

################################################################
# Everything after here are internal targets and really
# shouldn't be touched by anybody but the release engineers.
################################################################

# This target generates an index entry suitable for aggregation into
# a large index.  Format is:
#
# distribution-name|port-path|installation-prefix|comment| \
#  description-file|maintainer|categories|build deps|run deps|for arch
#
.if !target(describe)
describe:
	@${ECHO} -n "${PKGNAME}|${.CURDIR}|"; \
	${ECHO} -n "${PREFIX}|"; \
	if [ -f ${COMMENT} ]; then \
		${ECHO} -n "`${CAT} ${COMMENT}`"; \
	else \
		${ECHO} -n "** No Description"; \
	fi; \
	if [ -f ${DESCR} ]; then \
		${ECHO} -n "|${DESCR}"; \
	else \
		${ECHO} -n "|/dev/null"; \
	fi; \
	${ECHO} -n "|${MAINTAINER}|${CATEGORIES}|"; \
	case "A${FETCH_DEPENDS}B${BUILD_DEPENDS}C${LIB_DEPENDS}D${DEPENDS}E" in \
		ABCDE) ;; \
		*) cd ${.CURDIR} && ${ECHO} -n `make depends-list|sort -u`;; \
	esac; \
	${ECHO} -n "|"; \
	case "A${RUN_DEPENDS}B${LIB_DEPENDS}C${DEPENDS}D" in \
		ABCD) ;; \
		*) cd ${.CURDIR} && ${ECHO} -n `make package-depends|sort -u`;; \
	esac; \
	${ECHO} -n "|"; \
	if [ "${ONLY_FOR_ARCHS}" = "" ]; then \
		${ECHO} -n "any"; \
	else \
		${ECHO} -n "${ONLY_FOR_ARCHS}"; \
	fi; \
	${ECHO} ""
.endif

.if !target(readmes)
readmes:	readme
.endif

.if !target(readme)
readme:
	@rm -f README.html
	@cd ${.CURDIR} && make README.html
.endif

README_NAME=	${TEMPLATES}/README.port

README.html:
	@${ECHO_MSG} "===>  Creating README.html for ${PKGNAME}"
	@${MAKE} depends-list PACKAGE_NAME_AS_LINK=YES >> $@.tmp1
	@[ -s $@.tmp1 ] || echo "(none)" >> $@.tmp1
	@${MAKE} package-depends PACKAGE_NAME_AS_LINK=YES >> $@.tmp2
	@[ -s $@.tmp2 ] || echo "(none)" >> $@.tmp2
	@${ECHO} ${PKGNAME} | ${HTMLIFY} >> $@.tmp3
	@${CAT} ${README_NAME} | \
		${SED} -e 's|%%PORT%%|'"`${MAKE} package-path | ${HTMLIFY}`"'|g' \
			-e '/%%PKG%%/r$@.tmp3' \
			-e '/%%PKG%%/d' \
			-e '/%%COMMENT%%/r${PKGDIR}/COMMENT' \
			-e '/%%COMMENT%%/d' \
			-e '/%%BUILD_DEPENDS%%/r$@.tmp1' \
			-e '/%%BUILD_DEPENDS%%/d' \
			-e '/%%RUN_DEPENDS%%/r$@.tmp2' \
			-e '/%%RUN_DEPENDS%%/d' \
		>> $@
	@rm -f $@.tmp1 $@.tmp2 $@.tmp3

.if !target(print-depends-list)
print-depends-list:
.if defined(FETCH_DEPENDS) || defined(BUILD_DEPENDS) || \
	defined(LIB_DEPENDS) || defined(DEPENDS)
	@${ECHO} -n 'This port requires package(s) "'
	@${ECHO} -n `make depends-list | sort -u`
	@${ECHO} '" to build.'
.endif
.endif

.if !target(print-package-depends)
print-package-depends:
.if defined(RUN_DEPENDS) || defined(LIB_DEPENDS) || defined(DEPENDS)
	@${ECHO} -n 'This port requires package(s) "'
	@${ECHO} -n `make package-depends | sort -u`
	@${ECHO} '" to run.'
.endif
.endif

# Fake installation of package so that user can pkg_delete it later.
# Also, make sure that an installed port is recognized correctly in
# accordance to the @pkgdep directive in the packing lists

.if !target(fake-pkg)
fake-pkg:
	@if [ ! -f ${PLIST} -o ! -f ${COMMENT} -o ! -f ${DESCR} ]; then ${ECHO} "** Missing package files for ${PKGNAME} - installation not recorded."; exit 1; fi
	@if [ `/bin/ls -l ${COMMENT} | ${AWK} '{print $$5}'` -gt 60 ]; then \
	    ${ECHO} "** ${COMMENT} too large - installation not recorded."; \
	    exit 1; \
	 fi
	@if [ ! -d ${PKG_DBDIR} ]; then ${RM} -f ${PKG_DBDIR}; ${MKDIR} ${PKG_DBDIR}; fi
.if defined(FORCE_PKG_REGISTER)
	@${RM} -rf ${PKG_DBDIR}/${PKGNAME}
.endif
	@if [ ! -d ${PKG_DBDIR}/${PKGNAME} ]; then \
		${ECHO_MSG} "===>  Registering installation for ${PKGNAME}"; \
		${MKDIR} ${PKG_DBDIR}/${PKGNAME}; \
		${PKG_CMD} ${PKG_ARGS} -O ${PKGFILE} > ${PKG_DBDIR}/${PKGNAME}/+CONTENTS; \
		${CP} ${DESCR} ${PKG_DBDIR}/${PKGNAME}/+DESC; \
		${CP} ${COMMENT} ${PKG_DBDIR}/${PKGNAME}/+COMMENT; \
		if [ -f ${PKGDIR}/INSTALL ]; then \
			${CP} ${PKGDIR}/INSTALL ${PKG_DBDIR}/${PKGNAME}/+INSTALL; \
		fi; \
		if [ -f ${PKGDIR}/DEINSTALL ]; then \
			${CP} ${PKGDIR}/DEINSTALL ${PKG_DBDIR}/${PKGNAME}/+DEINSTALL; \
		fi; \
		if [ -f ${PKGDIR}/REQ ]; then \
			${CP} ${PKGDIR}/REQ ${PKG_DBDIR}/${PKGNAME}/+REQ; \
		fi; \
		if [ -f ${PKGDIR}/MESSAGE ]; then \
			${CP} ${PKGDIR}/MESSAGE ${PKG_DBDIR}/${PKGNAME}/+DISPLAY; \
		fi; \
		for dep in `make package-depends ECHO_MSG=${TRUE} | sort -u`; do \
			if [ -d ${PKG_DBDIR}/$$dep ]; then \
				if ! ${GREP} ^${PKGNAME}$$ ${PKG_DBDIR}/$$dep/+REQUIRED_BY \
					>/dev/null 2>&1; then \
					${ECHO} ${PKGNAME} >> ${PKG_DBDIR}/$$dep/+REQUIRED_BY; \
				fi; \
			fi; \
		done; \
	fi
.endif

# Depend is generally meaningless for arbitrary ports, but if someone wants
# one they can override this.  This is just to catch people who've gotten into
# the habit of typing `make depend all install' as a matter of course.
#
.if !target(depend)
depend:
.endif

# Same goes for tags
.if !target(tags)
tags:
.endif

.PHONY: \
   addsum all build build-depends checkpatch \
   checksum clean clean-depends configure deinstall \
   delete-package delete-package-links depend depends depends-list \
   describe distclean do-build do-configure do-extract \
   do-fetch do-install do-package do-patch extract \
   fake-pkg fetch fetch-depends fetch-list fetch-list-one-pkg \
   fetch-list-recursive install lib-depends makesum mirror-distfiles \
   misc-depends package package-depends package-links package-name \
   package-noinstall package-path patch plist post-build \
   post-configure post-extract post-fetch post-install post-package \
   post-patch pre-build pre-clean pre-configure pre-distclean \
   pre-extract pre-fetch pre-install pre-package pre-patch \
   pre-repackage print-depends-list print-package-depends readme \
   readmes real-extract real-fetch real-install reinstall \
   repackage run-depends tags uninstall
