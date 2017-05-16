if( LINUX )

	if( NOT LIBUUID_FOUND )
		message( FATAL_ERROR "annexd requires uuid library" )
	endif( NOT LIBUUID_FOUND )

	if( NOT HAVE_UUID_UUID_H )
		message( FATAL_ERROR "annexd required uuid header" )
	endif( NOT HAVE_UUID_UUID_H )

	SET( annexSources "GetSFRs.cpp;GetInstances.cpp;StatusReply.cpp;annex-status.cpp;annex-create.cpp;annex-setup.cpp;annex-update.cpp;BulkRequest.cpp;FunctorSequence.cpp;ReplyAndClean.cpp;PutRule.cpp;PutTargets.cpp;GetFunction.cpp;generate-id.cpp;OnDemandRequest.cpp;UploadFile.cpp;CreateStack.cpp;SetupReply.cpp;WaitForStack.cpp;GenerateConfigFile.cpp;CreateKeyPair.cpp;user-config-dir.cpp;CheckForStack.cpp;CheckConnectivity.cpp" )
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
