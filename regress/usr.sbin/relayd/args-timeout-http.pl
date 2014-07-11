# test that 3 seconds timeout occurs within 4 seconds idle in http

use strict;
use warnings;

our %args = (
    client => {
	func => \&http_client,
	len => 5,
	timefile => "",
    },
    relayd => {
	protocol => [ "http" ],
	relay => [ "session timeout 3" ],
    },
    server => {
	func => sub {
	    errignore();
	    http_server(@_);
	    sleep 5;
	    write_char(@_, 5);
	},
	sleep => 1,
	down => "Broken pipe",
	nocheck => 1,
    },
    len => 5,
);

1;
