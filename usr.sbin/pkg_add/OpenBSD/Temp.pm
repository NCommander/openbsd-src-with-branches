# ex:ts=8 sw=4:
# $OpenBSD: Temp.pm,v 1.6 2005/10/10 10:31:46 espie Exp $
#
# Copyright (c) 2003-2005 Marc Espie <espie@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use strict;
use warnings;
package OpenBSD::Temp;

use File::Temp;
use File::Path;

our $tempbase = $ENV{'PKG_TMPDIR'} || '/var/tmp';

my $dirs = [];
my $files = [];

my $handler = sub {
	my ($sig) = @_;
	File::Path::rmtree($dirs);
	unlink(@$files);
	$SIG{$sig} = 'DEFAULT';
	kill $sig, $$;
};

$SIG{'INT'} = $handler;
$SIG{'QUIT'} = $handler;
$SIG{'HUP'} = $handler;
$SIG{'KILL'} = $handler;
$SIG{'TERM'} = $handler;

sub dir()
{
	my $caught;
	my $h = sub { $caught = shift; };
	my $dir;
		
	{
	    local $SIG{'INT'} = $h;
	    local $SIG{'QUIT'} = $h;
	    local $SIG{'HUP'} = $h;
	    local $SIG{'KILL'} = $h;
	    local $SIG{'TERM'} = $h;
	    $dir = File::Temp::tempdir("pkginfo.XXXXXXXXXXX", 
	    	DIR => $tempbase, CLEANUP => 1).'/';
	    push(@$dirs, $dir);
	}
	if (defined $caught) {
		kill $caught, $$;
	}
	return $dir;
}

sub file()
{
	my $caught;
	my $h = sub { $caught = shift; };
	my ($fh, $file);
		
	{
	    local $SIG{'INT'} = $h;
	    local $SIG{'QUIT'} = $h;
	    local $SIG{'HUP'} = $h;
	    local $SIG{'KILL'} = $h;
	    local $SIG{'TERM'} = $h;
	    ($fh, $file) = File::Temp::tempfile("pkgout.XXXXXXXXXXX", 
	    	DIR => $tempbase, CLEANUP => 1);
	    push(@$files, $file);
	}
	if (defined $caught) {
		kill $caught, $$;
	}
	return $file;
}

sub list($)
{
	return File::Temp::tempfile("list.XXXXXXXXXXX", DIR => shift);
}

1;
