 ###############################################################
 # 
 # Copyright 2011 Red Hat, Inc. 
 # 
 # Licensed under the Apache License, Version 2.0 (the "License"); you 
 # may not use this file except in compliance with the License.  You may 
 # obtain a copy of the License at 
 # 
 #    http://www.apache.org/licenses/LICENSE-2.0 
 # 
 # Unless required by applicable law or agreed to in writing, software 
 # distributed under the License is distributed on an "AS IS" BASIS, 
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and 
 # limitations under the License. 
 # 
 ############################################################### 
 
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# disable PCRE build unless dep added to externals.
if(PCRE_FOUND)
	include_directories(${PCRE_INCLUDE})
	add_definitions(-DUSE_PCRE)
	if(HAVE_PCRE_H)
		add_definitions(-DHAVE_PCRE_H)
	elseif(HAVE_PCRE_PCRE_H)
		add_definitions(-DHAVE_PCRE_PCRE_H)
	endif()
else()
	add_definitions(-DUSE_POSIX_REGEX) 
endif(PCRE_FOUND)

if (HAVE_DLOPEN)
	add_definitions(-DHAVE_DLOPEN)
endif()

if (HAVE_DLFCN_H)
	add_definitions(-DHAVE_DLFCN_H)
endif()

if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
	add_definitions( -DCLASSAD_VERSION="${PACKAGE_VERSION}"
		-DCLASSAD_VERSION_MAJOR=${CMAKE_MATCH_1}
		-DCLASSAD_VERSION_MINOR=${CMAKE_MATCH_2}
		-DCLASSAD_VERSION_PATCH=${CMAKE_MATCH_3} )
endif()

set( Headers
classad/attrrefs.h
classad/cclassad.h
classad/classadCache.h
classad/classad_containers.h
classad/classad_distribution.h
classad/classadErrno.h
classad/classad.h
classad/classadItor.h
classad/collectionBase.h
classad/collection.h
classad/common.h
classad/debug.h
classad/exprList.h
classad/exprTree.h
classad/fnCall.h
classad/indexfile.h
classad/jsonSink.h
classad/jsonSource.h
classad/lexer.h
classad/lexerSource.h
classad/literals.h
classad/matchClassad.h
classad/natural_cmp.h
classad/operators.h
classad/query.h
classad/sink.h
classad/source.h
classad/transaction.h
classad/util.h
classad/value.h
classad/view.h
classad/xmlLexer.h
classad/xmlSink.h
classad/xmlSource.h
)

set (ClassadSrcs
attrrefs.cpp
classadCache.cpp
classad.cpp
collectionBase.cpp
collection.cpp
common.cpp
cxi.cpp
debug.cpp
exprList.cpp
exprTree.cpp
fnCall.cpp
indexfile.cpp
jsonSink.cpp
jsonSource.cpp
lexer.cpp
lexerSource.cpp
literals.cpp
matchClassad.cpp
natural_cmp.cpp
operators.cpp
query.cpp
shared.cpp
sink.cpp
source.cpp
transaction.cpp
util.cpp
value.cpp
view.cpp
xmlLexer.cpp
xmlSink.cpp
xmlSource.cpp
)

#####################################################################

add_library(classads_objects OBJECT ${ClassadSrcs})
set_property(TARGET classads_objects PROPERTY POSITION_INDEPENDENT_CODE 1)
add_library( classads STATIC $<TARGET_OBJECTS:classads_objects> )    # the one which all of condor depends upon
target_link_libraries( classads PUBLIC ${PCRE_FOUND};${CMAKE_DL_LIBS})
set_target_properties( classads PROPERTIES OUTPUT_NAME classad )

if (LINUX OR DARWIN)  
  add_library( classad SHARED $<TARGET_OBJECTS:classads_objects>)   # for distribution at this point may swap to depend at a future date.
  set_target_properties( classad PROPERTIES VERSION ${PACKAGE_VERSION} SOVERSION 15 )
  target_link_libraries( classad "${PCRE_FOUND};${CMAKE_DL_LIBS}" )
  install( TARGETS classad DESTINATION ${C_LIB_PUBLIC} )
endif()
if ( DARWIN )
  set_target_properties( classad PROPERTIES INSTALL_NAME_DIR ${CMAKE_CURRENT_BINARY_DIR}/lib )
  install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIB_PUBLIC}/libclassad.${PACKAGE_VERSION}.dylib)" )
endif()

if ( LINUX OR DARWIN )
  # set external reference to the shared library for linux
  set (CLASSADS_FOUND classad)
else()
  # set external reference to the static library for non-linux
  set (CLASSADS_FOUND classads)
endif()

add_executable( classad_functional_tester "classad_functional_tester.cpp" )
target_link_libraries( classad_functional_tester "${CLASSADS_FOUND};${PCRE_FOUND};${CMAKE_DL_LIBS}")
install (TARGETS classad_functional_tester DESTINATION ${C_BIN} )

add_executable( classad_version "classad_version.cpp" )
target_link_libraries( classad_version "${CLASSADS_FOUND};${PCRE_FOUND};${CMAKE_DL_LIBS}")
install (TARGETS classad_version DESTINATION ${C_BIN} )

###### Install elements with our distribution
install( TARGETS classads DESTINATION ${C_LIB_PUBLIC} )
install( FILES ${Headers} DESTINATION ${C_INCLUDE_PUBLIC}/classad )


set( CLASSADS_FOUND ${CLASSADS_FOUND} PARENT_SCOPE )

# standard output message used to slog users.  
message (STATUS "configured (CLASSADS_FOUND= ${CLASSADS_FOUND})")

# finally add dependencies if there are any
if (NOT PROPER)
  
  if(PCRE_REF)
	add_dependencies( classads ${PCRE_REF} )
  endif()

  if (NOT WINDOWS)
	############################################################################## 
	file( COPY ${Headers} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/include/classad )
	set_target_properties( classads
		PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib 
		LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
		OUTPUT_NAME classad)
	if ( LINUX OR DARWIN )
		set_target_properties( classad
			PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib 
			LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib
			RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
	endif()

  endif()

endif(NOT PROPER)

###### Test executables
condor_exe_test( classad_unit_tester "classad_unit_tester.cpp" "${CLASSADS_FOUND};${PCRE_FOUND};${CMAKE_DL_LIBS}" OFF)
condor_exe_test( _test_classad_parse "test_classad_parse.cpp" "${CLASSADS_FOUND};${PCRE_FOUND};${CMAKE_DL_LIBS}" OFF)
