if( LINUX )

	# This depedency is assumed via the dependency on the gahp client.
	if( NOT LIBUUID_FOUND )
		message( FATAL_ERROR "annexd requires uuid library" )
	endif( NOT LIBUUID_FOUND )

	# This depedency is assumed via the dependency on the gahp client.
	if( NOT HAVE_UUID_UUID_H )
		message( FATAL_ERROR "annexd required uuid header" )
	endif( NOT HAVE_UUID_UUID_H )

	SET( annexdSources "annexd.cpp" )
	SET( gahpClientPrefix "${CMAKE_CURRENT_SOURCE_DIR}/../condor_gridmanager" )
	SET( gahpClientSources "${gahpClientPrefix}/generic-gahp-client.cpp;${gahpClientPrefix}/gahp-client.cpp;${gahpClientPrefix}/proxymanager.cpp" )
	include_directories( ${gahpClientPrefix} )
	condor_exe( condor_annexd
		"${annexdSources};${gahpClientSources}"
		"${C_SBIN}"
		"${CONDOR_LIBS}"
		ON )

endif( LINUX )
