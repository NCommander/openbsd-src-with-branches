#!/usr/bin/perl -w
# ex:ts=8 sw=4:

# $OpenBSD: makewhatis.pl,v 1.6 2000/04/12 20:46:18 espie Exp $
#
# Copyright (c) 2000 Marc Espie.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# THIS SOFTWARE IS PROVIDED BY THE OPENBSD PROJECT AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OPENBSD
# PROJECT OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

use strict;
use File::Find;
use IO::File;
use Getopt::Std;

my ($picky, $testmode);

# write_uniques($list, $file):
#
#   write $list to file named $file, removing duplicate entries.
#   Change $file mode/owners to expected values
#
sub write_uniques
{
    my $list = shift;
    my $f = shift;
    my ($out, $last);
    local $_;

    $out = new IO::File $f, "w" or die "$0: Can't open $f";

    my @sorted = sort @$list;

    while ($_ = shift @sorted) {
	print $out $_, "\n" unless defined $last and $_ eq $last;
	$last = $_;
    }
    close $out;
    chmod 0444, $f;
    chown 0, (getgrnam 'bin')[2], $f;
}

# add_unformated_subject($lines, $toadd, $section):
#
#   build subject from list of $toadd lines, and add it to the list
#   of current subjects as section $section
#
sub add_unformated_subject
{
    my $subjects = shift;
    my $toadd = shift;
    my $section = shift;
    local $_ = join(' ', @$toadd);
	# unbreakable spaces
    s/\\\s+/ /g;
	# em dashes
    s/\\\(em\s+/- /g;
	# font changes
    s/\\f[BIRP]//g;
    s/\\-/($section) -/ || s/\s-\s/ ($section) - /;
	# other dashes
    s/\\-/-/g;
	# sequence of spaces
    s/\s+$//;
    s/\s+/ /g;
	# escaped characters
    s/\\\&(.)/$1/g;
	# gremlins...
    s/\\c//g;
    push(@$subjects, $_);
}

# $lines = handle_unformated($file)
#
#   handle an unformated manpage in $file
#
#   may return several subjects, perl(3p) do !
#
sub handle_unformated
{
    my $f = shift;
    my $filename = shift;
    my @lines = ();
    my $so_found = 0;
    local $_;
	# retrieve basename of file
    my ($name, $section) = $filename =~ m|(?:.*/)?(.*)\.([\w\d]+)|;
	# scan until macro
    while (<$f>) {
	next unless m/^\./;
	if (m/^\.de/) {
	    while (<$f>) {
		last if m/^\.\./;
	    }
	    next;
	}
	    # Some cross-refs just link to another manpage
	$so_found = 1 if m/\.so/;
	if (m/^\.TH/ || m/^\.th/) {
		# in pricky mode, we should try to match these
	    # ($name2, $section2) = m/^\.(?:TH|th)\s+(\S+)\s+(\S+)/;
	    	# scan until first section
	    while (<$f>) {
		next unless m/^\./;
		if (m/^\.SH/ || m/^\.sh/) {
		    my @subject = ();
		    while (<$f>) {
			last if m/^\.SH/ || m/^\.sh/ || m/^\.SS/ ||
			    m/^\.ss/ || m/^\.nf/;
			    # several subjects in one manpage
			if (m/^\.PP/ || m/^\.br/ || m/^\.PD/ || /^\.sp/) {
			    add_unformated_subject(\@lines, \@subject, $section) 
				if @subject != 0;
			    @subject = ();
			    next;
			}
			next if m/^\'/ || m/^\.tr\s+/ || m/^\.\\\"/ || m/^\.sv/;
			if (m/^\.de/) {
			    while (<$f>) {
				last if m/^\.\./;
			    }
			    next;
			}
			chomp;
			s/\.(?:B|I|IR|SM)\s+//;
			push(@subject, $_) unless m/^\s*$/;
		    }
		    add_unformated_subject(\@lines, \@subject, $section) 
			if @subject != 0;
		    return \@lines;
		}
	    }
	    print STDERR "Couldn't find subject in old manpage $filename\n";
	} elsif (m/^\.Dt/) {
	    $section .= "/$1" if (m/^\.Dt\s+\S+\s+\d\S*\s+(\S+)/);
	    while (<$f>) {
		next unless m/^\./;
		if (m/^\.Sh/) {
		    # subject/keep is the only way to deal with Nm/Nd pairs
		    my @subject = ();
		    my @keep = ();
		    my $nd_seen = 0;
		    while (<$f>) {
			last if m/^\.Sh/;
			s/\s,/,/g;
			if (s/^\.(..)\s+//) {
			    my $macro = $1;
			    next if $macro eq "\\\"";
			    s/\"(.*?)\"/$1/g;
			    s/\\-/-/g;
			    $macro eq 'Xr' and s/^(\S+)\s+(\d\S*)/$1 ($2)/;
			    $macro eq 'Ox' and s/^/OpenBSD /;
			    $macro eq 'Nx' and s/^/NetBSD /;
			    if ($macro eq 'Nd') {
				if (@keep != 0) {
				    add_unformated_subject(\@lines, \@keep, $section);
				    @keep = ();
				}
				push(@subject, "\\-");
				$nd_seen = 1;
			    }
			    if ($nd_seen && $macro eq 'Nm') {
				@keep = @subject;
				@subject = ();
				$nd_seen = 0;
			    }
			}
			push(@subject, $_) unless m/^\s*$/;
		    }
		    unshift(@subject, @keep) if @keep != 0;
		    add_unformated_subject(\@lines, \@subject, $section)
			if @subject != 0;
		    return \@lines;
		}
	    }
	}
    }
    if ($so_found == 0) {
	print STDERR "Unknown manpage type $filename\n";
    }
    return \@lines;
}
			
# add_formated_subject($subjects, $_, $section):
#   add subject $_ to the list of current $subjects, in section $section.
#
sub add_formated_subject
{
    my $subjects = shift;
    local $_ = shift;
    my $section = shift;

    # some twits underline the command name
    while (s/_\cH//g || s/(.)\cH\1/$1/g)
	{}
    if (m/-/) {
	s/([-+.\w\d,])\s+/$1 /g;
	s/([a-z][A-z])-\s+/$1/g;
	# some twits use: func -- description
	if (m/^[^-+.\w\d]*(.*) -(?:-?)\s+(.*)$/) {
	    my ($func, $descr) = ($1, $2);
	    $func =~ s/,\s*$//;
	    push(@$subjects, "$func ($section) - $descr");
	    return;
	}
    }

    print STDERR "Weird subject line in ", shift, ":\n", $_, "\n" if $picky;

    # try to find subject in line anyway
    if (m/^\s*(.*\S)(?:\s{3,}|\(\)\s+)(.*?)\s*$/) {
    	my ($func, $descr) = ($1, $2);
	$func =~ s/\s+/ /g;
	$descr =~ s/\s+/ /g;
	push(@$subjects, "$func ($section) - $descr");
	return;
    }

    print STDERR "Weird subject line in ", shift, ":\n", $_, "\n" unless $picky;
}

# $lines = handle_formated($file)
#
#   handle a formatted manpage in $file
#
#   may return several subjects, perl(3p) do !
#
sub handle_formated
{
    my $file = shift;
    my $filename = shift;
    local $_;
    my ($section, $subject);
    my @lines=();
    while (<$file>) {
	next if /^$/;
	chomp;
	# Remove boldface from wide characters
	while (s/(..)\cH\cH\1/$1/g)
	    {}
	# Remove boldface and underlining
	while (s/_\cH//g || s/(.)\cH\1/$1/g)
	    {}
	if (m/\w[-+.\w\d]*\(([-+.\w\d\/]+)\)/) {
	    $section = $1;
	    # Find architecture
	    if (m/Manual\s+\((.*?)\)/) {
		$section = "$section/$1";
	    }
	}
	# Not all man pages are in english
	# weird hex is `Namae' in japanese
	if (m/^(?:NAME|NAMN|Name|\xbe|\xcc\xbe\xbe\xce|\xcc\xbe\xc1\xb0)\s*$/) {
	    unless (defined $section) {
		# try to retrieve section from filename
		if ($filename =~ m/(?:cat|man)([\dln])\//) {
		    $section = $1;
		    print STDERR "Can't find section in $filename, deducting $section from context\n" if $picky;
		} else {
		    $section='??';
		    print STDERR "Can't find section in $filename\n";
		}
	    }
	    while (<$file>) {
		chomp;
		# perl agregates several subjects in one manpage
		if (m/^$/) {
		    add_formated_subject(\@lines, $subject, $section, $filename) 
			if defined $subject;
		    $subject = undef;
		} elsif (m/^\S/ || m/^\s+\*{3,}\s*$/) {
		    add_formated_subject(\@lines, $subject, $section, $filename) 
			if defined $subject;
		    last;
		} else {
		    $subject.=$_;
		}
	    }
	last;
	}
    }

    print STDERR "Can't parse $filename (not a manpage ?)\n" if @lines == 0;
    return \@lines;
}

# $list = find_manpages($dir)
#
#   find all manpages under $dir, trim some duplicates.
#
sub find_manpages
{
    my $dir = shift;
    my ($list, %nodes);
    $list=[];
    find(
	sub {
	return unless /\.[\dln]\w*(?:\.Z|\.gz)?$/;
	return unless -f $_;
	my $inode = (stat _)[1];
	return if defined $nodes{$inode};
	$nodes{$inode} = 1;
	push(@$list, $File::Find::name);
	}, $dir);
    return $list;
}

# $subjects = scan_manpages($list)
#
#   scan a set of manpages, return list of subjects
#
sub scan_manpages
{
    my $list = shift;
    local $_;
    my ($done);
    $done=[];

    for (@$list) {
	my ($file, $subjects);
	if (m/\.(?:Z|gz)$/) {
	    $file = new IO::File "gzip -fdc $_|";
	    $_ = $`;
	} else {
	    unless ($file = new IO::File $_) {
	    	warn "$0: Can't read $_\n";
		next;
	    }
	}
	if (m/\.[1-9ln][^.]*$/) {
	    $subjects = handle_unformated($file, $_);
	} elsif (m/\.0$/) {
	    $subjects = handle_formated($file, $_);
	    # in test mode, we try harder
	} elsif ($testmode) {
	    $subjects = handle_unformated($file, $_);
	    if (@$subjects == 0) {
	    	$subjects = handle_formated($file, $_);
	    }
	} else {
	    print STDERR "Can't find type of $_";
	    next;
	}
	push @$done, @$subjects;
    }
    return $done;
}

# build_index($dir)
#
#   build index for $dir
#
sub build_index
{
    my $dir = shift;
    my $list = find_manpages($dir);
    my $subjects = scan_manpages($list);
    write_uniques($subjects, "$dir/whatis.db");
}

# main code
    
my %opts;
getopts('tpd:', \%opts);

if (defined $opts{'p'}) {
    $picky = 1;
}
if (defined $opts{'t'}) {
    $testmode = 1;
    my $subjects = scan_manpages(\@ARGV);
    print join("\n", @$subjects), "\n";
    exit 0;
} 

if (defined $opts{'d'}) {
    my $mandir = $opts{'d'};
    unless (-d $mandir) {
	die "$0: $mandir: not a directory"
    }
    chdir $mandir;

    my $whatis = "$mandir/whatis.db";
    my $old = new IO::File $whatis or 
	die "$0 $whatis to merge with";
    my $subjects = scan_manpages(\@ARGV);
    while (<$old>) {
	chomp;
	push(@$subjects, $_);
    }
    close $old;
    write_uniques($subjects, $whatis);
    exit 0;
}
if ($#ARGV == -1) {
    local $_;
    @ARGV=();
    my $conf;
    $conf = new IO::File '/etc/man.conf' or 
	die "$0: Can't open /etc/man.conf";
    while (<$conf>) {
	chomp;
	push(@ARGV, $1) if /^_whatdb\s+(.*)\/whatis\.db\s*$/;
    }
    close $conf;
}
	
for my $mandir (@ARGV) {
    unless (-d $mandir) {
	die "$0: $mandir: not a directory"
    }
    build_index($mandir);
}


