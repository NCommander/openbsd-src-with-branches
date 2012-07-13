# $OpenBSD: Library.pm,v 1.7 2012/07/13 08:44:20 espie Exp $

# Copyright (c) 2007-2010 Steven Mestdagh <steven@openbsd.org>
# Copyright (c) 2012 Marc Espie <espie@openbsd.org>
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

package LT::Library::Stash;

sub new
{
	my $class = shift;

	bless {}, $class;
}

sub create
{
	my ($self, $key) = @_;
	if (!exists $self->{$key}) {
		$self->{$key} = LT::Library->new($key);
	}
	return $self->{$key};
}

package LT::Library;

use LT::Util;
use LT::Trace;

# find actual library filename
# XXX pick the right one if multiple are found!
sub resolve_library
{
	my ($self, $dirs, $shared, $staticflag, $linkmode, $gp) = @_;

	my $libtofind = $self->{key};
	my $libfile = 0;
	my @globbedlib;

	my $pic = '';	# used when finding static libraries
	if ($linkmode eq 'LT::LaFile') {
		$pic = '_pic';
	}

	if (defined $self->{lafile}) {
		require LT::LaFile;
		# if there is a .la file, use the info from there
		tsay {"found .la file $self->{lafile} for library key: ",
		    $self->{key}};
		my $lainfo = LT::LaFile->parse($self->{lafile});
		my $dlname = $lainfo->{dlname};
		my $oldlib = $lainfo->{old_library};
		my $libdir = $lainfo->{libdir};
		my $installed = $lainfo->{installed};
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
			tsay {".la file ", $self->{lafile}, 
			    "points to nonexistent file ", $libfile, " !"};
		}
	} else {
		# otherwise, search the filesystem
		# sort dir search order by priority
		# XXX not fully correct yet
		my @sdirs = sort { $dirs->{$b} <=> $dirs->{$a} } keys %$dirs;
		# search in .libs when priority is high
		map { $_ = "$_/$ltdir" if (exists $dirs->{$_} && $dirs->{$_} > 3) } @sdirs;
		push @sdirs, $gp->libsearchdirs if $gp;
		tsay {"searching for $libtofind"};
		tsay {"search path= ", join(':', @sdirs)};
		tsay {"search type= ", $shared ? 'shared' : 'static'};
		foreach my $sd (@sdirs) {
		       if ($shared) {
				# select correct library by sorting by version number only
				my $bestlib = $self->findbest($sd, $libtofind);
				if ($bestlib) {
					tsay {"found $libtofind in $sd"};
					$libfile = $bestlib;
					last;
				} else {	
					# XXX find static library instead?
					my $spath = "$sd/lib$libtofind$pic.a";
					if (-f $spath) {
						tsay {"found static $libtofind in $sd"};
						$libfile = $spath;
						last;
					}
				}
		       } else {
				# look for a static library
				my $spath = "$sd/lib$libtofind.a";
				if (-f $spath) {
					tsay {"found static $libtofind in $sd"};
					$libfile = $spath;
					last;
				}
		       }
		}
	}
	if (!$libfile) {
		delete $self->{fullpath};
		if ($linkmode eq 'LT::LaFile') {
			say "warning: dependency on $libtofind dropped";
			$self->{dropped} = 1;
		} elsif ($linkmode eq 'LT::Program') {
			die "Link error: $libtofind not found!\n";
		}
	} else {
		$self->{fullpath} = $libfile;
		tsay {"\$libs->{$self->{key}}->{fullpath} = ", 
		    $self->{fullpath}};
	}
}

sub findbest
{
	my ($self, $sd, $name) = @_;
	my $best = undef;
	if (opendir(my $dir, $sd)) {
		my ($major, $minor) = (-1, -1);
		while (my $_ = readdir($dir)) {
			next unless m/^lib\Q$name\E\.so\.(\d+)\.(\d+)$/;
			if ($1 > $major || ($1 == $major && $2 > $minor)) {
				($major, $minor) = ($1, $2);
				$best = "$sd/$_";
			}
		}
		closedir($dir);
	}
	return $best;
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
	tsay {"inspecting $filename for library dependencies..."};
	open(my $fh, '-|', "objdump", "-p", "--", $filename);
	while (<$fh>) {
		if (m/\s+NEEDED\s+(\S+)\s*$/) {
			push @deps, $1;
		}
	}
	tsay {"found ", (@deps == 0) ? 'no ' : '',
		"deps for $filename\n@deps"};
	return @deps;
}

sub new
{
	my ($class, $key) = @_;
	bless { key => $key }, $class;
}

1;
