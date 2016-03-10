# The client writes a message with sendsyslog syscall.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Create a ktrace dump of the client and check that sendsyslog(2)
# has been used.

use strict;
use warnings;

our %args = (
    client => {
	connect => { domain => "sendsyslog", version => 0 },
	ktrace => {
	    qr/CALL  sendsyslog\(/ => 1,
	    qr/CALL  sendsyslog2\(/ => 1,
	    qr/GIO   fd -1 wrote \d+ bytes/ => 2,
	    qr/RET   sendsyslog 0/ => 1,
	    qr/RET   sendsyslog2 0/ => 1,
	},
    },
);

1;
