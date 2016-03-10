#!/usr/local/bin/python2.7
# send ping6 fragment that will overlap the second fragment
# send atomic fragment with offset=0 and more=0, it must be processed

#      |XXXXXXXX|
# |-------------|

import os
from addr import *
from scapy.all import *

pid=os.getpid()
payload="ABCDEFGHIJKLMNOP"
dummy="0123456701234567"
packet=IPv6(src=SRC_OUT6, dst=DST_IN6)/ICMPv6EchoRequest(id=pid, data=payload)
frag=[]
frag.append(IPv6ExtHdrFragment(nh=58, id=pid, offset=1)/dummy)
frag.append(IPv6ExtHdrFragment(nh=58, id=pid)/str(packet)[40:64])
eth=[]
for f in frag:
	pkt=IPv6(src=SRC_OUT6, dst=DST_IN6)/f
	eth.append(Ether(src=SRC_MAC, dst=DST_MAC)/pkt)

if os.fork() == 0:
	time.sleep(1)
	sendp(eth, iface=SRC_IF)
	os._exit(0)

ans=sniff(iface=SRC_IF, timeout=3, filter=
    "ip6 and src "+DST_IN6+" and dst "+SRC_OUT6+" and icmp6")
for a in ans:
	if a and a.type == ETH_P_IPV6 and \
	    ipv6nh[a.payload.nh] == 'ICMPv6' and \
	    icmp6types[a.payload.payload.type] == 'Echo Reply':
		id=a.payload.payload.id
		print "id=%#x" % (id)
		if id != pid:
			print "WRONG ECHO REPLY ID"
			exit(2)
		data=a.payload.payload.data
		print "payload=%s" % (data)
		if data == payload:
			exit(0)
		print "PAYLOAD!=%s" % (payload)
		exit(2)
print "NO ECHO REPLY"
exit(1)
