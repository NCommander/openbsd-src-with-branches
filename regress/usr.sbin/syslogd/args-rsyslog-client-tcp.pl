# Test TCP with rsyslogd as sender.
# The client writes a message to rsyslogd UDP socket.
# The rsyslogd forwards the message to syslogd TCP listen socket.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the rsyslogd.
# The rsyslogd receives the message on its UDP socket.
# Find the message in rsyslogd, file, pipe, syslogd, server log.
# Check that the message is in rsyslogd, syslogd, server log.

use strict;
use warnings;

our %args = (
    client => {
	connect => { domain => AF_INET, proto => "udp", addr => "127.0.0.1" },
    },
    rsyslogd => {
	listen => { domain => AF_INET, proto => "udp", addr => "127.0.0.1" },
	connect => { domain => AF_INET, proto => "tcp", addr => "127.0.0.1",
	    port => 514 },
    },
    syslogd => {
	options => ["-T", "127.0.0.1:514"],
    },
);

1;
