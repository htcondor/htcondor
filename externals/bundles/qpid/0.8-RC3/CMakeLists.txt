###############################################################
 # 
 # Copyright (C) 1990-2010, Red Hat Inc. 
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

# qpid is for condor_contrib/mgmt/qmf
if (WANT_CONTRIB AND (LINUX OR WINDOWS))
	option(WITH_QPID "Compiling with support for QPID" ON)
endif()

if (WITH_QPID)

	if ( NOT PROPER )
	
		set (QPID_VER 0.8)

		if (NOT WINDOWS)
			set (QPID_CONFIGURE ./bootstrap && ./configure --prefix=${EXTERNAL_STAGE} )
			set (QPID_BUILD make )
			set (QPID_INSTALL make install )
		else()
			set (QPID_CONFIGURE cmake -DBOOST_ROOT:PATH=${BOOST_ROOT} -DCMAKE_INSTALL_PREFIX:PATH=${EXTERNAL_STAGE} -G \"Visual Studio 9 2008\" -DGEN_DOXYGEN:BOOL=OFF -DBoost_DEBUG:BOOL=TRUE )
			set (QPID_BUILD devenv qpid-cpp.sln /Rebuild RelWithDebInfo /project INSTALL )
			set (QPID_INSTALL echo "Done" )
		endif()

		ExternalProject_Add(qpid
					DEPENDS boost
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/qpid-${QPID_VER}
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/qpid-${QPID_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/qpid-${QPID_VER}.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd qpid-${QPID_VER}/cpp && ${QPID_CONFIGURE}
				    #--Build Step ----------
				    BUILD_COMMAND cd qpid-${QPID_VER}/cpp && ${QPID_BUILD}
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND cd qpid-${QPID_VER}/cpp && ${QPID_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		# all of the below locations need to be verified.
		if (WINDOWS)
			set( QPID_FOUND "${EXTERNAL_STAGE}/bin/qpidcommon.lib;${EXTERNAL_STAGE}/bin/qpidclient.lib;${EXTERNAL_STAGE}/bin/qmf.lib;${EXTERNAL_STAGE}/bin/qpidtypes.lib" )
			install ( FILES ${EXTERNAL_STAGE}/bin/qpidcommon.dll
					  ${EXTERNAL_STAGE}/bin/qpidclient.dll
					  ${EXTERNAL_STAGE}/bin/qmf.dll
					  ${EXTERNAL_STAGE}/bin/qpidtypes.dll
					  DESTINATION ${C_LIB} )
					  
			set( QMF_GEN ${EXTERNAL_STAGE}/managementgen/qmf-gen )
			
			set_property( TARGET qpid PROPERTY FOLDER "externals" )
			
		else()
			set( QPID_FOUND "${EXTERNAL_STAGE}/lib/libqpidcommon.so;${EXTERNAL_STAGE}/lib/libqpidclient.so;${EXTERNAL_STAGE}/lib/libqmf.so;${EXTERNAL_STAGE}/lib/libqpidtypes.so;${EXTERNAL_STAGE}/lib/qpidmessaging.so;${EXTERNAL_STAGE}/lib/libqmfconsole.so;${EXTERNAL_STAGE}/lib/libqmf2.so" )
			# TODO: so are we going to need to install qpid.so's along with condor..? or should we -static the plugins.
			# add linux install target
			#set( QMF_GEN ${EXTERNAL_STAGE}/managementgen/qmf-gen ) ?
		endif()

		append_var(CONDOR_EXTERNALS qpid)

	else( NOT PROPER )

		# I don't know how we check versions for native search of qpid-qmf? header check? program_exec?
		find_multiple( "qpidcommon;qpidclient;qmf;qmfengine;qmfconsole;qpidtypes;qpidmessaging;qmf2" QPID_FOUND)
		find_program ( QMF_GEN qmf-gen )

	endif( NOT PROPER )

	# you will need to update config.h.cmake with your var.
	message (STATUS "external configured QPID_FOUND=(${QPID_FOUND}) QMF_GEN=(${QMF_GEN})")
	set( HAVE_EXT_QPID ON PARENT_SCOPE )
	set (QPID_FOUND ${QPID_FOUND} PARENT_SCOPE)
	set (QMF_GEN ${QMF_GEN} PARENT_SCOPE)

else(WITH_QPID)

	message (STATUS "external skipped (qpid)")

endif(WITH_QPID)
