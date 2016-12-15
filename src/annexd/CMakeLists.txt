if( LINUX )

	if( NOT LIBUUID_FOUND )
		message( FATAL_ERROR "annexd requires uuid library" )
	endif( NOT LIBUUID_FOUND )

	if( NOT HAVE_UUID_UUID_H )
		message( FATAL_ERROR "annexd required uuid header" )
	endif( NOT HAVE_UUID_UUID_H )

	SET( annexdSources "annexd.cpp;BulkRequest.cpp;FunctorSequence.cpp;ReplyAndClean.cpp;PutRule.cpp;PutTargets.cpp;GetFunction.cpp" )
	SET( gahpClientPrefix "${CMAKE_CURRENT_SOURCE_DIR}/../condor_gridmanager" )
	SET( gahpClientSources "${gahpClientPrefix}/generic-gahp-client.cpp;${gahpClientPrefix}/gahp-client.cpp;${gahpClientPrefix}/ec2-gahp-client.cpp;${gahpClientPrefix}/proxymanager.cpp" )
	include_directories( "${gahpClientPrefix}" )
	include_directories( "${HAVE_UUID_UUID_H}" )
	condor_exe( condor_annexd
		"${annexdSources};${gahpClientSources}"
		"${C_SBIN}"
		"${CONDOR_LIBS};${LIBUUID_FOUND}"
		ON )

	condor_exe( condor_annex "annex.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF )
endif( LINUX )
