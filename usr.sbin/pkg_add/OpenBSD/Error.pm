# ex:ts=8 sw=4:
# $OpenBSD: Error.pm,v 1.33 2019/07/11 07:03:45 espie Exp $
#
# Copyright (c) 2004-2010 Marc Espie <espie@openbsd.org>
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

use strict;
use warnings;

package OpenBSD::Auto;
sub cache(*&)
{
	my ($sym, $code) = @_;
	my $callpkg = caller;
	my $actual = sub {
		my $self = shift;
		return $self->{$sym} //= &$code($self);
	};
	no strict 'refs';
	*{$callpkg."::$sym"} = $actual;
}

package OpenBSD::Handler;

my $list = [];

sub register
{
	my ($class, $code) = @_;
	push(@$list, $code);
}

my $handler = sub {
	my $sig = shift;
	for my $c (@$list) {
		&$c($sig);
	}
	$SIG{$sig} = 'DEFAULT';
	kill $sig, $$;
};

sub reset
{
	$SIG{'INT'} = $handler;
	$SIG{'QUIT'} = $handler;
	$SIG{'HUP'} = $handler;
	$SIG{'KILL'} = $handler;
	$SIG{'TERM'} = $handler;
}

__PACKAGE__->reset;

package OpenBSD::Error;
require Exporter;
our @ISA=qw(Exporter);
our @EXPORT=qw(Copy Unlink try throw catch catchall rethrow);

our ($FileName, $Line, $FullMessage);

my @signal_name = ();

use Carp;

sub fillup_names
{
	{
	# XXX force autoload
	package verylocal;

	require POSIX;
	POSIX->import(qw(signal_h));
	}

	for my $sym (keys %POSIX::) {
		next unless $sym =~ /^SIG([A-Z].*)/;
		my $i = eval "&POSIX::$sym()";
		next unless defined $i;
		$signal_name[$i] = $1;
	}
	# extra BSD signals
	$signal_name[5] = 'TRAP';
	$signal_name[7] = 'IOT';
	$signal_name[10] = 'BUS';
	$signal_name[12] = 'SYS';
	$signal_name[16] = 'URG';
	$signal_name[23] = 'IO';
	$signal_name[24] = 'XCPU';
	$signal_name[25] = 'XFSZ';
	$signal_name[26] = 'VTALRM';
	$signal_name[27] = 'PROF';
	$signal_name[28] = 'WINCH';
	$signal_name[29] = 'INFO';
}

sub find_signal
{
	my $number =  shift;

	if (@signal_name == 0) {
		fillup_names();
	}

	return $signal_name[$number] || $number;
}

sub child_error
{
	my $error = shift // $?;

	my $extra = "";

	if ($error & 128) {
		$extra = " (core dumped)";
	}
	if ($error & 127) {
		return "killed by signal ". find_signal($error & 127).$extra;
	} else {
		return "exit(". ($error >> 8) . ")$extra";
	}
}

sub Copy
{
	require File::Copy;

	my $r = File::Copy::copy(@_);
	if (!$r) {
		print "copy(", join(',', @_),") failed: $!\n";
	}
	return $r;
}

sub Unlink
{
	my $verbose = shift;
	my $r = unlink @_;
	if ($r != @_) {
		print "rm @_ failed: removed only $r targets, $!\n";
	} elsif ($verbose) {
		print "rm @_\n";
	}
	return $r;
}

sub dienow
{
	my ($error, $handler) = @_;
	if ($error) {
		if ($error =~ m/^(.*?)(?:\s+at\s+(.*)\s+line\s+(\d+)\.?)?$/o) {
			local $_ = $1;
			$FileName = $2;
			$Line = $3;
			$FullMessage = $error;

			$handler->exec($error, $1, $2, $3);
		} else {
			die "Fatal error: can't parse $error";
		}
	}
}

sub try(&@)
{
	my ($try, $catch) = @_;
	eval { &$try };
	dienow($@, $catch);
}

sub throw
{
	croak @_;

}

sub rethrow
{
	my $e = shift;
	die $e if $e;
}

sub catch(&)
{
		bless $_[0], "OpenBSD::Error::catch";
}

sub catchall(&)
{
	bless $_[0], "OpenBSD::Error::catch";
}

sub rmtree
{
	my $class = shift;
	require File::Path;
	require Cwd;

	# XXX make sure we live somewhere
	Cwd::getcwd() || chdir('/');

	File::Path::rmtree(@_);
}

package OpenBSD::Error::catch;
sub exec
{
	my ($self, $full, $e) = @_;
	&$self;
}

1;
