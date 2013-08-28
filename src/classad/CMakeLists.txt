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
 
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/classad/classad_stl.h.in ${CMAKE_CURRENT_BINARY_DIR}/classad/classad_stl.h.tmp)
# only update config.h if it is necessary b/c it causes massive rebuilding.
exec_program ( ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/classad/classad_stl.h.tmp ${CMAKE_CURRENT_BINARY_DIR}/classad/classad_stl.h )
 
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${BOOST_INCLUDE})

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

if (HAVE_LIBDL)
	set (DL_FOUND -ldl)
endif (HAVE_LIBDL)

dprint("classad_BOOST_INCLUDE=${BOOST_INCLUDE}")

if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
	add_definitions( -DCLASSAD_VERSION="${PACKAGE_VERSION}"
		-DCLASSAD_VERSION_MAJOR=${CMAKE_MATCH_1}
		-DCLASSAD_VERSION_MINOR=${CMAKE_MATCH_2}
		-DCLASSAD_VERSION_PATCH=${CMAKE_MATCH_3} )
endif()

file( GLOB Headers classad/*.h )
set( Headers "${Headers};${CMAKE_CURRENT_BINARY_DIR}/classad/classad_stl.h" )

##
#####################################################################

#####################################################################
# treat like an external for others to use
condor_pre_external( CLASSADS classads-${PACKAGE_VERSION} "lib;include" "alwaysbuild")
#####################################################################

if (NOT WINDOWS)

  condor_selective_glob("attrrefs.*;classad.*;classadCache.*;collection.*;collectionBase.*;debug.*;exprList.*;exprTree.*;fnCall.*;indexfile.*;lexer.*;lexerSource.*;literals.*;matchClassad.*;operators.*;query.*;sink.*;source.*;transaction.*;util.*;value.*;view.*;xmlLexer.*;xmlSink.*;xmlSource.*;cclassad.*;common.*" ClassadSrcs)
  add_library( classads STATIC ${ClassadSrcs} )    # the one which all of condor depends upon
  set_target_properties( classads PROPERTIES OUTPUT_NAME classad )

  if (LINUX OR DARWIN)  
  	add_library( classad SHARED ${ClassadSrcs} )   # for distribution at this point may swap to depend at a future date.
	set_target_properties( classad PROPERTIES VERSION ${PACKAGE_VERSION} SOVERSION 6 )
	condor_set_link_libs( classad "${PCRE_FOUND};${DL_FOUND}" )
	install( TARGETS classad DESTINATION ${C_LIB_PUBLIC} )
  endif()
  if ( DARWIN )
	add_custom_command( TARGET classad
		POST_BUILD
		WORKING_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib
		COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs libclassad.${PACKAGE_VERSION}.dylib )
  endif()

  if ( LINUX OR DARWIN )
	# set external reference to the shared library for linux
	set (CLASSADS_FOUND classad)
	set (CLASSADS_FOUND_STATIC classads)
  else()
	# set external reference to the static library for non-linux
	set (CLASSADS_FOUND classads)
	set (CLASSADS_FOUND_STATIC classads)
  endif()

  ##condor_exe( classad_functional_tester "classad_functional_tester.cpp" ${C_BIN} "${CLASSADS_FOUND};${PCRE_FOUND};${DL_FOUND}" OFF)
  add_executable( classad_functional_tester "classad_functional_tester.cpp" )
  condor_set_link_libs( classad_functional_tester "${CLASSADS_FOUND};${PCRE_FOUND};${DL_FOUND}")
  install (TARGETS classad_functional_tester DESTINATION ${C_BIN} )
  
  ##condor_exe( classad_version "classad_version.cpp" ${C_BIN} "${CLASSADS_FOUND};${PCRE_FOUND};${DL_FOUND}" OFF)
  add_executable( classad_version "classad_version.cpp" )
  condor_set_link_libs( classad_version "${CLASSADS_FOUND};${PCRE_FOUND};${DL_FOUND}")
  install (TARGETS classad_version DESTINATION ${C_BIN} )
  
  ###### Install elements with our distribution
  install( TARGETS classads DESTINATION ${C_LIB_PUBLIC} )
  install( FILES ${Headers} DESTINATION ${C_INCLUDE_PUBLIC}/classad )

else()	
	# windows specific configuration.
	condor_selective_glob("attrrefs.cpp;common.cpp;collection*;classadCache.*;fnCall.cpp;expr*;indexfile*;lexer*;literals.cpp;matchClassad.cpp;classad.cpp;debug.cpp;operators.cpp;util.cpp;value.cpp;query.cpp;sink.cpp;source.cpp;transaction.cpp;view.cpp;xml*" ClassadSrcs)
	add_library( classads STATIC ${ClassadSrcs} )
	set (CLASSADS_FOUND classads)
	set (CLASSADS_FOUND_STATIC classads)

endif()

set( CLASSADS_FOUND ${CLASSADS_FOUND} PARENT_SCOPE )
set( CLASSADS_FOUND_STATIC ${CLASSADS_FOUND_STATIC} PARENT_SCOPE )


condor_post_external( classads include OFF )

# standard output message used to slog users.  
message (STATUS "configured (CLASSADS_FOUND= ${CLASSADS_FOUND})")

# finally add dependencies if there are any
if (NOT PROPER)
  
  if(PCRE_REF)
	add_dependencies( classads ${PCRE_REF} )
  endif()

  # On Mac, we build our own pcre, unlike linux, so need to 
  # have a dependency here so we build pcre before classads
  if ( DARWIN AND PCRE_REF)
	add_dependencies( classad ${PCRE_REF} )
  endif()

  if (BOOST_REF)
	  add_dependencies( classads ${BOOST_REF} )
	  if ( LINUX OR DARWIN )
		  add_dependencies( classad ${BOOST_REF} )
	  endif()
  endif()

  if (NOT WINDOWS)
	############################################################################## 
	# the only reason this exists is b/c our externals depend on it
	# (blahp & creame)
	file( COPY ${Headers} DESTINATION ${CLASSADS_INSTALL_LOC}/include/classad )
	set_target_properties( classads
		PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib 
		LIBRARY_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib
		RUNTIME_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib
		OUTPUT_NAME classad)
	if ( LINUX OR DARWIN )
		set_target_properties( classad
			PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib 
			LIBRARY_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib
			RUNTIME_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib)
	endif()

  endif()

endif(NOT PROPER)

###### Test executables
condor_unit_test( classad_unit_tester "classad_unit_tester.cpp" "${CLASSADS_FOUND};${PCRE_FOUND};${DL_FOUND}" OFF)
add_subdirectory(tests)
