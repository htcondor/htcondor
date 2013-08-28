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


##############################################################
## Abandon all hope ye who enter here:
## this be the mystical workings of the infamous
## standard universe.  No moral platitudes apply, so
## batton your hatches, find yourself a cup of ale,
## because you are about to loose your mind.
## YARRR!
##############################################################

if (NOT CLIPPED)

	# check only the major and minor bits of gcc version.
	string(SUBSTRING ${CMAKE_C_COMPILER_VERSION} 0 2 GCC_VER_CHECK)
	# corner off glibc b/c it effect  
	set(GLIBC_DETECTED ON)
        set(GLIBC_BUILD_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/glibc-prefix/src/glibc)

	# By default, we don't need to set any flag environment variables
	set (GLIBC_FLAGS /bin/true)

	if (${GLIBC_VERSION} STREQUAL "23")
		set (VER 2.3.2.27.9.7)

		set (GLIBC_PATCH  patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/x86_64-syscall.S.patch )

		if (${GCC_VER_CHECK} MATCHES "3.4")
			set (GLIBC_PATCH  ${GLIBC_PATCH} && patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/gcc-3.4-fixes.patch )
		elseif (${GCC_VER_CHECK} MATCHES "4.")
			set (GLIBC_PATCH  ${GLIBC_PATCH} && patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/gcc-4-fixes.patch )
		endif()

		set (GLIBC_CONFIGURE ./configure --enable-static-nss --enable-add-ons=linuxthreads,c_stubs,glibc-compat --disable-shared --without-cvs)

		set (GLIBC_MAKE make )

	elseif(${GLIBC_VERSION} STREQUAL "25")

		if (${SYS_ARCH} STREQUAL "X86_64")

			set (VER 2.5-20061008T1257-x86_64-p0)
			set (GLIBC_TARNAME glibc-2.5-20061008T1257-x86_64-p0)

			set (GLIBC_CONFIGURE ./configure --enable-add-ons=nptl,rtkaio,c_stubs,libidn --without-cvs --enable-kernel=2.6.9 --with-headers=$HOME/junk/glibc-2.5-20061008T1257/override_headers:/usr/include --enable-bind-now --with-tls --with-__thread --build x86_64-redhat-linux --host x86_64-redhat-linux --disable-profile --enable-static-nss)

			set (GLIBC_MAKE export "CFLAGS=-mtune=generic -DNDEBUG=1 -g -O3" && $(MAKE) -r PARALLELMFLAGS=-s)

		else()
			set (VER 2.5-20061008T1257-p0)
			set (GLIBC_TARNAME glibc-2.5-20061008T1257-p0)

			set (GLIBC_CONFIGURE ./configure --enable-add-ons=nptl,rtkaio,c_stubs,libidn --without-cvs --enable-kernel=2.6.9 --with-headers=$HOME/junk/glibc-2.5-20061008T1257/override_headers:/usr/include --enable-bind-now --with-tls --with-__thread --build i486-redhat-linux --host i486-redhat-linux --disable-profile --enable-static-nss)

			set (GLIBC_MAKE export "CFLAGS=-march=i386 -mtune=generic -mno-tls-direct-seg-refs -DNDEBUG=1 -g -O3" && $(MAKE) -r PARALLELMFLAGS=-s )
		endif()

		set (GLIBC_PATCH patch -N --binary -p1 -i
${CMAKE_CURRENT_SOURCE_DIR}/${VER}/enable-static-nss.patch && patch -N --binary
-p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-nscd.patch && patch -N --binary
-p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-static-link-warning.patch &&
patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-tempnam-warning.patch)

	elseif(${GLIBC_VERSION} STREQUAL "27")

		if (${SYS_ARCH} STREQUAL "X86_64")
			set (VER 2.7-18-x86_64)
			set (GLIBC_TARNAME glibc-2.7-18-x86_64)

			set (GLIBC_FLAGS export CC=gcc-4.3 && export CXX=g++-4.3 && export AUTOCONF=false && export MAKEINFO=: && export "CFLAGS=-pipe -O2 -fstrict-aliasing -g" )

            set (GLIBC_CONFIGURE ./configure --host=x86_64-linux-gnu --build=x86_64-linux-gnu --without-cvs --enable-add-ons=libidn,nptl --without-selinux --enable-kernel=2.6.8 --disable-profile --with-headers=${GLIBC_BUILD_PREFIX}/debian/include --enable-bind-now --with-tls --with-__thread --enable-static-nss)

			set (GLIBC_MAKE $(MAKE) -r)
		else()
			set (VER 2.7-18)
			set (GLIBC_TARNAME glibc-2.7-18)

			set (GLIBC_FLAGS export CC=gcc-4.3 && export CXX=g++-4.3 && export AUTOCONF=false && export MAKEINFO=: && export "CFLAGS=-pipe -O2 -fstrict-aliasing -g -march=i686 -mtune=i686 -O3" )

			set (GLIBC_CONFIGURE ./configure --host=i686-linux --build=i486-linux-gnu --without-cvs --enable-add-ons=libidn,nptl --without-selinux --enable-kernel=2.6.8 --disable-profile --with-headers=${GLIBC_BUILD_PREFIX}/debian/include --enable-bind-now --with-tls --with-__thread --enable-static-nss)

			set (GLIBC_MAKE export "CFLAGS=-pipe -O2 -fstrict-aliasing -g -march=i686 -mtune=i686 -O3" && $(MAKE) -r PARALLELMFLAGS=-s)
		endif()

		set (GLIBC_PATCH patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/enable-static-nss.patch && patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-nscd.patch )

	elseif(${GLIBC_VERSION} STREQUAL "211")

		if (${SYS_ARCH} STREQUAL "X86_64")
			set (VER 2.11.2-10-x86_64-p1)
			set (GLIBC_TARNAME glibc-2.11.2-10-x86_64)

			set (GLIBC_FLAGS export CC=gcc-4.4 && export CXX=g++-4.4 && export AUTOCONF=false && export MAKEINFO=: && export "CFLAGS=-pipe -O2 -fstrict-aliasing -g" )

			set (GLIBC_CONFIGURE ./configure CC=gcc-4.4 CXX=g++-4.4 "CFLAGS=-pipe -O2 -fstrict-aliasing -g" --host=x86_64-linux-gnu --build=x86_64-linux-gnu --without-cvs --enable-add-ons=libidn,nptl --disable-profile --without-selinux "--with-pkgversion=Debian EGLIBC 2.11.2-10" --with-bugurl=http://www.debian.org/Bugs/ --enable-kernel=2.6.18 --enable-multi-arch --enable-static-nss )

			set (GLIBC_MAKE $(MAKE) -r)
		else()
			set (VER 2.11.2-10)
			set (GLIBC_TARNAME glibc-2.11.2-10)

			# XXX FIXME
			set (GLIBC_FLAGS export CC=gcc-4.4 && export CXX=g++ && export AUTOCONF=false && export MAKEINFO=: && export "CFLAGS=-pipe -O2 -fstrict-aliasing -g" )

			# XXX FIXME
			set (GLIBC_CONFIGURE ./configure CC=gcc-4.4 CXX=g++-4.4 "CFLAGS=-pipe -O2 -fstrict-aliasing -g" --host=x86_64-linux-gnu --build=x86_64-linux-gnu --without-cvs --enable-add-ons=libidn,nptl --disable-profile --without-selinux --with-pkgversion="Debian EGLIBC 2.11.2-10" --with-bugurl="http://www.debian.org/Bugs/" --with-headers=${GLIBC_BUILD_PREFIX}/debian/include --enable-kernel=2.6.18 --enable-multi-arch --enable-static-nss )

			# XXX FIXME
			set (GLIBC_MAKE export "CFLAGS=-pipe -O2 -fstrict-aliasing -g -march=i686 -mtune=i686 -O3" && $(MAKE) -r PARALLELMFLAGS=-s)

		endif()

		# The enable-static-nss patch is still needed...
		set (GLIBC_PATCH patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/enable-static-nss.patch && patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-nscd.patch)

	elseif(${GLIBC_VERSION} STREQUAL "212")

		if (${SYS_ARCH} STREQUAL "X86_64")
			set (VER 2.12-2-x86_64-p1)
			set (GLIBC_TARNAME glibc-2.12-2-x86_64)

			set (GLIBC_FLAGS export CC=gcc && export CXX=g++ && export AUTOCONF=false && export MAKEINFO=: && export "CFLAGS=-mtune=generic -fasynchronous-unwind-tables -DNDEBUG -g -O3" )

            set (GLIBC_CONFIGURE ./configure CC=gcc CXX=g++ "CFLAGS=-mtune=generic -fasynchronous-unwind-tables -DNDEBUG -g -O3" --enable-add-ons=nptl,rtkaio,c_stubs,libidn --without-cvs --enable-kernel=2.6.18 --with-headers=/usr/include --enable-bind-now --with-tls --with-__thread --build x86_64-redhat-linux --host x86_64-redhat-linux --enable-multi-arch --disable-profile --enable-experimental-malloc --enable-nss-crypt --enable-static-nss )

			set (GLIBC_MAKE $(MAKE) -r)
		else()
			set (VER 2.12-2)
			set (GLIBC_TARNAME glibc-2.12-2)

			set (GLIBC_FLAGS export CC=gcc-4.3 && export CXX=g++-4.3 && export AUTOCONF=false && export MAKEINFO=: && export "CFLAGS=-mtune=generic -fasynchronous-unwind-tables -DNDEBUG -g -O3" )

			# XXX FIXME
			set (GLIBC_CONFIGURE ./configure --host=i686-linux --build=i486-linux-gnu --without-cvs --enable-add-ons=libidn,nptl --without-selinux --enable-kernel=2.6.8 --disable-profile --with-headers=${GLIBC_BUILD_PREFIX}/debian/include --enable-bind-now --with-tls --with-__thread --enable-static-nss)

			set (GLIBC_MAKE export "CFLAGS=-pipe -O2 -fstrict-aliasing -g -march=i686 -mtune=i686 -O3" && $(MAKE) -r PARALLELMFLAGS=-s)
		endif()

		# The enable-static-nss patch works, and disable link warning about dynamic nss
		set (GLIBC_PATCH patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/enable-static-nss.patch && patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-static-link-warning.patch && patch --binary -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-tempnam-warning.patch && patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-nscd.patch)

	elseif(${GLIBC_VERSION} STREQUAL "213")
		if (${SYS_ARCH} STREQUAL "X86_64")
			set (VER 2.13-38)
			set (GLIBC_TARNAME glibc-2.13-38)

			set (GLIBC_FLAGS export CC=gcc-4.4 && export CXX=g++-4.4 && export AUTOCONF=false && export MAKEINFO=: && export "CFLAGS=-pipe -O2 -fstrict-aliasing -g")

            # Omitted --enable-stackguard-randomization
            # Debian's ./debian/rules constructs a local include/ directory
            # that's duplicated in /usr/include, but only if you install
            # the 'gcc-multilib' package.  Installing a gcc-specific multilib
            # won't work.  *sigh*
			set (GLIBC_CONFIGURE ./configure CC=gcc-4.4 CXX=g++-4.4 "CFLAGS=-pipe -O2 -fstrict-aliasing -g" --host=x86_64-linux-gnu --build=x86_64-linux-gnu --without-cvs --enable-add-ons=libidn,nptl --enable-profile --without-selinux "--with-pkgversion=Debian EGLIBC 2.13-38" --with-bugurl=http://www.debian.org/Bugs/ --with-headers=/usr/include --enable-kernel=2.6.26 --enable-multi-arch --enable-static-nss)

			set (GLIBC_MAKE $(MAKE) -r)
		else()
		    message(STATUS "HTCondor will not build 32-bit standard universe for glibc version ${GLIBC_VERSION}" )
		    set(GLIBC_DETECTED OFF)
		    set(CLIPPED ON PARENT_SCOPE)
		endif()

		# The enable-static-nss patch works, and disable link warning about dynamic nss
		set (GLIBC_PATCH patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/enable-static-nss.patch && patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-static-link-warning.patch && patch --binary -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-tempnam-warning.patch && patch -N --binary -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/${VER}/disable-nscd.patch)

	else()
		message(STATUS "Condor can not build standard universe for glibc version:${GLIBC_VERSION}")
		set(GLIBC_DETECTED OFF)
		set(CLIPPED ON PARENT_SCOPE )
	endif()

	if (GLIBC_DETECTED)

	  condor_pre_external( GLIBC glibc-${VER} "lib;include" "lib/libc.a")
	  set (GLIBC_INSTALL make install )

	  ExternalProject_Add(glibc
					  #-- Download Step ----------
					  DOWNLOAD_DIR ${GLIBC_STAGE}/dl
					  URL ${EXTERNALS_SOURCE_URL}/${GLIBC_TARNAME}.tar.gz
					  #--Patch step ----------
					  PATCH_COMMAND ${GLIBC_PATCH}
					  #--Configure step ----------
					  CONFIGURE_COMMAND mkdir -p build && cd build && ${GLIBC_FLAGS} && ../${GLIBC_CONFIGURE} --prefix=${GLIBC_INSTALL_LOC}
					  #--Build Step ----------
					  BUILD_COMMAND cd build && ${GLIBC_MAKE}
					  BUILD_IN_SOURCE 1
					  #--install Step ----------
					  INSTALL_DIR ${GLIBC_INSTALL_LOC}
					  INSTALL_COMMAND cd build && ${GLIBC_INSTALL} )

	  set( EXT_GLIBC_FOUND "${GLIBC_INSTALL_LOC}/lib/libc.a" )
	  message ( STATUS "external configured (EXT_GLIBC_FOUND=${EXT_GLIBC_FOUND})" )
	  set( EXT_GLIBC_FOUND ${EXT_GLIBC_FOUND} PARENT_SCOPE )
	  
	  condor_post_external( glibc OFF OFF )

	  install ( FILES ${GLIBC_INSTALL_LOC}/lib/libc.a DESTINATION ${C_LIB} RENAME libcondor_c.a)
	  install ( FILES ${GLIBC_INSTALL_LOC}/lib/libnss_files.a DESTINATION ${C_LIB} RENAME libcondor_nss_files.a)
	  install ( FILES ${GLIBC_INSTALL_LOC}/lib/libnss_dns.a DESTINATION ${C_LIB} RENAME libcondor_nss_dns.a)
	  install ( FILES ${GLIBC_INSTALL_LOC}/lib/libresolv.a DESTINATION ${C_LIB} RENAME libcondor_resolv.a)
  endif(GLIBC_DETECTED)

endif(NOT CLIPPED)
