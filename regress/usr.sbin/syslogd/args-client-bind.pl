# Syslog binds UDP socket to localhost.
# The client writes a message to a localhost UDP socket.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that the file log contains the localhost name.
# Check that fstat contains a bound UDP socket.

use strict;
use warnings;

our %args = (
    client => {
	connect => { domain => AF_UNSPEC, addr => "localhost", port => 514 },
	loggrep => {
	    qr/connect sock: (127.0.0.1|::1) \d+/ => 1,
	    get_testlog() => 1,
	},
    },
    syslogd => {
	options => ["-U", "localhost"],
	fstat => {
	    qr/^root .* internet/ => 0,
	    qr/^_syslogd .* internet/ => 3,
	    qr/ internet6? dgram udp (127.0.0.1|\[::1\]):514$/ => 1,
	},
    },
    file => {
	loggrep => qr/ localhost /. get_testlog(),
    },
);

1;
