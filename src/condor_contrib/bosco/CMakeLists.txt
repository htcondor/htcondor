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

if ( LINUX OR DARWIN )
	if ( WANT_FULL_DEPLOYMENT )
		option(WITH_BOSCO "Compiling with support for BOSCO" ON)
	else( WANT_FULL_DEPLOYMENT )
		option(WITH_BOSCO "Compiling with support for BOSCO" OFF)
	endif( WANT_FULL_DEPLOYMENT )
endif ( LINUX OR DARWIN )

if ( WITH_BOSCO )

	install ( FILES bosco_cluster DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	install ( FILES bosco_ssh_start DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	install ( FILES shellselector DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	install ( FILES bosco_start DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	install ( FILES bosco_stop DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	install ( FILES bosco_uninstall DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	install ( FILES bosco_findplatform DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	install ( FILES bosco_quickstart DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	install ( FILES htsub DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

endif( WITH_BOSCO )
