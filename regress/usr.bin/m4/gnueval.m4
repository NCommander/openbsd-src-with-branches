dnl $OpenBSD$
dnl exponentiation is right associative
eval(`4**2**3')
dnl priority between unary operators and *
eval(`4**2*3')
eval(`-4**3')
