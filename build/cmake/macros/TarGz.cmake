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
        COMMAND ls ARGS -lR ${CPACK_PACKAGE_FILE_NAME}
        COMMAND ps ARGS auwx
        COMMAND ${TAR_COMMAND} ARGS czf ${CPACK_PACKAGE_FILE_NAME}.tar.gz --owner=0 --group=0 --numeric-owner ${CPACK_PACKAGE_FILE_NAME}
        COMMAND mv ARGS ${CPACK_PACKAGE_FILE_NAME} ${CMAKE_INSTALL_PREFIX}
    )
    add_custom_target(targz DEPENDS ${CPACK_PACKAGE_FILE_NAME}.tar.gz)
endif()
