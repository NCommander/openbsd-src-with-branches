# test ENOTCONN for splicing to unconnected udp socket

use strict;
use warnings;
use IO::Socket;
use BSD::Socket::Splice "SO_SPLICE";

our %args = (
    errno => 'ENOTCONN',
    func => sub {
	my $sb = IO::Socket::INET->new(
	    Proto => "udp",
	    LocalAddr => "127.0.0.1",
	) or die "bind socket failed: $!";

	my $sc = IO::Socket::INET->new(
	    Proto => "udp",
	    PeerAddr => $sb->sockhost(),
	    PeerPort => $sb->sockport(),
	) or die "connect socket failed: $!";

	$sb->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $sc->fileno()))
	    or die "splice from unconnected socket failed: $!";
	$sc->setsockopt(SOL_SOCKET, SO_SPLICE, pack('i', $sb->fileno()))
	    and die "splice to unconnected socket succeeded";
    },
);
