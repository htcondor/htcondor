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

if ( HAVE_EXT_CREAM )

	# cream is such an odd beast and it collides with other versions of
	# externals
        if (NOT PROPER)
            include_directories ( ${CREAM_STAGE}/include )
            link_directories ( ${CREAM_STAGE}/lib )
        endif()
        
	add_definitions(-D_REENTRANT)
	# LTO fails with CREAM because they have references to the typeinfo of exceptions,
	# but the typeinfo for these are not generated as the exceptions are solely defined
	# in header files.
	# Hence, CREAM exception handling does not work; we just didn't notice until LTO was attempted.
	STRING (REGEX REPLACE "-flto" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	
	set_source_files_properties(cream_gahp_server.cpp PROPERTIES COMPILE_FLAGS -Wno-shadow)

	condor_exe( cream_gahp
		"cream_gahp_server.cpp"
		${C_SBIN}
		"${CREAM_FOUND};${OPENSSL_FOUND};${EXPAT_FOUND};${GLOBUS_THR_FOUND};${VOMS_FOUND};${CLASSADS_FOUND};${PCRE_FOUND}"
		OFF )
        
else ( HAVE_EXT_CREAM )

	message(STATUS "NOTE: *cream* targets will not be built HAVE_EXT_CREAM=${HAVE_EXT_CREAM}")

endif ( HAVE_EXT_CREAM )

