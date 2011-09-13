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

#Modify timestamp in changelog file
PackageDate( DEB DEB_DATE)

configure_file( changelog.in changelog.Debian @ONLY )

add_custom_target( generate_changelog
		   ALL
		   WORKING_DIRECTORY	${CMAKE_CURRENT_BINARY_DIR}
		   COMMAND rm -f changelog.Debian.gz
		   COMMAND gzip -9c changelog.Debian > changelog.Debian.gz )

install(FILES		copyright
        DESTINATION	"${C_DOC}" )

install(FILES		${CMAKE_CURRENT_BINARY_DIR}/changelog.Debian.gz
        DESTINATION	"${C_DOC}" )
