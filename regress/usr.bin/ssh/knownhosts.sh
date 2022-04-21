#	$OpenBSD: proxy-connect.sh,v 1.12 2020/01/23 11:19:12 dtucker Exp $
#	Placed in the Public Domain.

tid="known hosts"

opts="-F $OBJ/ssh_proxy"

trace "test initial connection"
${SSH} $opts somehost true || fail "initial connection"

trace "learn hashed known host"
>$OBJ/known_hosts
${SSH} -ohashknownhosts=yes -o stricthostkeychecking=no $opts somehost true \
   || fail "learn hashed known_hosts"

trace "test hashed known hosts"
${SSH} $opts somehost true || fail "reconnect with hashed known hosts"
