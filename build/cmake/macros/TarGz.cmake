if (NOT WINDOWS)
    if (LINUX)
		set( TAR_COMMAND tar)
    else()
    	# BSD and Solaris have both BSD tar, named "tar", and GNU tar, named "gtar" in ports...

		# Macos is _like_ bsd, but calls it "gnutuar"
		FIND_PROGRAM(TAR_COMMAND NAMES "gtar" "gnutar")

		# but old batlab has gnu tar named "tar" in the path first, and no gtar
		if (NOT TAR_COMMAND)
			# Could not find gtar, use "tar" and hope for the best
			set( TAR_COMMAND tar)
		endif()
    endif()


    add_custom_command(
        OUTPUT ${CPACK_PACKAGE_FILE_NAME}.tar.gz
        COMMAND mv ARGS ${CMAKE_INSTALL_PREFIX} ${CPACK_PACKAGE_FILE_NAME}
	# For reasons we just can't explain, occasionally, on macos only, 
	# tar fails with "file changed as we read it", because the mtime
	# of a .a file moved.  Just retry the tar if it fails for any reason
        COMMAND ${TAR_COMMAND} ARGS czf ${CPACK_PACKAGE_FILE_NAME}.tar.gz --owner=0 --group=0 --numeric-owner ${CPACK_PACKAGE_FILE_NAME} || ${TAR_COMMAND} ARGS czf ${CPACK_PACKAGE_FILE_NAME}.tar.gz --owner=0 --group=0 --numeric-owner ${CPACK_PACKAGE_FILE_NAME}
        COMMAND mv ARGS ${CPACK_PACKAGE_FILE_NAME} ${CMAKE_INSTALL_PREFIX}
    )
    add_custom_target(targz DEPENDS ${CPACK_PACKAGE_FILE_NAME}.tar.gz)
endif()
