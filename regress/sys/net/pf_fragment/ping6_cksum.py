#!/usr/local/bin/python2.7
# check icmp6 checksum in returned icmp packet

import os
from addr import *
from scapy.all import *

dstaddr=sys.argv[1]
pid=os.getpid()
payload="a" * 1452
p=(Ether(src=SRC_MAC, dst=PF_MAC)/IPv6(src=SRC_OUT6, dst=dstaddr)/
    ICMPv6EchoRequest(id=pid, data=payload))
echocksum=IPv6(str(p.payload)).payload.cksum
print "echocksum=%#04x" % (echocksum)
a=srp1(p, iface=SRC_IF, timeout=2)
if a and a.type == scapy.layers.dot11.ETHER_TYPES.IPv6 and \
    ipv6nh[a.payload.nh] == 'ICMPv6' and \
    icmp6types[a.payload.payload.type] == 'Packet too big':
	outercksum=a.payload.payload.cksum
	print "outercksum=%#04x" % (outercksum)
	q=a.payload.payload.payload
	if ipv6nh[q.nh] == 'ICMPv6' and \
	    icmp6types[q.payload.type] == 'Echo Request':
		innercksum=q.payload.cksum
		print "innercksum=%#04x" % (innercksum)
		if innercksum == echocksum:
			exit(0)
		print "INNERCKSUM!=ECHOCKSUM"
		exit(1)
	print "NO INNER ECHO REQUEST"
	exit(2)
print "NO PACKET TOO BIG"
exit(2)
