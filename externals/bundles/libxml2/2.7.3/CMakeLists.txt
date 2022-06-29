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

if (LINUX OR APPLE)
  option(WITH_LIBXML2 "Compiling with support for LIBXML2" ON)
endif()

if (WITH_LIBXML2)
	
    	find_multiple( "xml2" LIBXML2_FOUND )

endif(WITH_LIBXML2)

if (LIBXML2_FOUND)
	message (STATUS "external configured (LIBXML2_FOUND=${LIBXML2_FOUND})")
	set( LIBXML2_FOUND ${LIBXML2_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_LIBXML2 ON PARENT_SCOPE )
else()
	message (STATUS "external skipped (libxml2)")
endif()


