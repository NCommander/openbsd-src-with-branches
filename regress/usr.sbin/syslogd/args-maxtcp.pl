# The syslogd listens on 127.0.0.1 TCP socket.
# The client creates MAXTCP connections to syslogd TCP socket.
# The syslogd writes it into a file and through a pipe.
# The syslogd passes it via UDP to the loghost.
# The server receives the message on its UDP socket.
# Find the message in client, file, pipe, syslogd, server log.
# Check that an additional connection gets denied by syslogd.
# Check that close and reopen a connection succeeds.
# Check that every connection transfers a message.

use strict;
use warnings;
use IO::Socket::INET6;
use constant MAXTCP => 20;

our %args = (
    client => {
	connect => { domain => AF_INET, proto => "tcp", addr => "127.0.0.1",
	    port => 514 },
	func => sub {
	    my $self = shift;
	    local $| = 1;
	    my @s;
	    $s[0] = \*STDOUT;
	    # open all additional connections and one more
	    for (my $i = 1; $i <= MAXTCP; $i++) {
		$s[$i] = IO::Socket::INET6->new(
		    Domain              => AF_INET,
		    Proto               => "tcp",
		    PeerAddr            => "127.0.0.1",
		    PeerPort            => 514,
		) or die "tcp socket $i connect failed: $!";
	    }
	    # the last connection was denied
	    defined $s[MAXTCP]->getline()
		and die "MAXTCP connection not closed by syslogd\n";
	    # close and reopen a single connection
	    close($s[1])
		or die "tcp socket 1 close failed: $!";
	    ${$self->{syslogd}}->loggrep("tcp logger .* connection close", 5)
		or die ref($self), " syslogd did not close connection";
	    $s[1] = IO::Socket::INET6->new(
		Domain              => AF_INET,
		Proto               => "tcp",
		PeerAddr            => "127.0.0.1",
		PeerPort            => 514,
	    ) or die "tcp socket 1 connect again failed: $!";
	    # write messages over all connections
	    for (my $i = 0; $i < MAXTCP; $i++) {
		my $msg = get_testlog(). " $i tcp socket";
		my $fh = $s[$i];
		print $fh "$msg\n";
		print STDERR "<<< $msg\n";
	    }
	    ${$self->{syslogd}}->loggrep("tcp logger .* complete line", 5,
		MAXTCP) or die ref($self),
		" syslogd did not receive all complete lines";
	    write_shutdown($self);
	},
    },
    syslogd => {
	options => ["-T", "127.0.0.1:514"],
	fstat => {
	    qr/^root .* internet/ => 0,
	    qr/^_syslogd .* internet/ => 3,
	    qr/ internet6? stream tcp \w+ (127.0.0.1|\[::1\]):514$/ => 1,
	},
	loggrep => {
	    qr/tcp logger .* accepted/ => MAXTCP+1,
	    qr/tcp logger .* denied: maximum /.MAXTCP.qr/ reached/ => 1,
	},
    },
    file => {
	loggrep => {
	    qr/ localhost /.get_testlog().qr/ \d+ tcp socket/ => MAXTCP,
	},
    },
);

1;
