# ex:ts=8 sw=4:
# $OpenBSD: State.pm,v 1.3 2010/06/15 08:26:39 espie Exp $
#
# Copyright (c) 2007-2010 Marc Espie <espie@openbsd.org>
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
#

use strict;
use warnings;

# common routines to everything state.
# in particular, provides "singleton-like" access to UI.
package OpenBSD::State;
use Carp;

sub new
{
	my ($class, $cmd) = @_;
	my $o = bless {cmd => $cmd}, $class;
	$o->init(@_);
	return $o;
}

sub init
{
}

sub usage_is
{
	my ($self, @usage) = @_;
	$self->{usage} = \@usage;
}

sub usage
{
	my $self = shift;
	my $code = 0;
	if (@_) {
		print STDERR "$self->{cmd}: ", $self->f(@_), "\n";
		$code = 1;
	}
	print STDERR "Usage: $self->{cmd} ", shift(@{$self->{usage}}), "\n";
	for my $l (@{$self->{usage}}) {
		print STDERR "       $l\n";
	}
	exit($code);
}

sub f
{
	my $self = shift;
	if (@_ == 0) {
		return undef;
	}
	my $_ = shift;
	# make it so that #0 is #
	unshift(@_, '#');
	s/\#(\d+)/$_[$1]/ge;
	return $_;
}

sub _fatal
{
	my $self = shift;
	# implementation note: to print "fatal errors" elsewhere,
	# the way is to eval { croak @_}; and decide what to do with $@.
	croak "Fatal error: ", @_, "\n";
}

sub fatal
{
	my $self = shift;
	$self->_fatal($self->f(@_));
}

sub _print
{
	my $self = shift;
	print @_;
}

sub _errprint
{
	my $self = shift;
	print STDERR @_;
}

sub print
{
	my $self = shift;
	$self->_print($self->f(@_));
}

sub say
{
	my $self = shift;
	$self->_print($self->f(@_), "\n");
}

sub errprint
{
	my $self = shift;
	$self->_errprint($self->f(@_));
}

sub errsay
{
	my $self = shift;
	$self->_errprint($self->f(@_), "\n");
}

sub do_options
{
	my ($state, $sub) = @_;
	require OpenBSD::Error;
	# this could be nicer...
	eval { &$sub; };
	OpenBSD::Error::dienow($@, 
	    bless sub { $state->usage("#1", $_)}, "OpenBSD::Error::catchall");
}

my @signal_name = ();
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
		$signal_name[eval "&POSIX::$sym()"] = $1;
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
	my $self = shift;
	my $error = $?;

	my $extra = "";

	if ($error & 128) {
		$extra = $self->f(" (core dumped)");
	}
	if ($error & 127) {
		return $self->f("killed by signal #1#2", 
		    find_signal($error & 127), $extra);
	} else {
		return $self->f("exit(#1)#2", ($error >> 8), $extra);
	}
}

sub system
{
	my $self = shift;
	my $r = CORE::system(@_);
	if ($r != 0) {
		$self->say("system(#1) failed: #2", 
		    join(", ", @_), $self->child_error);
	}
	return $r;
}

sub copy_file
{
	my $self = shift;
	require File::Copy;

	my $r = File::Copy::copy(@_);
	if (!$r) {
		$self->say("copy(#1) failed: #2", join(',', @_), $!);
	}
	return $r;
}

sub unlink
{
	my $self = shift;
	my $verbose = shift;
	my $r = unlink @_;
	if ($r != @_) {
		$self->say("rm #1 failed: removed only #2 targets, #3",
		    join(' ', @_), $r, $1);
	} elsif ($verbose) {
		$self->say("rm #1", join(' ', @_));
	}
	return $r;
}

sub copy
{
	my $self = shift;
	require File::Copy;

	my $r = File::Copy::copy(@_);
	if (!$r) {
		$self->say("copy(#1) failed: #2", join(',', @_), $!);
	}
	return $r;
}

1;
