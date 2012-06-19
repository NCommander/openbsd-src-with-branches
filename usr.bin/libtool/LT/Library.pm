# $OpenBSD: Library.pm,v 1.2 2011/11/14 22:14:38 jasper Exp $

# Copyright (c) 2007-2010 Steven Mestdagh <steven@openbsd.org>
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
use feature qw(say switch state);

package LT::Library;

use LT::Util;

# find actual library filename
# XXX pick the right one if multiple are found!
sub find
{
	my ($self, $dirs, $shared, $staticflag, $linkmode, $ldconfigdirs) = @_;

	my $libtofind = $self->{key};
	my $libfile = 0;
	my @globbedlib;

	my $pic = '';	# used when finding static libraries
	if ($linkmode eq 'LaFile') {
		$pic = '_pic';
	}

	if (defined $self->{lafile}) {
		require LT::LaFile;
		# if there is a .la file, use the info from there
		LT::Trace::debug {"found .la file $self->{lafile} for library key: $self->{key}\n"};
		my $lainfo = LT::LaFile->parse($self->{lafile});
		my $dlname = $lainfo->{'dlname'};
		my $oldlib = $lainfo->{'old_library'};
		my $libdir = $lainfo->{'libdir'};
		my $installed = $lainfo->{'installed'};
 		my $d = abs_dir($self->{lafile});
		# get the name we need (this may include a -release)
		if (!$dlname && !$oldlib) {
			die "Link error: neither static nor shared library found in $self->{lafile}\n";
		}
		if ($d !~ m/\Q$ltdir\E$/ && $installed eq 'no') {
			$d .= "/$ltdir";
		}
		if ($shared) {
			if ($dlname) {
				$libfile = "$d/$dlname";
			} else {
				# fall back to static
				$libfile = "$d/$oldlib";
			}
			# if -static has been passed, don't link dynamically
			# against not-installed libraries
			if ($staticflag && $installed eq 'no') {
				$libfile = "$d/$oldlib";
			}
		} else {
			$libfile = "$d/$oldlib";
		}
		if (! -f $libfile) {
			LT::Trace::debug {".la file $self->{lafile} points to nonexistent file $libfile !\n"};
		}
	} else {
		# otherwise, search the filesystem
		# sort dir search order by priority
		# XXX not fully correct yet
		my @sdirs = sort { $dirs->{$b} <=> $dirs->{$a} } keys %$dirs;
		# search in .libs when priority is high
		map { $_ = "$_/$ltdir" if (exists $dirs->{$_} && $dirs->{$_} > 3) } @sdirs;
		push @sdirs, @$ldconfigdirs if ($ldconfigdirs);
		LT::Trace::debug {"searching for $libtofind\n"};
		LT::Trace::debug {"search path= ", join(':', @sdirs), "\n"};
		LT::Trace::debug {"search type= ", ($shared) ? 'shared' : 'static', "\n"};
		foreach my $sd (@sdirs) {
		   if ($shared) {
			# select correct library by sorting by version number only
			@globbedlib = sort { my ($x,$y) =
			map { /\.so\.(\d+\.\d+)$/; $1 } ($a,$b); $y <=> $x }
			glob "$sd/lib$libtofind.so.*.*";
			if ($globbedlib[0]) {
				LT::Trace::debug {"found $libtofind in $sd\n"};
				$libfile = $globbedlib[0];
				last;
			} else {	# XXX find static library instead?
				my $spath = "$sd/lib$libtofind$pic.a";
				if (-f $spath) {
					LT::Trace::debug {"found static $libtofind in $sd\n"};
					$libfile = $spath;
					last;
				}
			}
		   } else {
			# look for a static library
			my $spath = "$sd/lib$libtofind.a";
			if (-f $spath) {
				LT::Trace::debug {"found static $libtofind in $sd\n"};
				$libfile = $spath;
				last;
			}
		   }
		}
	}
	if (!$libfile) {
		if (defined $self->{fullpath}) { delete $self->{fullpath}; }
		if ($linkmode eq 'LaFile') {
			say "warning: dependency on $libtofind dropped";
			$self->{dropped} = 1;
		} elsif ($linkmode eq 'Program') {
			die "Link error: $libtofind not found!\n";
		}
	} else {
		$self->{fullpath} = $libfile;
		LT::Trace::debug {"\$libs->{$self->{key}}->{fullpath} = ", $self->{fullpath}, "\n"};
	}
}

# give a list of library dependencies found in the actual shared library
sub inspect
{
	my $self = shift;

	my $filename = $self->{fullpath};
	my @deps;

	if (!defined($filename)){
		say "warning: library was specified that could not be found: $self->{key}";
		return;
	}
	LT::Trace::debug {"inspecting $filename for library dependencies...\n"};
	open(my $fh, '-|', "objdump -p $filename");
	while (<$fh>) {
		if (m/\s+NEEDED\s+(\S+)\s*$/) {
			push @deps, $1;
		}
	}
	LT::Trace::debug {"found ", (@deps == 0) ? 'no ' : '',
		"deps for $filename\n@deps\n"};
	return @deps;
}

sub new
{
	my ($class, $key) = @_;
	bless { key => $key }, $class;
}

1;
