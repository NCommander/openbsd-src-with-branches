divert(-1)
#
# Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
# mail.local is not setuid on OpenBSD 2.4 and higher
#

divert(0)
VERSIONID(`@(#)bsd4.4.m4	8.10 (Berkeley) 10/6/1998')
ifdef(`HELP_FILE',, `define(`HELP_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/helpfile', `/usr/share/misc/sendmail.hf'))')dnl
ifdef(`STATUS_FILE',, `define(`STATUS_FILE', ifdef(`_USE_ETC_MAIL_', `/etc/mail/statistics', `/var/log/sendmail.st'))')dnl
ifdef(`LOCAL_MAILER_PATH',, `define(`LOCAL_MAILER_PATH', /usr/libexec/mail.local)')dnl
ifdef(`LOCAL_MAILER_FLAGS',, `define(`LOCAL_MAILER_FLAGS', `rmn9S')')dnl
ifdef(`UUCP_MAILER_ARGS',, `define(`UUCP_MAILER_ARGS', `uux - -r -z -a$g $h!rmail ($u)')')dnl
