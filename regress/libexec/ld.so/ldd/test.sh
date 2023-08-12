#!/bin/sh -ex
# $OpenBSD$

res=0

test() {
  if eval "$@"; then
    echo "passed"
  else
    echo "FAILED"
    res=1
  fi
}

test "ldd empty 2>&1 | grep 'incomplete ELF header'"
test "ldd short 2>&1 | grep 'incomplete program header'"

exit $res
