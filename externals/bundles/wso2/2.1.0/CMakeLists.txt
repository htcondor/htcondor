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
if (WANT_CONTRIB AND LINUX AND PROPER)
    option(WITH_WSO2 "Compiling with support for WITH_WSO2" ON)
endif()

if (WITH_WSO2)

	# need to embed libDir in axis2.xml
	if ( ${SYS_ARCH} STREQUAL "X86_64" )
		set (WS02_LIB_ROOT "/usr/lib64")
	else()
		set (WS02_LIB_ROOT "/usr/lib")
	endif ()

	if ( NOT PROPER )

		set ( WSO2_VER wso2-2.0.1 )

		#ExternalProject_Add(zlib
		#		    PREFIX ${EXTERNAL_BUILD_PREFIX}/${WSO2_VER}
		#		    #-- Download Step ----------
		#			DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${WSO2_VER}.tar.gz ${CMD_TERM}
		#		    DOWNLOAD_DIR ${EXTERNAL_DL}
		#		    URL http://parrot.cs.wisc.edu/externals/${WSO2_VER}.tar.gz
		#		    #--Configure step ----------
		#		    CONFIGURE_COMMAND cd ${WSO2_VER} && ./configure --prefix=${EXTERNAL_STAGE}
		#		    #--Build Step ----------
		#		    BUILD_COMMAND cd ${WSO2_VER} && make
		#		    #--install Step ----------
		#		    INSTALL_DIR ${EXTERNAL_STAGE}
		#		    INSTALL_COMMAND cd ${WSO2_VER} && make install )
		#
		# Set the target dependencies which the rest of condor depends on.
		#set(WSO2_FOUND "${EXTERNAL_STAGE}/STUFF?")
		#append_var(CONDOR_EXTERNALS zlib)
		#
		#install ( FILES ${EXTERNAL_STAGE}/lib/libz.a DESTINATION ${C_LIB} RENAME libcondor_z.a)

		
		#########################################################
		#### import from pmackinn's stuff.
		#if ( NOT DEFINED ENV{WSFCPP_HOME} )
		#	message( FATAL_ERROR "WSFCPP_HOME not defined for non-proper build")
		#endif ()
		#
		#set ( AXIS2_LIBS -L$ENV{WSFCPP_HOME}/lib
		#	-libaxis2_engine
		#	-laxutil -laxis2_axiom -laxis2_parser
		#	-lneethi -lguththila
		#	-laxis2_http_common -laxis2_http_receiver -laxis2_http_sender
		#	-lwso2_wsf )
		#
		#include_directories(
		#		$ENV{WSFCPP_HOME}/include/axis2-1.6.0
		#		$ENV{WSFCPP_HOME}/include/wso2-wsf-cpp
		#		$ENV{WSFCPP_HOME}/include/axis2-1.6.0/platforms )
		#	set(axis2_repo_loc $ENV{WSFCPP_HOME})
	else( )

		find_multiple( "axis2_engine;axutil;axis2_axiom;axis2_parser;axis2_http_common;axis2_http_receiver;axis2_http_sender;neethi;guththila;wso2_wsf" WSO2_FOUND )
		append_var (WSO2_INCLUDES "/usr/include/axis2-1.6.0;/usr/include/wso2-wsf-cpp;/usr/include/axis2-1.6.0/platforms")

		install (CODE "execute_process(COMMAND echo Local testing instructions )" )
		install (CODE "execute_process(COMMAND echo set WSFCPP_HOME to \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}axis2.xml)" )
		install (CODE "execute_process(COMMAND echo change the servicesDir parameter in axis2.xml to \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}services)" )

	endif( NOT PROPER )

endif(WITH_WSO2)

if (WSO2_FOUND)
	message (STATUS "external configured (WSO2_FOUND=${WSO2_FOUND})")
	set( WSO2_FOUND ${WSO2_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_WSO2 ON PARENT_SCOPE )
	set( WS02_LIB_ROOT ${WS02_LIB_ROOT} PARENT_SCOPE )
else()
	message (STATUS "external skipped (wso2)")
endif()
