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

		set (LIBVERT_VER libvirt-0.6.2)

		ExternalProject_Add(libvirt
					DEPENDS libxml2
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${LIBVERT_VER}
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${LIBVERT_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/${LIBVERT_VER}.tar.gz
					PATCH_COMMAND cd ${LIBVERT_VER} && patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/no_gnutls.patch
					#--Configure step ----------
				    CONFIGURE_COMMAND cd ${LIBVERT_VER} && ./configure --prefix=${EXTERNAL_STAGE} --disable-shared --without-python --without-xen --without-libvirtd --without-remote --without-test --without-uml --without-openvz --without-avahi --without-selinux --without-sasl --with-libxml=${EXTERNAL_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND cd ${LIBVERT_VER} && make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND cd ${LIBVERT_VER} && make install )

		# Set the target dependencies which the rest of condor depends on.
		set(LIBVIRT_FOUND "${EXTERNAL_STAGE}/lib/libvirt.a")
		append_var(CONDOR_EXTERNALS libvirt)

	endif( NOT PROPER AND NOT LIBVIRT_FOUND )

endif(WITH_LIBVIRT)

if (LIBVIRT_FOUND)
    message (STATUS "external configured (LIBVIRT_FOUND=${LIBVIRT_FOUND})")
    set( LIBVIRT_FOUND ${LIBVIRT_FOUND} PARENT_SCOPE )
    set( HAVE_EXT_LIBVIRT ON PARENT_SCOPE )
else(LIBVIRT_FOUND)
    message (STATUS "external skipped (libvirt)")
endif(LIBVIRT_FOUND)
