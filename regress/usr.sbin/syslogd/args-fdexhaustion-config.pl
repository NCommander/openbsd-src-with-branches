# The syslogd is started with reduced file descriptor limits.
# The syslogd config contains more log files than possible.
# The client writes a message to Sys::Syslog native method.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check the error messages and multiple log file content.

use strict;
use warnings;

our %args = (
    syslogd => {
	conf => join("", map { "*.*\t\$objdir/file-$_.log\n" } 0..19),
	rlimit => {
	    RLIMIT_NOFILE => 30,
	},
	loggrep => {
	    qr/syslogd: receive_fd: recvmsg: Message too long/ => 5*2,
	    # One file is opened by test default config, 20 by multifile.
	    qr/X FILE:/ => 1+15,
	    qr/X UNUSED:/ => 5,
	},
    },
    multifile => [
	(map { { loggrep => get_testgrep() } } 0..14),
	(map { { loggrep => { qr/./s => 0 } } } 15..19),
    ],
);

1;
