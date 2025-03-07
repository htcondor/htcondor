
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


##################################################################
## Begin the CPACK variable on-slaught.
##
## Start with the common section.
##################################################################
set (PACKAGE_REVISION "${BUILDID}")
set (CPACK_PACKAGE_NAME ${PACKAGE_NAME})
set (CPACK_PACKAGE_VENDOR "Condor Team - University of Wisconsin Madison")
set (CPACK_PACKAGE_VERSION ${PACKAGE_VERSION})
set (CPACK_PACKAGE_CONTACT "condor-users@cs.wisc.edu")
set (URL "http://www.cs.wisc.edu/condor/")
set (CONDOR_VERSION "${PACKAGE_NAME}-${PACKAGE_VERSION}")
GET_TIMEDATE( BUILD_TIMEDATE )

if(PRE_RELEASE)
  if(BUILDID)
    CLEAN_STRING( CLEAN_BUILDID "${BUILDID}" )
    set (CONDOR_PACKAGE_NAME "${PACKAGE_NAME}-${PACKAGE_VERSION}-${CLEAN_BUILDID}")
    set (CONDOR_TEST_PACKAGE_NAME "${TEST_PACKAGE_NAME}-${PACKAGE_VERSION}-${CLEAN_BUILDID}")
  elseif(BUILD_DATETIME)
    set (CONDOR_PACKAGE_NAME "${PACKAGE_NAME}-${PACKAGE_VERSION}-${BUILD_TIMEDATE}")
    set (CONDOR_TEST_PACKAGE_NAME "${TEST_PACKAGE_NAME}-${PACKAGE_VERSION}-${BUILD_TIMEDATE}")
  else()
    CLEAN_STRING( CLEAN_RELEASE "${PRE_RELEASE}" )
    set (CONDOR_PACKAGE_NAME "${PACKAGE_NAME}-${PACKAGE_VERSION}-${CLEAN_RELEASE}")
    set (CONDOR_TEST_PACKAGE_NAME "${TEST_PACKAGE_NAME}-${PACKAGE_VERSION}-${CLEAN_RELEASE}")
  endif(BUILDID)
else()
  set (CONDOR_PACKAGE_NAME "${CONDOR_VERSION}")
  set (CONDOR_TEST_PACKAGE_NAME "${TEST_PACKAGE_NAME}-${CONDOR_VERSION}")
endif()

set (CPACK_PACKAGE_DESCRIPTION "Condor is a specialized workload management system for
compute-intensive jobs. Like other full-featured batch systems,
Condor provides a job queueing mechanism, scheduling policy,
priority scheme, resource monitoring, and resource management.
Users submit their serial or parallel jobs to Condor, Condor places
them into a queue, chooses when and where to run the jobs based
upon a policy, carefully monitors their progress, and ultimately
informs the user upon completion.")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Condor: High Throughput Computing")

# Debian need this indentation to look nicely
# Short describtion (1st line) copied from Ubuntu's package description
set(CPACK_DEBIAN_DESCRIPTION_SUMMARY "a workload management system for compute-intensive jobs
 Condor is a specialized workload management system for
 compute-intensive jobs. Condor provides a job queueing mechanism,
 scheduling policy, priority scheme, resource monitoring,
 and resource management. Users submit their serial or parallel jobs
 to Condor, Condor places them into a queue, chooses when and where
 to run the jobs based upon a policy, carefully monitors their progress,
 and ultimately informs the user upon completion.")

set(CPACK_RESOURCE_FILE_LICENSE "${CONDOR_SOURCE_DIR}/LICENSE")
#set(CPACK_RESOURCE_FILE_README "${CONDOR_SOURCE_DIR}/build/backstage/release_notes/README")
#set(CPACK_RESOURCE_FILE_WELCOME "${CONDOR_SOURCE_DIR}/build/backstage/release_notes/DOC") # this should be more of a Hiya welcome.

if(SYSTEM_NAME)
  set(CPACK_SYSTEM_NAME "${SYSTEM_NAME}" )
else()
  set(CPACK_SYSTEM_NAME "${OS_NAME}-${SYS_ARCH}" )
endif()
set(CPACK_TOPLEVEL_TAG "${OS_NAME}-${SYS_ARCH}" )
set(CPACK_PACKAGE_ARCHITECTURE ${SYS_ARCH} )

# always strip the source files.
set(CPACK_SOURCE_STRIP_FILES TRUE)

# here is where we can
if (PLATFORM)
  set (CPACK_PACKAGE_FILE_NAME "${CONDOR_PACKAGE_NAME}-${PLATFORM}" )
  set (CPACK_TEST_PACKAGE_FILE_NAME "${CONDOR_TEST_PACKAGE_NAME}-${PLATFORM}" )
elseif(SYSTEM_NAME)
  set (CPACK_PACKAGE_FILE_NAME "${CONDOR_PACKAGE_NAME}-${SYSTEM_NAME}" )
  set (CPACK_TEST_PACKAGE_FILE_NAME "${CONDOR_TEST_PACKAGE_NAME}-${SYSTEM_NAME}" )
else()
  set (CPACK_PACKAGE_FILE_NAME "${CONDOR_PACKAGE_NAME}-${OS_NAME}-${SYS_ARCH}" )
  set (CPACK_TEST_PACKAGE_FILE_NAME "${CONDOR_TEST_PACKAGE_NAME}-${OS_NAME}-${SYS_ARCH}" )
endif()

# you can enable/disable file stripping.
option(CONDOR_STRIP_PACKAGES "Enables stripping of packaged binaries" ON)
set(CPACK_STRIP_FILES ${CONDOR_STRIP_PACKAGES})
if (NOT CONDOR_STRIP_PACKAGES)
  set (PACKAGE_REVISION "${PACKAGE_REVISION}+symbols" )
  set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-unstripped" )
else()
  set (CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}-stripped" )
endif()

##################################################################
## Now onto platform specific package generation
## http://www.itk.org/Wiki/CMake:CPackPackageGenerators
##################################################################

#option used to enable/disable make package for rpm/deb with different install paths
option(CONDOR_PACKAGE_BUILD "Enables a package build" OFF)

#option used to control RPATH when using rpmbuild directly
option(CONDOR_RPMBUILD "Whether rpmbuild is being used to build HTCondor" OFF)

# 1st set the location of the install targets, these are the defaults for
set( C_BIN			bin)
set( C_LIB			lib)
set( C_LIB_PUBLIC		lib)
set( C_LIB32		lib)
set( C_LIBEXEC		libexec )
set( C_LIBEXEC_BLAHP		libexec )
set( C_SBIN			sbin)

set( C_PYTHONARCHLIB lib/python)
set( C_PYTHON3ARCHLIB lib/python3)

set( C_INCLUDE		include)
set( C_INCLUDE_PUBLIC		include)
set( C_MAN			man)
set( C_SRC			src)
set( C_SQL			sql)
set( C_SERVICES     services)

set( C_INIT			etc/init.d )
set( C_ETC			etc/examples )
set( C_CONFIGD		etc/condor/config.d )
set( C_GANGLIAD		etc/condor/ganglia.d )
set( C_SYSCONFIG	etc/sysconfig )

set( C_ETC_EXAMPLES etc/examples )
set( C_SHARE_EXAMPLES .)
set( C_DOC			. )

set( C_LOCAL_DIR	var/lib/condor )
set( C_LOG_DIR		var/log/condor )
set( C_LOCK_DIR		var/lock/condor )
set( C_RUN_DIR		var/run/condor )
# NOTE: any RPATH should use these variables + PREFIX for location

# set a default generator
set ( CPACK_GENERATOR "TGZ" )

# Here, we set CONDOR_RPATH for the tarball build. If we're doing a native
# package build, it'll be overwritten further below. EXTERNALS_RPATH must
# include both paths, as we build the externals once for both types. The
# settings for EXTERNALS_RPATH must be kept in synch with the C_LIB
# settings made below for package builds.
if ( "${OS_NAME}" STREQUAL "LINUX" )
	set( EXTERNALS_LIB "${C_LIB}/condor" )
	if (${BIT_MODE} MATCHES "32" OR ${SYS_ARCH} MATCHES "IA64" )
		set( CONDOR_RPATH "$ORIGIN/../lib:/lib:/usr/lib:$ORIGIN/../lib/condor:/usr/lib/condor" )
		set( EXTERNALS_RPATH "$ORIGIN/../lib:/lib:/usr/lib:$ORIGIN/../lib/condor:/usr/lib/condor" )
		set( PYTHON_RPATH "$ORIGIN/../../:/lib:/usr/lib:$ORIGIN/../../condor" )
	else()
		set( CONDOR_RPATH "$ORIGIN/../lib:/lib64:/usr/lib64:$ORIGIN/../lib/condor:/usr/lib64/condor" )
		set( EXTERNALS_RPATH "$ORIGIN/../lib:/lib64:/usr/lib64:$ORIGIN/../lib/condor:/usr/lib64/condor" )
        if ( ${SYSTEM_NAME} MATCHES "rhel7" OR ${SYSTEM_NAME} MATCHES "centos7" OR ${SYSTEM_NAME} MATCHES "sl7")
            set( PYTHON_RPATH "$ORIGIN/../../:/usr/lib64/boost169:/lib64:/usr/lib64:$ORIGIN/../../condor" )
        else()
            set( PYTHON_RPATH "$ORIGIN/../../:/lib64:/usr/lib64:$ORIGIN/../../condor" )
        endif()
	endif()
elseif( APPLE )
	set( EXTERNALS_LIB "${C_LIB}/condor" )
endif()

# Use the limited RPATH when building directly with RPM
if ( CONDOR_RPMBUILD )
	if (${BIT_MODE} MATCHES "32" OR ${SYS_ARCH} MATCHES "IA64" )
	    set( CONDOR_RPATH "/usr/lib:/usr/lib/condor" )
	    set( PYTHON_RPATH "/usr/lib/condor" )
	else()
	    set( CONDOR_RPATH "/usr/lib64:/usr/lib64/condor" )
        if ( ${SYSTEM_NAME} MATCHES "rhel7" OR ${SYSTEM_NAME} MATCHES "centos7" OR ${SYSTEM_NAME} MATCHES "sl7")
            set( PYTHON_RPATH "/usr/lib64/condor:/usr/lib64/boost169" )
        else()
            set( PYTHON_RPATH "/usr/lib64/condor" )
        endif()
	endif ()
endif()

if ( "${OS_NAME}" STREQUAL "FREEBSD" )

	# Condor installs nothing useful to FreeBSD in C_INIT, so
	# just tuck it out of the way.  FreeBSD RC scripts come from
	# the port's "files" directory.
	set( C_INIT		etc/condor )
	set( C_ETC		etc/condor )
	set( C_CONFIGD		etc/condor/config.d )
	set( C_SYSCONFIG	etc/condor/sysconfig )
	
	set( C_ETC_EXAMPLES	etc/condor/examples )
	# Condor installs an "examples" directory into C_SHARE_EXAMPLES
	# so set it to share/condor instead of share/condor/examples.
	set( C_SHARE_EXAMPLES	share/examples/condor )
	set( C_DOC		share/doc/condor )
	
elseif ( ${OS_NAME} MATCHES "^WIN" )

	# override for windows.
	set( C_BIN bin )
	set( C_LIB bin )
	set( C_LIB_PUBLIC bin )
	set( C_LIBEXEC bin )
	set( C_SBIN bin )
	set( C_ETC etc )

	set (CPACK_INCLUDE_TOPLEVEL_DIRECTORY 0)
	set (CPACK_PACKAGE_INSTALL_DIRECTORY "${CONDOR_PACKAGE_NAME}")
	set (CPACK_PACKAGE_FILE_NAME "${CONDOR_PACKAGE_NAME}")
	set (CPACK_TEST_PACKAGE_FILE_NAME "${CONDOR_TEST_PACKAGE_NAME}")
	set (CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${CONDOR_VERSION}")

	############################################################
	# create the WIX package input file (condor.xsl) even if we aren't doing packaging.
	set (CONDOR_WIX_LOC ${CONDOR_SOURCE_DIR}/msconfig/WiX)

	# branding and licensing
	set (CPACK_PACKAGE_ICON ${CONDOR_WIX_LOC}/Bitmaps/dlgbmp.bmp)
	set (CPACK_RESOURCE_FILE_LICENSE "${CONDOR_SOURCE_DIR}/msconfig/license.rtf")

	# set the product GUID as a hash of $(CONDOR_VERSION) in condor_version_namespace.
	CLEAN_STRING( CLEAN_RELEASE "${PRE_RELEASE}" )
	message (STATUS "Generating CPACK_WIX_PRODUCT_GUID from ${CONDOR_VERSION} ${CLEAN_RELEASE}")
	execute_process(COMMAND ${CONDOR_SOURCE_DIR}/msconfig/generate_product_guid.cmd ${CONDOR_VERSION} ${CLEAN_RELEASE} 
	                OUTPUT_VARIABLE CPACK_WIX_PRODUCT_GUID)
	message (STATUS "CPACK_WIX_PRODUCT_GUID = ${CPACK_WIX_PRODUCT_GUID}")

	# this will strip trailing whitespace and newlines
	#string(REGEX REPLACE "[ \t\n]+" "" CPACK_WIX_PRODUCT_GUID "${CPACK_WIX_PRODUCT_GUID}")
	#set (CPACK_WIX_PRODUCT_GUID "61F15EF4-0C24-4240-8706-3EB2EA0AF62C")

	set (CPACK_WIX_UPGRADE_GUID "ef96d7c4-29df-403c-8fab-662386a089a4")
    
	set (CPACK_WIX_WXS_FILES ${CONDOR_WIX_LOC}/xml/CondorCfgDlg.wxs;${CONDOR_WIX_LOC}/xml/CondorPoolCfgDlg.wxs;${CONDOR_WIX_LOC}/xml/CondorExecCfgDlg.wxs;${CONDOR_WIX_LOC}/xml/CondorDomainCfgDlg.wxs;${CONDOR_WIX_LOC}/xml/CondorEmailCfgDlg.wxs;${CONDOR_WIX_LOC}/xml/CondorJavaCfgDlg.wxs;${CONDOR_WIX_LOC}/xml/CondorPermCfgDlg.wxs;${CONDOR_WIX_LOC}/xml/CondorVMCfgDlg.wxs;${CONDOR_WIX_LOC}/xml/CondorUpHostDlg.wxs)
	set (CPACK_WIX_BITMAP_FOLDER ${CONDOR_WIX_LOC}/Bitmaps)

	#setup all the merge module settings.
	if (MSVC90)
		set (VC_CRT_MSM Microsoft_VC90_CRT_x86.msm)
		find_file( CPACK_VC_POLICY_MODULE 
			policy_9_0_Microsoft_VC90_CRT_x86.msm
               		"C:/Program Files/Common Files/Merge Modules" "C:/Program Files (x86)/Common Files/Merge Modules" )
		set (WIX_MERGE_MODLES "<Merge Id=\"VCPolicy\" Language=\"1033\" DiskId=\"1\" SourceFile=\"${CPACK_VC_POLICY_MODULE}\"/>")
		set (WIX_MERGE_REFS "<MergeRef Id=\"VCPolicy\"/>")
		set (MSVCVER vc90)
		set (MSVCVERNUM 9.0)
	elseif(MSVC10)
		set (VC_CRT_MSM Microsoft_VC100_CRT_x86.msm)
		set (MSVCVER vc100)
		set (MSVCVERNUM 10.0)
	elseif(MSVC11)
		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
			set (VC_CRT_MSM Microsoft_VC110_CRT_x64.msm)
		else()
			set (VC_CRT_MSM Microsoft_VC110_CRT_x86.msm)
		endif()
		set (MSVCVER vc110)
		set (MSVCVERNUM 11.0)
	elseif(MSVC14)
		if (MSVC_VERSION LESS 1910)
			# 14.0 toolset 140 (vs 2015)
			if (CMAKE_SIZEOF_VOID_P EQUAL 8)
				set (VC_CRT_MSM Microsoft_VC140_CRT_x64.msm)
			else()
				set (VC_CRT_MSM Microsoft_VC140_CRT_x86.msm)
			endif()
			set (MSVCVER vc140)
			set (MSVCVERNUM 14.0)
		elseif (MSVC_VERSION LESS 1920)
			# 15.0 toolset 141 (vs 2017)
			if (CMAKE_SIZEOF_VOID_P EQUAL 8)
				set (VC_CRT_MSM Microsoft_VC141_CRT_x64.msm)
			else()
				set (VC_CRT_MSM Microsoft_VC141_CRT_x86.msm)
			endif()
			set (MSVCVER vc141)
			set (MSVCVERNUM 14.1)
		elseif (MSVC_VERSION LESS 1930)
			# 16.0 toolset 142 (vs 2019)
			if (CMAKE_SIZEOF_VOID_P EQUAL 8)
				set (VC_CRT_MSM Microsoft_VC142_CRT_x64.msm)
			else()
				set (VC_CRT_MSM Microsoft_VC142_CRT_x86.msm)
			endif()
			set (MSVCVER vc142)
			set (MSVCVERNUM 14.2)
		else ()
			# 17 toolset 143 (vs 2022)
			if (CMAKE_SIZEOF_VOID_P EQUAL 8)
				set (VC_CRT_MSM Microsoft_VC143_CRT_x64.msm)
			else()
				set (VC_CRT_MSM Microsoft_VC143_CRT_x86.msm)
			endif()
			set (MSVCVER vc143)
			set (MSVCVERNUM 14.3)
		endif()
	else()
		message(FATAL_ERROR "unsupported compiler version")
	endif()

	# look for the all important C-runtime
	find_file( CPACK_VC_MERGE_MODULE 
		${VC_CRT_MSM}
		"C:/Program Files/Common Files/Merge Modules" "C:/Program Files (x86)/Common Files/Merge Modules" )
	if (CPACK_VC_MERGE_MODULE MATCHES "-NOTFOUND")
		# VC tools after 2015 put the merge modules next to the tools
		find_file( CPACK_VC_MERGE_MODULE
			${VC_CRT_MSM}
			"$ENV{VS170COMNTOOLS}../../VC/Redist/MSVC/v143/MergeModules"
			"$ENV{SV170COMNTOOLS}../../VC/Redist/MSVC/14.16.27012/MergeModules"
		)
		## Starting with Visual C++ 2019, merge modules are deprecated and MS makes them hard to get a hold of
		## Instead, they want you to redistribute a separate installer for the vc runtime
		## so if we don't find the merge modules, just build the Condor installer without them
		if (CPACK_VC_MERGE_MODULE MATCHES "-NOTFOUND")
			set (VC_CRT_MSM off)
		endif ()
	endif ()

	if (VC_CRT_MSM)
		message(STATUS "CPACK_VC_MERGE_MODULE = ${CPACK_VC_MERGE_MODULE}")

		set (WIX_MERGE_MODLES "<Merge Id=\"VCCRT\" Language=\"1033\" DiskId=\"1\" SourceFile=\"${CPACK_VC_MERGE_MODULE}\"/>\n${WIX_MERGE_MODLES}")
		set (WIX_MERGE_REFS "<MergeRef Id=\"VCCRT\"/>\n${WIX_MERGE_REFS}")
	else(VC_CRT_MSM)
		message(STATUS "CRT MergeModules not found, building installer without them.")
		set (WIX_MERGE_MODLES "")
		set (WIX_MERGE_REFS "")
	endif ()
    		   
	configure_file(${CONDOR_WIX_LOC}/xml/win.xsl.in ${CONDOR_BINARY_DIR}/msconfig/WiX/xml/condor.xsl @ONLY)
	    
	set (CPACK_GENERATOR "ZIP")
		
	install ( FILES ${CPACK_WIX_WXS_FILES} ${CONDOR_BINARY_DIR}/msconfig/WiX/xml/condor.xsl
			DESTINATION ${C_ETC}/WiX/xml )
	
	install ( DIRECTORY ${CPACK_WIX_BITMAP_FOLDER}
			DESTINATION ${C_ETC}/WiX)
			
	install ( FILES ${CONDOR_SOURCE_DIR}/msconfig/license.rtf ${CONDOR_SOURCE_DIR}/msconfig/do_wix.bat ${CONDOR_SOURCE_DIR}/msconfig/WiX/config.vbs
			  DESTINATION ${C_ETC}/WiX )

	# the following will dump a header used to insert file info into bin's
	set ( WINVER ${CMAKE_CURRENT_BINARY_DIR}/src/condor_includes/condor_winver.h)
	file( WRITE ${WINVER} "#define CONDOR_VERSION \"${PACKAGE_VERSION}\"\n")
	#file( APPEND ${WINVER} "#define CONDOR_BLAH \"${YOUR_VAR}\"\n")

	# below are options an overrides to enable packge generation for rpm & deb
elseif( "${OS_NAME}" STREQUAL "LINUX" AND CONDOR_PACKAGE_BUILD )

	execute_process( COMMAND python2 -c "import distutils.sysconfig; import sys; sys.stdout.write(distutils.sysconfig.get_python_lib(1)[1:])" OUTPUT_VARIABLE C_PYTHONARCHLIB)
	execute_process( COMMAND python3 -c "import distutils.sysconfig; import sys; sys.stdout.write(distutils.sysconfig.get_python_lib(1)[1:])" OUTPUT_VARIABLE C_PYTHON3ARCHLIB)

	# it's a smaller subset easier to differentiate.
	# check the operating system name

	if ( DEB_SYSTEM_NAME )

		message (STATUS "Configuring for Debian package on ${LINUX_NAME}-${LINUX_VER}-${DEBIAN_CODENAME}-${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

		##############################################################
		# For details on DEB package generation see:
		# http://www.itk.org/Wiki/CMake:CPackPackageGenerators#DEB_.28UNIX_only.29
		##############################################################
		set ( CPACK_GENERATOR "DEB" )

		# Use dkpg-shlibdeps to generate dependency list
		set ( CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON )
		# Enable debug message
		set ( CPACK_DEBIAN_PACKAGE_DEBUG 1 )

		set (CPACK_PACKAGE_FILE_NAME "${CONDOR_PACKAGE_NAME}-${PACKAGE_REVISION}-${DEB_SYSTEM_NAME}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}" )
		string( TOLOWER "${CPACK_PACKAGE_FILE_NAME}" CPACK_PACKAGE_FILE_NAME )

		set (CPACK_TEST_PACKAGE_FILE_NAME "${CONDOR_TEST_PACKAGE_NAME}-${PACKAGE_REVISION}-${DEB_SYSTEM_NAME}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}" )
		string( TOLOWER "${CPACK_TEST_PACKAGE_FILE_NAME}" CPACK_TEST_PACKAGE_FILE_NAME )

		set ( CPACK_DEBIAN_PACKAGE_SECTION "contrib/misc")
		set ( CPACK_DEBIAN_PACKAGE_PRIORITY "extra")
		set ( CPACK_DEBIAN_PACKAGE_DESCRIPTION "${CPACK_DEBIAN_DESCRIPTION_SUMMARY}")
		set ( CPACK_DEBIAN_PACKAGE_MAINTAINER "Condor Team <${CPACK_PACKAGE_CONTACT}>" )
		set ( CPACK_DEBIAN_PACKAGE_VERSION "${PACKAGE_VERSION}-${PACKAGE_REVISION}")
		set ( CPACK_DEBIAN_PACKAGE_HOMEPAGE "${URL}")
		set ( CPACK_DEBIAN_PACKAGE_DEPENDS "python, adduser, libdate-manip-perl, ecryptfs-utils")

		#Control files
		set( CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
			"${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/debian/postinst"
			"${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/debian/postrm"
			"${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/debian/preinst"
			"${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/debian/prerm"
			"${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/debian/conffiles")

		#Directory overrides
		#Same as RPM
		set( C_BIN			usr/bin )
		set( C_LIB			usr/lib/condor )
		set( C_LIB_PUBLIC		usr/lib )
		set( C_LIB32		usr/lib/condor )
		set( C_SBIN			usr/sbin )
		set( C_INCLUDE		usr/include/condor )
		set( C_INCLUDE_PUBLIC		usr/include )
		set( C_MAN			usr/share/man )
		set( C_SRC			usr/src)
		set( C_SQL			usr/share/condor/sql)
		set( C_INIT			etc/init.d )
		set( C_ETC			etc/condor )
		set( C_CONFIGD		etc/condor/config.d )
		set( C_GANGLIAD         etc/condor/ganglia.d )

		#Debian specific
		set( C_ETC_EXAMPLES	usr/share/doc/condor/etc/examples )
		set( C_SHARE_EXAMPLES usr/share/doc/condor)
		set( C_DOC			usr/share/doc/condor )
		set( C_LIBEXEC		usr/libexec/condor )
		set( C_LIBEXEC_BLAHP	usr/libexec/blahp )
		set( C_SYSCONFIG	etc/default )

	elseif ( RPM_SYSTEM_NAME )
		# This variable will be defined if the platfrom support RPM
		message (STATUS "Configuring RPM package on ${LINUX_NAME}-${LINUX_VER} -> ${RPM_SYSTEM_NAME}.${SYS_ARCH}")

		##############################################################
		# For details on RPM package generation see:
		# http://www.itk.org/Wiki/CMake:CPackPackageGenerators#RPM_.28Unix_Only.29
		##############################################################

		set ( CPACK_GENERATOR "RPM" )
		#set ( CPACK_SOURCE_GENERATOR "RPM" )

		#Enable trace during packaging
		set(CPACK_RPM_PACKAGE_DEBUG 1)

		set (CPACK_PACKAGE_FILE_NAME "${CONDOR_PACKAGE_NAME}-${PACKAGE_REVISION}.${RPM_SYSTEM_NAME}.${SYS_ARCH}" )
		string( TOLOWER "${CPACK_PACKAGE_FILE_NAME}" CPACK_PACKAGE_FILE_NAME )

		set (CPACK_TEST_PACKAGE_FILE_NAME "${CONDOR_TEST_PACKAGE_NAME}-${PACKAGE_REVISION}.${RPM_SYSTEM_NAME}.${SYS_ARCH}" )
		string( TOLOWER "${CPACK_TEST_PACKAGE_FILE_NAME}" CPACK_TEST_PACKAGE_FILE_NAME )

		set ( CPACK_RPM_PACKAGE_RELEASE ${PACKAGE_REVISION})
		set ( CPACK_RPM_PACKAGE_GROUP "Applications/System")
		set ( CPACK_RPM_PACKAGE_LICENSE "Apache License, Version 2.0")
		set ( CPACK_RPM_PACKAGE_VENDOR ${CPACK_PACKAGE_VENDOR})
		set ( CPACK_RPM_PACKAGE_URL ${URL})
		set ( CPACK_RPM_PACKAGE_DESCRIPTION ${CPACK_PACKAGE_DESCRIPTION})

		string(TIMESTAMP CPACK_RPM_DATE "+%a %b %d %Y")

		#Specify SPEC file template
		set(CPACK_RPM_USER_BINARY_SPECFILE "${CMAKE_CURRENT_SOURCE_DIR}/build/packaging/rpm/condor.spec.in")

		#Directory overrides

		if (${BIT_MODE} MATCHES "32" OR ${SYS_ARCH} MATCHES "IA64" )
			set( C_LIB			usr/lib/condor )
			set( C_LIB_PUBLIC		usr/lib )
			set( C_LIB32		usr/lib/condor )
		else()
			set( C_LIB			usr/lib64/condor )
			set( C_LIB_PUBLIC		usr/lib64 )
			set( C_LIB32		usr/lib/condor )
		endif ()

		if ( ${LINUX_NAME} MATCHES "openSUSE"  OR  ${LINUX_NAME} MATCHES "sles" )
			set( CPACK_RPM_SHADOW_PACKAGE "pwdutils" )
		else()
			set( CPACK_RPM_SHADOW_PACKAGE "shadow-utils" )
		endif()

		set( C_BIN			usr/bin )
		set( C_LIBEXEC		usr/libexec/condor )
		set( C_LIBEXEC_BLAHP		usr/libexec/blahp )
		if ( ${LINUX_NAME} MATCHES "Fedora"  AND  ${LINUX_VER} GREATER 41 )
			set( C_SBIN			usr/bin )
		else()
			set( C_SBIN			usr/sbin )
		endif()
		set( C_INCLUDE		usr/include/condor )
		set( C_INCLUDE_PUBLIC		usr/include )
		set( C_MAN			usr/share/man/man1 )
		set( C_SRC			usr/src)
		set( C_SQL			usr/share/condor/sql)
		set( C_INIT			etc/init.d )
		set( C_ETC			etc/condor )
		set( C_CONFIGD		etc/condor/config.d )
		set( C_GANGLIAD		etc/condor/ganglia.d )
		set( C_SYSCONFIG	etc/sysconfig )
		set( C_ETC_EXAMPLES	usr/share/doc/${CONDOR_VERSION}/etc/examples )
		set( C_SHARE_EXAMPLES usr/share/doc/${CONDOR_VERSION})
		set( C_DOC			usr/share/doc/${CONDOR_VERSION} )

	endif()

	set( EXTERNALS_LIB "${C_LIB}" )
	set( CONDOR_RPATH "/${C_LIB_PUBLIC}:/${C_LIB}" )
	set( PYTHON_RPATH "/${C_LIB}" )

	# Generate empty folder to ship with package
	# Local dir
	file (MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/execute )
	add_custom_target(	change_execute_folder_permission
						ALL
						WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
						COMMAND chmod 1777 execute
						DEPENDS ${CMAKE_BINARY_DIR}/execute)
	install(DIRECTORY	${CMAKE_BINARY_DIR}/execute
			DESTINATION	"${C_LOCAL_DIR}"
			DIRECTORY_PERMISSIONS	USE_SOURCE_PERMISSIONS)
	# Blank directory means we are creating an emtpy directoy
	# CPackDeb does not package empyty directoy, so we will recreate them during posinst
	install(DIRECTORY
			DESTINATION	"${C_LOCAL_DIR}/spool")
	install(DIRECTORY
			DESTINATION	"${C_CONFIGD}")
	install(DIRECTORY
			DESTINATION	"${C_LOCK_DIR}")
	install(DIRECTORY
			DESTINATION	"${C_LOG_DIR}")
	install(DIRECTORY
			DESTINATION	"${C_RUN_DIR}")
	install(DIRECTORY
			DESTINATION	"${C_LIB32}")

endif()

