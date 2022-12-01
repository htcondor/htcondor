# Find and export openssl and crypto library targets that point
# to libraries installed on this system

if (WINDOWS)
	message(FATAL_ERROR "Configuration error -- Windows cannot use a packaged OpenSSL")
endif()

# Create the two library targets that everyone will link to
# later clauses will set_target_properties to assign the actual libraries
# to these targets.  For platforms where we dlopen openssl, create
# a header-only target that has no libraries in it.
add_library(openssl SHARED IMPORTED GLOBAL)
add_library(crypto SHARED IMPORTED GLOBAL)
add_library(openssl-headers INTERFACE)

# openssl always depends on crypto
target_link_libraries(openssl INTERFACE crypto)

if (APPLE)
	include(FetchContent)

	# Fetch the whole source tarball, then delete the
	# CMakeLists.txt so we don't try to build it
	# Then download a copy of the dynamic libraries to
	# link against.
	FetchContent_declare(
		libressl_headers_darwin
		URL ${EXTERNALS_SOURCE_URL}/libressl-2.8.3.tar.gz
		PATCH_COMMAND /bin/rm -f "${CMAKE_BINARY_DIR}/_deps/libressl_headers_darwin-src/CMakeLists.txt" && ln -s ../libressl_libs_darwin-src/lib "${CMAKE_BINARY_DIR}/_deps/libressl_headers_darwin-src/lib"
	)

	# then fetch prebuilt libressl libraries
	string(TOLOWER "${SYS_ARCH}" LIBRESSL_ARCH)
	FetchContent_declare(
		libressl_libs_darwin
		URL ${EXTERNALS_SOURCE_URL}/libressl_libs_${LIBRESSL_ARCH}-2.8.3.tar.gz
	)
	FetchContent_MakeAvailable(libressl_headers_darwin libressl_libs_darwin)

	# On macOS 13 (Ventura), we can't use the OS LibreSSL
	# libraries at runtime.
	# Instead, ship the libraries we download and link against.
	execute_process(COMMAND install_name_tool -id @loader_path/libcrypto.44.dylib libcrypto.44.dylib
		WORKING_DIRECTORY ${libressl_libs_darwin_SOURCE_DIR}/lib
	)
	execute_process(COMMAND install_name_tool -change /usr/lib/libcrypto.44.dylib @loader_path/libcrypto.44.dylib -id @loader_path/libssl.46.dylib libssl.46.dylib
		WORKING_DIRECTORY ${libressl_libs_darwin_SOURCE_DIR}/lib
	)

	file(GLOB LIBRESSL_FILES_TO_INSTALL ${libressl_libs_darwin_SOURCE_DIR}/lib/lib*.*.dylib)
	install ( FILES
		${LIBRESSL_FILES_TO_INSTALL}
		DESTINATION ${C_LIB}/condor )

	set_target_properties(openssl PROPERTIES IMPORTED_LOCATION "${libressl_libs_darwin_SOURCE_DIR}/lib/libssl.dylib")
	set_target_properties(crypto  PROPERTIES IMPORTED_LOCATION "${libressl_libs_darwin_SOURCE_DIR}/lib/libcrypto.dylib")
	target_include_directories(openssl-headers INTERFACE "${libressl_headers_darwin_SOURCE_DIR}/include")
	target_include_directories(crypto INTERFACE "${libressl_headers_darwin_SOURCE_DIR}/include")

else()
	# The not-mac and not-windows case
	find_library(LIBSSL_FOUND "ssl")

	# If we dlopen libssl.so, this defines its name
	find_so_name( LIBSSL_SO ${LIBSSL_FOUND} )
	set( LIBSSL_SO ${LIBSSL_SO} PARENT_SCOPE )
	find_library(LIBCRYPTO_FOUND_LOCAL "crypto")

	# and set the contents of the targets appropriately
	set_target_properties(openssl PROPERTIES IMPORTED_LOCATION "${LIBSSL_FOUND}")

	# Remove this when we drop Ubuntu 18
	if (${CMAKE_VERSION} VERSION_GREATER 3.12) 
		target_include_directories(openssl-headers INTERFACE "/usr/include")
	endif()

	set_target_properties(crypto  PROPERTIES IMPORTED_LOCATION "${LIBCRYPTO_FOUND_LOCAL}")

endif()

# Lots of places in htcondor check for this, so lets set it
# now, and clear those all out later.  HTCondor can't really be
# built without openssl today, but it is a lot of work to remove
# all those checks
set( HAVE_EXT_OPENSSL ON PARENT_SCOPE )

# Remove this when we drop Ubuntu 18 (and cmake 3.10)
if (${CMAKE_VERSION} VERSION_LESS 3.12)
	set(OPENSSL_INCLUDE "/usr/include")
	set(OPENSSL_INCLUDE ${OPENSSL_INCLUDE} PARENT_SCOPE)
else()
	get_target_property(OPENSSL_INCLUDE openssl-headers INTERFACE_INCLUDE_DIRECTORIES)
	set(OPENSSL_INCLUDE ${OPENSSL_INCLUDE} PARENT_SCOPE)
endif()

message (STATUS "openssl external configured")
