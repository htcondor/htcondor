option(WITH_QPID "Compiling with support for QPID" ON)

if (WITH_QPID)

	if ( NOT PROPER )

		set (QPID_MAJOR_VER 0.8)
		set (QPID_MINOR_VER RC3)

		ExternalProject_Add(qpid
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${QPID_VER}
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://people.apache.org/~robbie/qpid/${QPID_MAJOR_VER}/${QPID_MINOR_VER}/qpid-cpp-${QPID_MAJOR_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://people.apache.org/~robbie/qpid/qpid-cpp-${QPID_MAJOR_VER}.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd qpidc-${QPID_MAJOR_VER} && ./configure --prefix=${EXTERNAL_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND cd qpidc-${QPID_MAJOR_VER} && make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND cd qpidc-${QPID_MAJOR_VER} && make install )

		# Set the target dependencies which the rest of condor depends on.
		# all of the below locations need to be verified.
		if (WINDOWS)
			set( QPID_FOUND "${EXTERNAL_STAGE}/lib/qpidcommon.lib;${EXTERNAL_STAGE}/lib/qpidclient.lib;${EXTERNAL_STAGE}/lib/qmf.lib;${EXTERNAL_STAGE}/lib/qpidtypes.lib" )
			install ( FILES ${EXTERNAL_STAGE}/bin/qpidcommon.dll
					  ${EXTERNAL_STAGE}/bin/qpidcommon.pdb
					  ${EXTERNAL_STAGE}/bin/qpidclient.dll
					  ${EXTERNAL_STAGE}/bin/qpidclient.pdb
					  ${EXTERNAL_STAGE}/bin/qmf.dll
					  ${EXTERNAL_STAGE}/bin/qmf.pdb
					  ${EXTERNAL_STAGE}/bin/qpidtypes.dll
					  DESTINATION ${C_LIB} )
			set( QMF_GEN ${EXTERNAL_STAGE}/managementgen/qmf-gen )
		else()
			set( QPID_FOUND "${EXTERNAL_STAGE}/lib/libqpidcommon.so;${EXTERNAL_STAGE}/lib/libqpidclient.so;${EXTERNAL_STAGE}/lib/libqmf.so;${EXTERNAL_STAGE}/lib/libqpidtypes.so" )
			# TODO: so are we going to need to install qpid.so's along with condor..? or should we -static the plugins.
			# add linux install target
			#set( QMF_GEN ${EXTERNAL_STAGE}/managementgen/qmf-gen ) ?
		endif()

		append_var(CONDOR_EXTERNALS qpid)

	else( NOT PROPER )

		# I don't know how we check versions for native search of qpid-qmf? header check? program_exec?
		find_multiple( "qpidcommon;qpidclient;qmf;qmfengine;qpidtypes" QPID_FOUND)
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
