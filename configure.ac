AC_PREREQ([2.61])
AC_INIT([jsonnet], [0.8.5])
AC_CANONICAL_HOST
AC_CANONICAL_BUILD
AC_CANONICAL_TARGET
AC_DISABLE_STATIC

AC_LANG([C++])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AM_MAINTAINER_MODE([enable])

AC_PROG_CC
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])

#AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
  Makefile
])
AC_CHECK_HEADERS([alloca.h features.h libio.h stdc-predef.h stdio.h stdlib.h wchar.h])
AM_MISSING_PROG(OD, od)
AM_MISSING_PROG(GREP, grep)
AM_MISSING_PROG(TR, tr)
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

LT_PREREQ([2.2])
LT_INIT
LT_LANG([C++])
LT_OUTPUT

AC_OUTPUT
