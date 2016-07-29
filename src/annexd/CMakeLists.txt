if( LIBUUID_FOUND AND HAVE_UUID_H )

	file( GLOB annexdSources *.cpp )
	condor_daemon( annexd
		"${annexdSources}"
		"${CONDOR_LIBS}"
		"${C_SBIN}"
		OFF )

endif( LIBUUID_FOUND AND HAVE_UUID_H )
