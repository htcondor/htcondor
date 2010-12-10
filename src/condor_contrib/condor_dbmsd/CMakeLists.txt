 ###############################################################
 # 
 # Copyright (C) 1990-2010, Redhat. 
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

if (HAVE_EXT_POSTGRESQL)

	condor_glob( dbmsdHdrs dbmsdSrcs "" )
	condor_exe( condor_dbmsd "${dbmsdHdrs};${dbmsdSrcs}" OFF "tt;${PCRE_FOUND};${CONDOR_LIBS};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${POSTGRESQL_FOUND}")

else(HAVE_EXT_POSTGRESQL)
	message(STATUS "NOTE: *dbmsd* targets will not be built HAVE_EXT_POSTGRESQL=${HAVE_EXT_POSTGRESQL} ")
endif(HAVE_EXT_POSTGRESQL)
