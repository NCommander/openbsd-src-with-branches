/*	$OpenBSD$	*/


#ifndef _HPPA_FRAME_H_
#define _HPPA_FRAME_H_

#define	FRAME_PC	0

struct trapframe {
	int i;
	int tf_regs[10];
};

#endif
