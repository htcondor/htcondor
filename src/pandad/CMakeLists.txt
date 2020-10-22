# We need Linux-specific linker magic.
if( LINUX AND HAVE_EXT_OPENSSL AND HAVE_EXT_CURL AND HAVE_PTHREADS )

	include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
	include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

	set(SourceFiles 
		main.cpp
		PipeBuffer.cpp
	)

	condor_exe( pandad
				"${SourceFiles}"
				${C_LIBEXEC}
				"${CONDOR_TOOL_LIBS};${CURL_FOUND}"
				OFF )

	condor_plugin( panda-plugin "pandadClassAdLogPlugin.cpp" "${C_LIBEXEC}" "" off )
endif()
