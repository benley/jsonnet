# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([jsonnet], [0.8.5])

: ${CXXFLAGS="-g -O3"}
: ${CFLAGS="-g -O3"}
AC_LANG([C++])
AC_CONFIG_MACRO_DIR([m4])
AC_DISABLE_STATIC

AC_CANONICAL_HOST
AC_CANONICAL_BUILD
AC_CANONICAL_TARGET
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

LT_PREREQ([2.2])
LT_INIT([pic-only])
AC_SUBST([LIBTOOL_DEPS])
LT_LANG([C++])
AX_CXX_COMPILE_STDCXX([11], [noext], [mandatory])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AX_PYTHON
AX_PYTHON_MODULE([jinja2], true, python)
AM_MISSING_PROG(OD, od)
AM_MISSING_PROG(GREP, grep)
AM_MISSING_PROG(TR, tr)
AM_MISSING_PROG(CP, cp)

# Checks for header files.
AC_CHECK_HEADERS([alloca.h features.h libio.h stdc-predef.h stdio.h stdlib.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([floor pow sqrt strerror strtol strtoul])

AM_INIT_AUTOMAKE([-Wall -Werror -Wno-portability foreign subdir-objects])
AM_MAINTAINER_MODE([enable])

AC_CONFIG_FILES([
  Makefile
  libjsonnet.pc
  doc/Makefile
])

AC_OUTPUT
