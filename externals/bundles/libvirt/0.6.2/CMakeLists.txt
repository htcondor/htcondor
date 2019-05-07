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

if (LINUX)
  option(WITH_LIBVIRT "Compiling with support for LIBVIRT" ON)
endif(LINUX)

if (WITH_LIBVIRT)

    find_multiple( "virt" LIBVIRT_FOUND )

	if ( NOT PROPER AND NOT LIBVIRT_FOUND )

		if (NOT LIBXML2_FOUND)
			message(FATAL_ERROR "Can't find libxml2, needed to build libvirt external!")
		endif()

		set (LIBVIRT_REAL_VERSION libvirt-0.6.2)

		condor_pre_external( LIBVIRT ${LIBVIRT_REAL_VERSION}-p1 "lib;include" "lib/libvirt.a" )

		if ( BUILDING_LIBXML2 )
			set (LIBVIRT_DEPENDS ${LIBXML2_REF})
			set (LIBVIRT_CONFIGURE --with-libxml=${LIBXML2_INSTALL_LOC})
		else ()
			set (LIBVIRT_DEPENDS "")
			set (LIBVIRT_CONFIGURE "")
		endif ()
		ExternalProject_Add(libvirt
					DEPENDS ${LIBVIRT_DEPENDS}
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${LIBVIRT_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/${LIBVIRT_REAL_VERSION}.tar.gz
				    PATCH_COMMAND patch -N -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/no_gnutls.patch
					#--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${LIBVIRT_INSTALL_LOC} --without-python --without-xen --without-libvirtd --without-remote --without-test --without-uml --without-openvz --without-avahi --without-selinux --without-sasl ${LIBVIRT_CONFIGURE}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${LIBVIRT_INSTALL_LOC}
				    INSTALL_COMMAND make install )

		# Set the target dependencies which the rest of condor depends on.
		set(LIBVIRT_FOUND "${LIBVIRT_INSTALL_LOC}/lib/libvirt.so")
		
		condor_post_external( libvirt include OFF "${LIBVIRT_FOUND}" )

	endif( NOT PROPER AND NOT LIBVIRT_FOUND )

endif(WITH_LIBVIRT)

if (LIBVIRT_FOUND)
    message (STATUS "external configured (LIBVIRT_FOUND=${LIBVIRT_FOUND})")
    set( LIBVIRT_FOUND ${LIBVIRT_FOUND} PARENT_SCOPE )
    set( HAVE_EXT_LIBVIRT ON PARENT_SCOPE )
else(LIBVIRT_FOUND)
    message (STATUS "external skipped (libvirt)")
endif(LIBVIRT_FOUND)
