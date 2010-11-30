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
#     	g++ -fno-implicit-templates -g -DCLASSAD_DISTRIBUTION -I$(PERL_ARCHLIB)/CORE -c classad_wrap.cpp
#     	g++ -shared $(LIB_OBJ) classad_wrap.o -o ClassAd.so
#     	mkdir -p perl/lib/ClassAd
#     	mv ClassAd.so ClassAd.pm perl/lib/















# This assists when working on a UW CS CSL machine, but can't be
# turned on all the time or it will tie this file to CSL machines.
# Instead the CSL's autotool install should be updated to handle the
# non-standard location of libtool.
#ACLOCAL_AMFLAGS = -I /s/libtool/share/aclocal

if ENABLE_EXPLICIT_TEMPLATES
  _libclassad_la_SOURCES = instantiations.cpp
endif

NAMESPACE = -DWANT_CLASSAD_NAMESPACE=1

if ENABLE_NAMESPACE
_libclassad_ns.la=libclassad_ns.la
endif
   _libclassad.la=libclassad.la $(_libclassad_ns.la)
lib_LTLIBRARIES = $(_libclassad.la)

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
	classad/source.h

if ENABLE_NAMESPACE
_classad_functional_tester_ns=classad_functional_tester_ns
endif
   _classad_functional_tester=classad_functional_tester $(_classad_functional_tester_ns)
if ENABLE_NAMESPACE
_cxi_ns=cxi_ns
endif
   _cxi=cxi $(_cxi_ns)
if ENABLE_NAMESPACE
_classad_version_ns=classad_version_ns
endif
   _classad_version=classad_version $(_classad_version_ns)
bin_PROGRAMS = 								\
	$(_classad_functional_tester)					\
	$(_cxi) 							\
	$(_classad_version)

if ENABLE_NAMESPACE
_classad_functional_tester_ns_s=classad_functional_tester_ns_s
endif
   _classad_functional_tester_s=classad_functional_tester_s $(_classad_functional_tester_ns_s)
if ENABLE_NAMESPACE
_classad_unit_tester_ns=classad_unit_tester_ns
endif
   _classad_unit_tester=classad_unit_tester $(_classad_unit_tester_ns)
if ENABLE_NAMESPACE
_test_xml_ns=test_xml_ns
endif
   _test_xml=test_xml $(_test_xml_ns)
if ENABLE_NAMESPACE
_sample_ns=sample_ns
endif
   _sample=sample $(_sample_ns)
if ENABLE_NAMESPACE
_extra_tests_ns=extra_tests_ns
endif
   _extra_tests=extra_tests $(_extra_tests_ns)
TESTS =									\
	$(_classad_functional_tester_s)					\
	$(_classad_unit_tester)						\
	$(_test_xml)							\
	$(_sample)							\
	$(_extra_tests)
# This must be set because we are patching libtool to remove rpaths
TESTS_ENVIRONMENT=LD_LIBRARY_PATH=.libs


check_PROGRAMS =							\
	$(_classad_functional_tester)					\
	$(TESTS)

libclassad_la_SOURCES = \
	attrrefs.cpp new_classad.cpp common.cpp collection.cpp collectionBase.cpp new_classad_debug.cpp	\
	exprList.cpp exprTree.cpp fnCall.cpp indexfile.cpp lexer.cpp		\
	lexerSource.cpp literals.cpp matchClassad.cpp new_classad_operators.cpp query.cpp	\
	sink.cpp source.cpp transaction.cpp new_classad_util.cpp new_classad_value.cpp view.cpp xmlLexer.cpp	\
	xmlSink.cpp xmlSource.cpp cclassad.cpp common.cpp $(_libclassad_la_SOURCES)

libclassad_la_LDFLAGS = -version-info 1:0:1
libclassad_ns_la_LDFLAGS = -version-info 1:0:1

if ENABLE_NAMESPACE
libclassad_ns_la_SOURCES = $(libclassad_la_SOURCES)
   libclassad_ns_la_CXXFLAGS = $(NAMESPACE)
endif

# BEGIN MF_DEFINE_PROGRAM(cxi,,)
cxi_SOURCES = cxi.cpp 
cxi_CXXFLAGS = 
cxi_LDADD = libclassad.la
if ENABLE_NAMESPACE
cxi_ns_SOURCES = $(cxi_SOURCES)
   cxi_ns_CXXFLAGS = $(NAMESPACE) 
   cxi_ns_LDADD = libclassad_ns.la
endif
# END MF_DEFINE_PROGRAM(cxi,,)

# BEGIN MF_DEFINE_PROGRAM(classad_version,,)
classad_version_SOURCES = classad_version.cpp 
classad_version_CXXFLAGS = 
classad_version_LDADD = libclassad.la
if ENABLE_NAMESPACE
classad_version_ns_SOURCES = $(classad_version_SOURCES)
   classad_version_ns_CXXFLAGS = $(NAMESPACE) 
   classad_version_ns_LDADD = libclassad_ns.la
endif
# END MF_DEFINE_PROGRAM(classad_version,,)

if ENABLE_EXPLICIT_TEMPLATES
  _classad_functional_tester_SOURCES = test_instantiations.cpp
endif

# BEGIN MF_DEFINE_PROGRAM(classad_functional_tester,$(_classad_functional_tester_SOURCES),)
classad_functional_tester_SOURCES = classad_functional_tester.cpp $(_classad_functional_tester_SOURCES)
classad_functional_tester_CXXFLAGS = 
classad_functional_tester_LDADD = libclassad.la
if ENABLE_NAMESPACE
classad_functional_tester_ns_SOURCES = $(classad_functional_tester_SOURCES)
   classad_functional_tester_ns_CXXFLAGS = $(NAMESPACE) 
   classad_functional_tester_ns_LDADD = libclassad_ns.la
endif
# END MF_DEFINE_PROGRAM(classad_functional_tester,$(_classad_functional_tester_SOURCES),)

# BEGIN MF_DEFINE_PROGRAM(classad_unit_tester,,)
classad_unit_tester_SOURCES = classad_unit_tester.cpp 
classad_unit_tester_CXXFLAGS = 
classad_unit_tester_LDADD = libclassad.la
if ENABLE_NAMESPACE
classad_unit_tester_ns_SOURCES = $(classad_unit_tester_SOURCES)
   classad_unit_tester_ns_CXXFLAGS = $(NAMESPACE) 
   classad_unit_tester_ns_LDADD = libclassad_ns.la
endif
# END MF_DEFINE_PROGRAM(classad_unit_tester,,)

# BEGIN MF_DEFINE_PROGRAM(test_xml,,)
test_xml_SOURCES = test_xml.cpp 
test_xml_CXXFLAGS = 
test_xml_LDADD = libclassad.la
if ENABLE_NAMESPACE
test_xml_ns_SOURCES = $(test_xml_SOURCES)
   test_xml_ns_CXXFLAGS = $(NAMESPACE) 
   test_xml_ns_LDADD = libclassad_ns.la
endif
# END MF_DEFINE_PROGRAM(test_xml,,)

# BEGIN MF_DEFINE_PROGRAM(sample,,)
sample_SOURCES = sample.cpp 
sample_CXXFLAGS = 
sample_LDADD = libclassad.la
if ENABLE_NAMESPACE
sample_ns_SOURCES = $(sample_SOURCES)
   sample_ns_CXXFLAGS = $(NAMESPACE) 
   sample_ns_LDADD = libclassad_ns.la
endif
# END MF_DEFINE_PROGRAM(sample,,)

# BEGIN MF_DEFINE_PROGRAM(extra_tests,,-DTEST_LIBNAME='"shared.so"')
extra_tests_SOURCES = extra_tests.cpp 
extra_tests_CXXFLAGS = -DTEST_LIBNAME='"shared.so"'
extra_tests_LDADD = libclassad.la
if ENABLE_NAMESPACE
extra_tests_ns_SOURCES = $(extra_tests_SOURCES)
   extra_tests_ns_CXXFLAGS = $(NAMESPACE) -DTEST_LIBNAME='"shared_ns.so"'
   extra_tests_ns_LDADD = libclassad_ns.la
endif
# END MF_DEFINE_PROGRAM(extra_tests,,-DTEST_LIBNAME='"shared.so"')

# The following method of building the libraries for testing dlopen
# does not work on all platforms, so it is disabled by default.
# On some platforms, when configured with --disable-shared, it is
# necessary to also configure --with-pic in order for the dlopen test
# to work.
if ENABLE_DLOPEN_CHECK
extra_tests_DEPENDENCIES = shared.so
if ENABLE_NAMESPACE
extra_tests_ns_DEPENDENCIES = shared_ns.so
endif

shared.so:
	$(CXXCOMPILE) -fPIC -shared -o shared.so shared.cpp -L.libs -lclassad

shared_ns.so:
	$(CXXCOMPILE) $(NAMESPACE) -fPIC -shared -o shared_ns.so shared.cpp -L.libs -lclassad_ns
endif

Makefile.am: Makefile.am.m4
	rm -f Makefile.am
	m4 Makefile.am.m4 > Makefile.am
	chmod a-w Makefile.am

classad_functional_tester_s: classad_functional_tester
	echo "#!/bin/sh" > classad_functional_tester_s
	echo "exec ./classad_functional_tester functional_tests.txt" >> classad_functional_tester_s
	chmod a+x classad_functional_tester_s

classad_functional_tester_ns_s: classad_functional_tester_ns
	echo "#!/bin/sh" > classad_functional_tester_ns_s
	echo "exec ./classad_functional_tester_ns functional_tests.txt" >> classad_functional_tester_ns_s
	chmod a+x classad_functional_tester_ns_s

CLEANFILES = shared.so shared_ns.so classad_functional_tester_s classad_functional_tester_ns_s 
