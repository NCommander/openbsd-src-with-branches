divert(-1)
#
# Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#

divert(0)dnl
VERSIONID(`$Sendmail: msp.m4,v 1.29 2001/12/13 23:56:38 gshapiro Exp $')
divert(-1)
define(`ALIAS_FILE', `')
define(`confDELIVERY_MODE', `i')
define(`confUSE_MSP', `True')
define(`confFORWARD_PATH', `')
define(`confPRIVACY_FLAGS', `goaway,noetrn,restrictqrun')
define(`confDONT_PROBE_INTERFACES', `True')
dnl ---------------------------------------------
dnl run as this user (even if called by root)
ifdef(`confRUN_AS_USER',,`define(`confRUN_AS_USER', `smmsp')')
ifdef(`confTRUSTED_USER',,`define(`confTRUSTED_USER', confRUN_AS_USER)')
dnl ---------------------------------------------
dnl This queue directory must have the same group
dnl as sendmail and it must be group-writable.
dnl notice: do not test for QUEUE_DIR, it is set in some ostype/*.m4 files
ifdef(`MSP_QUEUE_DIR',
`define(`QUEUE_DIR', `MSP_QUEUE_DIR')',
`define(`QUEUE_DIR', `/var/spool/clientmqueue')')
define(`_MTA_HOST_', ifelse(defn(`_ARG_'), `', `localhost', `_ARG_'))
define(`_MSP_FQHN_',`dnl used to qualify addresses
ifdef(`MASQUERADE_NAME', ifdef(`_MASQUERADE_ENVELOPE_', `$M', `$j'), `$j')')
define(`RELAY_MAILER_ARGS', `TCP $h'ifelse(_ARG2_, `MSA', ` 587'))
dnl ---------------------------------------------
ifdef(`confPID_FILE', `dnl',
`define(`confPID_FILE', QUEUE_DIR`/sm-client.pid')')
define(`confQUEUE_FILE_MODE', `0660')dnl
ifdef(`STATUS_FILE',
`define(`_F_',
`define(`_b_', index(STATUS_FILE, `sendmail.st'))ifelse(_b_, `-1', `STATUS_FILE', `substr(STATUS_FILE, 0, _b_)sm-client.st')')
define(`STATUS_FILE', _F_)
undefine(`_b_') undefine(`_F_')',
`define(`STATUS_FILE', QUEUE_DIR`/sm-client.st')')
FEATURE(`no_default_msa')dnl
ifelse(defn(`_DPO_'), `',
`DAEMON_OPTIONS(`Name=NoMTA, Addr=127.0.0.1, M=E')dnl')
define(`_DEF_LOCAL_MAILER_FLAGS', `')dnl
define(`_DEF_LOCAL_SHELL_FLAGS', `')dnl
define(`LOCAL_MAILER_PATH', `[IPC]')dnl
define(`LOCAL_MAILER_FLAGS', `lmDFMuXkw5')dnl
define(`LOCAL_MAILER_ARGS', `TCP $h')dnl
define(`LOCAL_MAILER_DSN_DIAGNOSTIC_CODE', `SMTP')dnl
define(`LOCAL_SHELL_PATH', `[IPC]')dnl
define(`LOCAL_SHELL_FLAGS', `lmDFMuXk5')dnl
define(`LOCAL_SHELL_ARGS', `TCP $h')dnl
MODIFY_MAILER_FLAGS(`SMTP', `+k05')dnl
MODIFY_MAILER_FLAGS(`ESMTP', `+k05')dnl
MODIFY_MAILER_FLAGS(`DSMTP', `+k05')dnl
MODIFY_MAILER_FLAGS(`SMTP8', `+k05')dnl
MODIFY_MAILER_FLAGS(`RELAY', `+k0')dnl
MAILER(`local')dnl
MAILER(`smtp')dnl

LOCAL_CONFIG
D{MTAHost}_MTA_HOST_

LOCAL_RULESETS
SLocal_localaddr
R$+			$: $>ParseRecipient $1
R$* < @ $+ > $*		$#relay $@ ${MTAHost} $: $1 < @ $2 > $3
ifdef(`_USE_DECNET_SYNTAX_',
`# DECnet
R$+ :: $+		$#relay $@ ${MTAHost} $: $1 :: $2', `dnl')
R$*			$#relay $@ ${MTAHost} $: $1 < @ _MSP_FQHN_ >
