ACLOCAL_AMFLAGS = -I m4
AM_CXXFLAGS = ${my_CPPFLAGS} -Wall -Werror -pedantic -fPIC -I$(top_srcdir)/include
AM_CFLAGS = ${my_CFLAGS} -Wall -Werror -pedantic -std=c99 -fPIC -I$(top_srcdir)/include

bin_PROGRAMS = jsonnet
jsonnet_SOURCES = cmd/jsonnet.cpp
jsonnet_LDADD = libjsonnet.la $(LDADD)

lib_LTLIBRARIES = libjsonnet.la
libjsonnet_la_SOURCES = core/desugaring.cpp core/lexer.cpp core/libjsonnet.cpp \
                        core/parser.cpp core/static_analysis.cpp core/vm.cpp
nodist_libjsonnet_la_SOURCES = core/std.jsonnet.h
libjsonnet_la_SOURCES += core/ast.h core/desugaring.h core/lexer.h \
                         core/parser.h core/state.h core/static_analysis.h \
                         core/static_error.h core/vm.h
libjsonnet_HEADERS =$(top_srcdir)/include/libjsonnet.h
libjsonnetdir = $(includedir)

BUILT_SOURCES = core/std.jsonnet.h
CLEANFILES = core/std.jsonnet.h
core/std.jsonnet.h: stdlib/std.jsonnet
	(($(OD) -v -Anone -t u1 $< \
		| $(TR) " " "\n" \
		| $(GREP) -v "^$$" \
		| $(TR) "\n" "," ) && echo "0") > $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libjsonnet.pc
