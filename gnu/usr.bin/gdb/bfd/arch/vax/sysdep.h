/*	$Id: sysdep.h,v 1.1 1995/06/05 15:22:54 ragge Exp $ */
#ifndef	hosts_vaxbsd_H
/* DEC VAX running any BSD Unix */
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <sys/file.h>
#include <machine/param.h>

#ifndef	O_ACCMODE
#define O_ACCMODE (O_RDONLY | O_WRONLY | O_RDWR)
#endif

#define SEEK_SET 0
#define SEEK_CUR 1

#define	HOST_PAGE_SIZE			NBPG
#define	HOST_MACHINE_ARCH		bfd_arch_vax
#define	HOST_TEXT_START_ADDR		USRTEXT

#define	HOST_STACK_END_ADDR		USRSTACK

#define TRAD_UNIX_CORE_FILE_FAILING_SIGNAL(core_bfd) \
  ((core_bfd)->tdata.trad_core_data->u.u_sig)
#define u_comm u_kproc.kp_proc.p_comm

#include "fopen-same.h"

#define hosts_vaxbsd_H
#endif
