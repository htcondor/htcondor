##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

###
### WARNING: Only edit the version of this file that ends with .m4!
### All other versions will be overwritten whenever the .m4 version is
### changed.
###

# NOTES:
#  * += does not work well with conditionals in Automake 1.6.3, thus _'s
#  ** The if's should happen after initial assignments and use +=
#  ** Also, modifications to CPPFLAGS and LDFLAGS are now in configure.ac
#  * If blah_SOURCES exists blah be used somewhere, thus if conditionals
#    around some _SOURCES etc
#  * We no longer build the Perl module as no one currently knows how
#    to use or test it. The knowledge of how to build it is below:
#   
#     PERL_ARCHLIB = $(shell perl -MConfig -e 'print "$$Config{archlib}\n"')
#     .PHONY: perl
#     perl: all perl/lib/$(PERL_ARCHNAME)/ClassAd.so
#
#     perl/lib/$(PERL_ARCHNAME)/ClassAd.so: $(LIB_OBJ) ClassAd.swg
#   	swig -v -Wall -c++ -perl -proxy -o classad_wrap.cpp ClassAd.swg
#     	g++ -fno-implicit-templates -g -I$(PERL_ARCHLIB)/CORE -c classad_wrap.cpp
#     	g++ -shared $(LIB_OBJ) classad_wrap.o -o ClassAd.so
#     	mkdir -p perl/lib/ClassAd
#     	mv ClassAd.so ClassAd.pm perl/lib/


dnl
dnl BEGIN M4 CODE
dnl 


dnl Use []s for quotes over `', ` is a pain to type
changequote(`[', `]')dnl


dnl Define all the flavors of a program (given: $1). Optionally, add
dnl extra source files (given: $2), if $1.cpp isn't enough. Also, it 
dnl became useful to pass in the libname for extra_tests, so $3 is
dnl added to CXXFLAGS
define([MF_DEFINE_PROGRAM],
  [# BEGIN MF_DEFINE_PROGRAM($1,$2,$3)
$1_SOURCES = $1.cpp $2
$1_CXXFLAGS = $3
$1_LDADD = libclassad.la
])
# END MF_DEFINE_PROGRAM($1,$2,$3)])

dnl
dnl END M4 CORE
dnl

# This assists when working on a UW CS CSL machine, but can't be
# turned on all the time or it will tie this file to CSL machines.
# Instead the CSL's autotool install should be updated to handle the
# non-standard location of libtool.
#ACLOCAL_AMFLAGS = -I /s/libtool/share/aclocal

if ENABLE_EXPLICIT_TEMPLATES
  _libclassad_la_SOURCES = instantiations.cpp
endif

lib_LTLIBRARIES = libclassad.la

nobase_include_HEADERS =						\
	classad/attrrefs.h classad/collectionBase.h classad/lexer.h	\
	classad/transaction.h classad/cclassad.h classad/collection.h	\
	classad/lexerSource.h classad/util.h				\
	classad/classad_distribution.h classad/common.h			\
	classad/literals.h classad/value.h classad/classadErrno.h	\
	classad/debug.h classad/matchClassad.h classad/view.h		\
	classad/exprList.h						\
	classad/operators.h classad/xmlLexer.h classad/classad.h	\
	classad/exprTree.h classad/query.h classad/xmlSink.h		\
	classad/classadItor.h classad/fnCall.h classad/sink.h		\
	classad/xmlSource.h classad/classad_stl.h classad/indexfile.h	\
	classad/source.h \
	classad/classadCache.h

bin_PROGRAMS = 								\
	classad_functional_tester					\
	cxi 								\
	classad_version

TESTS =									\
	classad_functional_tester_s					\
	classad_unit_tester						\
	test_xml							\
	sample								\
	extra_tests
# This must be set because we are patching libtool to remove rpaths
TESTS_ENVIRONMENT=LD_LIBRARY_PATH=.libs


check_PROGRAMS =							\
	classad_functional_tester					\
	$(TESTS)

libclassad_la_SOURCES = \
	attrrefs.cpp classad.cpp classadCache.cpp common.cpp collection.cpp collectionBase.cpp debug.cpp	\
	exprList.cpp exprTree.cpp fnCall.cpp indexfile.cpp lexer.cpp		\
	lexerSource.cpp literals.cpp matchClassad.cpp operators.cpp query.cpp	\
	sink.cpp source.cpp transaction.cpp util.cpp value.cpp view.cpp xmlLexer.cpp	\
	xmlSink.cpp xmlSource.cpp cclassad.cpp $(_libclassad_la_SOURCES)

libclassad_la_LDFLAGS = -version-info @VERSION_INFO@

MF_DEFINE_PROGRAM([cxi])

MF_DEFINE_PROGRAM([classad_version])

if ENABLE_EXPLICIT_TEMPLATES
  _classad_functional_tester_SOURCES = test_instantiations.cpp
endif

MF_DEFINE_PROGRAM([classad_functional_tester],
  [$(_classad_functional_tester_SOURCES)])

MF_DEFINE_PROGRAM([classad_unit_tester])

MF_DEFINE_PROGRAM([test_xml])

MF_DEFINE_PROGRAM([sample])

MF_DEFINE_PROGRAM([extra_tests], [],
  [-DTEST_LIBNAME='"shared.so"'])

# The following method of building the libraries for testing dlopen
# does not work on all platforms, so it is disabled by default.
# On some platforms, when configured with --disable-shared, it is
# necessary to also configure --with-pic in order for the dlopen test
# to work.
if ENABLE_DLOPEN_CHECK
extra_tests_DEPENDENCIES = shared.so

shared.so:
	$(CXXCOMPILE) -fPIC -shared -o shared.so shared.cpp -L.libs -lclassad
endif

Makefile.am: Makefile.am.m4
	rm -f Makefile.am
	m4 Makefile.am.m4 > Makefile.am
	chmod a-w Makefile.am

classad_functional_tester_s: classad_functional_tester
	echo "#!/bin/sh" > classad_functional_tester_s
	echo "exec ./classad_functional_tester functional_tests.txt" >> classad_functional_tester_s
	chmod a+x classad_functional_tester_s

CLEANFILES = shared.so shared_ns.so classad_functional_tester_s
