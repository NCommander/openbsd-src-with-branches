#!/usr/local/bin/python2.7
# check ip and icmp checksum in returned icmp packet

import os
from addr import *
from scapy.all import *

dstaddr=sys.argv[1]
pid=os.getpid()
payload="a" * 1452
p=(Ether(src=SRC_MAC, dst=PF_MAC)/IP(flags="DF", src=SRC_OUT, dst=dstaddr)/
    ICMP(id=pid)/payload)
ipcksum=IP(str(p.payload)).chksum
print "ipcksum=%#04x" % (ipcksum)
echocksum=IP(str(p.payload)).payload.chksum
print "echocksum=%#04x" % (echocksum)
a=srp1(p, iface=SRC_IF, timeout=2)
if a and a.type == scapy.layers.dot11.ETHER_TYPES.IPv4 and \
    a.payload.proto == 1 and \
    icmptypes[a.payload.payload.type] == 'dest-unreach' and \
    icmpcodes[a.payload.payload.type][a.payload.payload.code] == \
    'fragmentation-needed':
	outeripcksum=a.payload.chksum
	print "outeripcksum=%#04x" % (outeripcksum)
	outercksum=a.payload.payload.chksum
	print "outercksum=%#04x" % (outercksum)
	q=a.payload.payload.payload
	inneripcksum=q.chksum
	print "inneripcksum=%#04x" % (inneripcksum)
	if q.proto == 1 and \
	    icmptypes[q.payload.type] == 'echo-request':
		innercksum=q.payload.chksum
		print "innercksum=%#04x" % (innercksum)
		if innercksum == echocksum:
			exit(0)
		print "INNERCKSUM!=ECHOCKSUM"
		exit(1)
	print "NO INNER ECHO REQUEST"
	exit(2)
print "NO FRAGMENTATION NEEDED"
exit(2)
