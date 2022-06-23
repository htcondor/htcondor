###############################################################
#
# Copyright (C) 2022, Condor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you
# may not use this file except in compliance with the License.  You may
# obtain a copy of the License at # #    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
###############################################################

option(WITH_OPENSSL "Compiling with support for OPENSSL" ON)

# This one only works with Windows

if (WITH_OPENSSL)

	if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		set (OPENSSL_DOWNLOAD openssl-1.1.1m-VC140-Win64.tar.gz)
		set (OPENSSL_DOWNLOAD_MD5SUM 3c110b43d8cfe0f49c36d91fe613ad65)
	else ()
		set (OPENSSL_DOWNLOAD openssl-1.1.1m-VC140-Win32.tar.gz)
		set (OPENSSL_DOWNLOAD_MD5SUM 979cd7ad777895a19b819fd4310aaa0b)
	endif ()

	include(FetchContent)
	FetchContent_declare(openssl
		URL ${EXTERNALS_SOURCE_URL}/${OPENSSL_DOWNLOAD}
		URL_MD5 ${OPENSSL_DOWNLOAD_MD5}
		)
	FetchContent_MakeAvailable(openssl)

	# create the imported (fake) targets the rest of condor uses
	add_library(openssl SHARED IMPORTED GLOBAL)
	add_library(crypto SHARED IMPORTED GLOBAL)
	add_library(openssl-headers SHARED IMPORTED GLOBAL)

	set_target_properties(openssl PROPERTIES IMPORTED_LOCATION "${openssl_SOURCE_DIR}/bin/libssl-1_1-x64.dll")
	set_target_properties(openssl PROPERTIES IMPORTED_IMPLIB "${openssl_SOURCE_DIR}/lib/libssl.lib")
	set_target_properties(crypto  PROPERTIES IMPORTED_LOCATION "${openssl_SOURCE_DIR}/bin/libcrypto-1_1-x64.dll")

	set_target_properties(crypto  PROPERTIES IMPORTED_IMPLIB "${openssl_SOURCE_DIR}/lib/libcrypto.lib")
	target_include_directories(openssl-headers INTERFACE "${openssl_SOURCE_DIR}/include")
	target_include_directories(openssl INTERFACE "${openssl_SOURCE_DIR}/include")

	# openssl always depends on crypto
	target_link_libraries(openssl INTERFACE crypto)

	file(GLOB OPENSSL_FILES_TO_INSTALL ${openssl_SOURCE_DIR}/lib/*.dll ${openssl_SOURCE_DIR}/bin/*.dll)
	install ( FILES
		${OPENSSL_FILES_TO_INSTALL}
		DESTINATION ${C_LIB} )

	# Remove this when we drop Ubuntu 18 (and cmake 3.10)
	get_target_property(OPENSSL_INCLUDE openssl-headers INTERFACE_INCLUDE_DIRECTORIES)
	set(OPENSSL_INCLUDE ${OPENSSL_INCLUDE} PARENT_SCOPE)

	if (OPENSSL_FOUND)
		message (STATUS "external configured (OPENSSL_FOUND=${OPENSSL_FOUND})")
		set( OPENSSL_FOUND ${OPENSSL_FOUND} PARENT_SCOPE )
		set( OPENSSL_INSTALL_LOC ${OPENSSL_INSTALL_LOC} PARENT_SCOPE )
		set( HAVE_EXT_OPENSSL ON PARENT_SCOPE )
	endif(OPENSSL_FOUND)

	message (STATUS "external configured (openssl)")

else(WITH_OPENSSL)

	message (STATUS "external skipped (openssl)")

endif(WITH_OPENSSL)
