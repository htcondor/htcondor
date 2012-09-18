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

MACRO (CONDOR_UNIT_TEST _CNDR_TARGET _SRCS _LINK_LIBS _BOOST_UT)

	set(${_CNDR_TARGET}BOOST_UT ${_BOOST_UT})

	# in c the line below == if (testing && (!b_boost_test || (b_boost_test && b_have_boost)) 
	if (BUILD_TESTING AND ( NOT ${_CNDR_TARGET}BOOST_UT OR (${_CNDR_TARGET}BOOST_UT AND HAVE_EXT_BOOST)) )
	
		enable_testing()

		# we are dependent on boost unit testing framework.
		if (${_CNDR_TARGET}BOOST_UT)
		
		    include_directories(${BOOST_INCLUDE})
		    
		endif() 
		
		# the structure is a testing subdirectory, so set inlude to go up one level
		include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../)

		set ( LOCAL_${_CNDR_TARGET} ${_CNDR_TARGET} )

		if ( WINDOWS )
			string (REPLACE ".exe" "" ${LOCAL_${_CNDR_TARGET}} ${LOCAL_${_CNDR_TARGET}})
		else()
			if (PROPER AND ${_CNDR_TARGET}BOOST_UT)
			  # if you are proper then link the system .so (now .a's by def)
			  add_definitions(-DBOOST_TEST_DYN_LINK)
			endif()
		endif( WINDOWS )

		add_executable( ${LOCAL_${_CNDR_TARGET}} ${_SRCS})
		
		if (CONDOR_EXTERNALS)
			add_dependencies ( ${LOCAL_${_CNDR_TARGET}} ${CONDOR_EXTERNALS} )
		endif()

		if ( ${_CNDR_TARGET}BOOST_UT )
		
		    if ( WINDOWS )
			set_property( TARGET ${LOCAL_${_CNDR_TARGET}} PROPERTY FOLDER "tests" )
			#windows will require runtime to match so make certain we link the right one.
			target_link_libraries (${LOCAL_${_CNDR_TARGET}} optimized libboost_unit_test_framework-${MSVCVER}-mt-${BOOST_SHORTVER} )
			target_link_libraries (${LOCAL_${_CNDR_TARGET}} debug libboost_unit_test_framework-${MSVCVER}-mt-gd-${BOOST_SHORTVER} ) 
		    else()
			target_link_libraries (${LOCAL_${_CNDR_TARGET}}  -lboost_unit_test_framework )
		    endif ( WINDOWS )
		    
		endif()
		    
		condor_set_link_libs( ${LOCAL_${_CNDR_TARGET}} "${_LINK_LIBS}" )

		add_test ( ${LOCAL_${_CNDR_TARGET}}_unit_test ${LOCAL_${_CNDR_TARGET}} )

	endif()

ENDMACRO(CONDOR_UNIT_TEST)
