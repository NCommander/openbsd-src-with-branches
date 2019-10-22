#	$OpenBSD$

.PATH: ${.CURDIR}/libunbound/services

SRCS+=	authzone.c listen_dnsport.c localzone.c mesh.c modstack.c \
	outbound_list.c outside_network.c view.c
