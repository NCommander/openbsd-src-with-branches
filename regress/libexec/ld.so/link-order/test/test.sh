# $OpenBSD$

msg=`LD_LIBRARY_PATH=lib $1`
case $2 in
%ERROR%)
  test $? -ne 0;;
*)
  test X"$msg" = X"$2"
esac
exit $?
