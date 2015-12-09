# Initialize so we can use += later
BUILT_SOURCES =
CLEANFILES =

ACLOCAL_AMFLAGS = -I m4
_includedir = $(top_srcdir)/include
AM_CXXFLAGS = ${my_CPPFLAGS} -Wall -Werror -pedantic -fPIC -I$(_includedir)
AM_CFLAGS = ${my_CFLAGS} -Wall -Werror -pedantic -std=c99 -fPIC -I$(_includedir)

bin_PROGRAMS = jsonnet
jsonnet_SOURCES = cmd/jsonnet.cpp
jsonnet_LDADD = libjsonnet.la $(LDADD)

lib_LTLIBRARIES = libjsonnet.la
_libjsonnet_sources = core/desugaring.cpp core/lexer.cpp core/libjsonnet.cpp \
                      core/parser.cpp core/static_analysis.cpp core/vm.cpp
_libjsonnet_headers = core/ast.h core/desugaring.h core/lexer.h \
                      core/parser.h core/state.h core/static_analysis.h \
                      core/static_error.h core/vm.h

libjsonnet_la_SOURCES = $(_libjsonnet_sources) $(_libjsonnet_headers)
nodist_libjsonnet_la_SOURCES = core/std.jsonnet.h

libjsonnet_HEADERS = $(top_srcdir)/include/libjsonnet.h
libjsonnetdir = $(includedir)

BUILT_SOURCES += core/std.jsonnet.h
CLEANFILES += core/std.jsonnet.h
# Encode standard library for embedding in C
core/std.jsonnet.h: stdlib/std.jsonnet
	(($(OD) -v -Anone -t u1 $< \
		| $(TR) " " "\n" \
		| $(GREP) -v "^$$" \
		| $(TR) "\n" "," ) && echo "0") > $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libjsonnet.pc

EMCXX ?= em++
EMCXXFLAGS = $(CXXFLAGS) $(AM_CXXFLAGS) --memory-init-file 0 -s DISABLE_EXCEPTION_CATCHING=0

JS_EXPORTED_FUNCTIONS = 'EXPORTED_FUNCTIONS=["_jsonnet_make", "_jsonnet_evaluate_snippet", "_jsonnet_realloc", "_jsonnet_destroy"]'

CLEANFILES += libjsonnet.js
# Javascript build of C binding
libjsonnet.js: $(libjsonnet_la_SOURCES) $(libjsonnet_HEADERS) core/std.jsonnet.h include/libjsonnet.h
	$(EMCXX) -s $(JS_EXPORTED_FUNCTIONS) $(EMCXXFLAGS) $(LDFLAGS) $(_libjsonnet_sources) -o $@

CLEANFILES += doc/libjsonnet.js
doc/libjsonnet.js: libjsonnet.js
	$(CP) $^ $@

SUBDIRS = doc
