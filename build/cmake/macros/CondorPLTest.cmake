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

MACRO ( CONDOR_PL_TEST _TARGET _DESC _TEST_RUNS )

	if (BUILD_TESTING)

		foreach(test ${_TEST_RUNS})
			file (APPEND ${TEST_TARGET_DIR}/list_${test} "${_TARGET}\n")
		endforeach(test)
		
		# add to all targets 
		file (APPEND ${TEST_TARGET_DIR}/list_all "${_TARGET}\n")

		# I'm not certain but it appears that the description files are not gen'd
		# file ( APPEND ${TEST_TARGET_DIR}/${_TARGET}.desc ${_DESC} )

	endif (BUILD_TESTING)

ENDMACRO ( CONDOR_PL_TEST )
