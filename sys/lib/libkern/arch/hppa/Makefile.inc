#	$OpenBSD$

SRCS+=	__main.c imax.c imin.c lmax.c lmin.c max.c min.c ulmax.c ulmin.c \
	random.c ashrdi3.c

# `source' files built from m4 source
SRCS+=	divU.S divI.S
SRCS+=	remU.S remI.S
SRCS+=	mulU.S mulI.S

