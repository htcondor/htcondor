if( LINUX )

	if( NOT LIBUUID_FOUND )
		message( FATAL_ERROR "annexd requires uuid library" )
	endif( NOT LIBUUID_FOUND )

	if( NOT HAVE_UUID_UUID_H )
		message( FATAL_ERROR "annexd required uuid header" )
	endif( NOT HAVE_UUID_UUID_H )


	set(annexSources
		annex-condor-off.cpp
		annex-condor-status.cpp
		annex.cpp
		annex-create.cpp
		annex-setup.cpp
		annex-status.cpp
		annex-update.cpp
		BulkRequest.cpp
		CheckConnectivity.cpp
		CheckForStack.cpp
		CondorStatusReply.cpp
		CreateKeyPair.cpp
		CreateStack.cpp
		FunctorSequence.cpp
		GenerateConfigFile.cpp
		generate-id.cpp
		GetFunction.cpp
		GetInstances.cpp
		OnDemandRequest.cpp
		PutRule.cpp
		PutTargets.cpp
		ReplyAndClean.cpp
		SetupReply.cpp
		StatusReply.cpp
		UploadFile.cpp
		user-config-dir.cpp
		WaitForStack.cpp
		)

	SET( gahpClientPrefix "${CMAKE_CURRENT_SOURCE_DIR}/../condor_gridmanager" )
	SET( gahpClientSources "${gahpClientPrefix}/generic-gahp-client.cpp;${gahpClientPrefix}/gahp-client.cpp;${gahpClientPrefix}/ec2-gahp-client.cpp;${gahpClientPrefix}/proxymanager.cpp" )

	include_directories( "${gahpClientPrefix}" )
	include_directories( "${HAVE_UUID_UUID_H}" )

	condor_exe( condor_annex
		"annex.cpp;${annexSources};${gahpClientSources}"
		"${C_BIN}"
		"${CONDOR_LIBS};${LIBUUID_FOUND}"
		ON )

endif( LINUX )
