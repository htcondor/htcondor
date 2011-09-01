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

if (HAVE_LIBDL)
	set (DL_FOUND -ldl)
endif (HAVE_LIBDL)

##
#####################################################################

#####################################################################
# treat like an external for others to use
condor_pre_external( CLASSADS classads-7.7.2 "lib;include" "alwaysbuild")
#####################################################################

if (NOT WINDOWS)

  condor_selective_glob("attrrefs.*;classad.*;collection.*;collectionBase.*;debug.*;exprList.*;exprTree.*;fnCall.*;indexfile.*;lexer.*;lexerSource.*;literals.*;matchClassad.*;operators.*;query.*;sink.*;source.*;transaction.*;util.*;value.*;view.*;xmlLexer.*;xmlSink.*;xmlSource.*;cclassad.*;common.*" ClassadSrcs)
  add_library( classads STATIC ${ClassadSrcs} )    # the one which all of condor depends upon
  set_target_properties( classads PROPERTIES OUTPUT_NAME classad )

  if (LINUX)  
  	add_library( classad SHARED ${ClassadSrcs} )   # for distribution at this point may swap to depend at a future date.
	set_target_properties( classad PROPERTIES VERSION 2.0.0 SOVERSION 2 )
	condor_set_link_libs( classad "${PCRE_FOUND};${DL_FOUND}" )
	install( TARGETS classad DESTINATION ${C_LIB_PUBLIC} )
  endif()

  if ( LINUX )
	# set external reference to the shared library for linux
	set (CLASSADS_FOUND classad)
  else()
	# set external reference to the static library for non-linux
	set (CLASSADS_FOUND classads)
  endif()

  ###### Test executables
  condor_exe_test( classad_unit_tester "classad_unit_tester.cpp" "${CLASSADS_FOUND};${PCRE_FOUND};${DL_FOUND}")
  condor_exe( classad_functional_tester "classad_functional_tester.cpp" ${C_BIN} "${CLASSADS_FOUND};${PCRE_FOUND};${DL_FOUND}" OFF)
  condor_exe( classad_version "classad_version.cpp" ${C_BIN} "${CLASSADS_FOUND};${PCRE_FOUND};${DL_FOUND}" OFF)
  
  ###### Install elements with our distribution
  install( TARGETS classads DESTINATION ${C_LIB_PUBLIC} )
  install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/classad DESTINATION ${C_INCLUDE_PUBLIC} )

else()	
	# windows specific configuration.
	condor_selective_glob("attrrefs.cpp;common.cpp;collection*;fnCall.cpp;expr*;indexfile*;lexer*;literals.cpp;matchClassad.cpp;classad.cpp;debug.cpp;operators.cpp;util.cpp;value.cpp;query.cpp;sink.cpp;source.cpp;transaction.cpp;view.cpp;xml*" ClassadSrcs)
	add_library( classads STATIC ${ClassadSrcs} )
	set (CLASSADS_FOUND classads)

endif()

condor_post_external( classads include OFF )

# standard output message used to slog users.  
message (STATUS "configured (CLASSADS_FOUND= ${CLASSADS_FOUND})")

# finally add dependencies if there are any
if (NOT PROPER)
  
  if(PCRE_REF)
	add_dependencies( ${CLASSADS_FOUND} ${PCRE_REF} )
  endif()

  if (NOT WINDOWS)
	############################################################################## 
	# the only reason this exists is b/c our externals depend on it
	# (blahp & creame)
	file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/classad DESTINATION ${CLASSADS_INSTALL_LOC}/include )
	set_target_properties( classads
		PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib 
		LIBRARY_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib
		RUNTIME_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib
		OUTPUT_NAME classad)
	if ( LINUX )
		set_target_properties( classad
			PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib 
			LIBRARY_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib
			RUNTIME_OUTPUT_DIRECTORY ${CLASSADS_INSTALL_LOC}/lib)
	endif()

	if (PCRE_REF)
		add_custom_command (OUTPUT ${CLASSADS_INSTALL_LOC}/lib/libpcre.a
                    COMMAND cp
                    ARGS -f ${PCRE_FOUND} ${CLASSADS_INSTALL_LOC}/lib)

		add_custom_target( pcre_coinstall ALL DEPENDS ${CLASSADS_INSTALL_LOC}/lib/libpcre.a )

		add_dependencies( pcre_coinstall ${PCRE_REF} )

		add_dependencies( ${CLASSADS_FOUND} pcre_coinstall )
	endif()

  endif()

endif(NOT PROPER)

