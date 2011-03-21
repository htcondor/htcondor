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

add_definitions(-DCLASSAD_DISTRIBUTION -DWANT_CLASSAD_NAMESPACE=1 -DDISABLE_DLOPEN_CHECK )

# disable PCRE build unless dep added to externals.
if(PCRE_FOUND)
	add_definitions(-DUSE_PCRE -DHAVE_PCRE_H)
else()
	add_definitions(-DUSE_POSIX_REGEX) 
endif(PCRE_FOUND)

# in order to update configure
if (NOT WINDOWS)

  # because of the external dependencies on classads we need to be careful to make it look like a normal install
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/classad DESTINATION ${EXTERNAL_STAGE}/include )

  if (WANT_FULL_DEPLOYMENT)
	install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/classad DESTINATION ${C_INCLUDE} )
  endif()

  condor_selective_glob("attrrefs.*;classad.*;collection.*;collectionBase.*;debug.*;exprList.*;exprTree.*;fnCall.*;indexfile.*;lexer.*;lexerSource.*;literals.*;matchClassad.*;operators.*;query.*;sink.*;source.*;transaction.*;util.*;value.*;view.*;xmlLexer.*;xmlSink.*;xmlSource.*;cclassad.*;common.*" ClassadSrcs)

  add_library( classads STATIC ${ClassadSrcs} )
  set_target_properties( classads
	PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${EXTERNAL_STAGE}/lib 
	LIBRARY_OUTPUT_DIRECTORY ${EXTERNAL_STAGE}/lib
	RUNTIME_OUTPUT_DIRECTORY ${EXTERNAL_STAGE}/lib
	OUTPUT_NAME classad_ns)

else()
	
	# I should probably just add all and subtract tests.
	condor_selective_glob("attrrefs.cpp;common.cpp;collection*;fnCall.cpp;expr*;indexfile*;lexer*;literals.cpp;matchClassad.cpp;classad.cpp;debug.cpp;operators.cpp;util.cpp;value.cpp;query.cpp;sink.cpp;source.cpp;transaction.cpp;view.cpp;xml*" ClassadSrcs)
	add_library( classads STATIC ${ClassadSrcs} )
	set_property( TARGET classads PROPERTY FOLDER "libraries" )

endif()

set( CLASSADS_FOUND classads )

if (NOT PROPER)
  add_dependencies( classads pcre )
endif(NOT PROPER)

condor_exe_test(classad_unit_tester "classad_unit_tester.cpp" "${CLASSADS_FOUND};${PCRE_FOUND}")

condor_exe_test(classad_functional_tester "classad_functional_tester.cpp" "${CLASSADS_FOUND};${PCRE_FOUND}")


message (STATUS "external configured (CLASSADS_FOUND=${CLASSADS_FOUND})")
set( CLASSADS_FOUND ${CLASSADS_FOUND} PARENT_SCOPE )
set( HAVE_EXT_CLASSADS ON PARENT_SCOPE )
append_var (CONDOR_EXTERNALS classads)
