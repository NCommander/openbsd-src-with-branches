/*	$OpenBSD: assert.h,v 1.1 1998/12/15 04:45:50 smurph Exp $ */
#define assert(x) \
({\
	if (!(x)) {\
		printf("assertion failure \"%s\" line %d file %s\n", \
		#x, __LINE__, __FILE__); \
		panic("assertion"); \
	} \
})
