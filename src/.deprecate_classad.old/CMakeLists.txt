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


if (WANT_OLD_CLASSADS)
	file( GLOB ClassAdsOldRmvSrcs classad_lookup* *test*)

	condor_glob( ClassAdsOldHdrs ClassAdsOldSrcs "${ClassAdsOldRmvSrcs}" )

	#create the library
	condor_static_lib( classad.old "${ClassAdsOldHdrs};${ClassAdsOldSrcs}" )
else()
	message(STATUS "NOTE: skipping classads.old")
endif()
