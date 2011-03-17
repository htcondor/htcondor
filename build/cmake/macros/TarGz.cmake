if (NOT WINDOWS)
    add_custom_command(
        OUTPUT ${CPACK_PACKAGE_FILE_NAME}.tar.gz
        COMMAND mv ARGS ${CMAKE_INSTALL_PREFIX} ${CPACK_PACKAGE_FILE_NAME}
        COMMAND tar ARGS czf ${CPACK_PACKAGE_FILE_NAME}.tar.gz --owner=0 --group=0 --numeric-owner ${CPACK_PACKAGE_FILE_NAME}
        COMMAND mv ARGS ${CPACK_PACKAGE_FILE_NAME} ${CMAKE_INSTALL_PREFIX}
    )
    add_custom_target(targz DEPENDS ${CPACK_PACKAGE_FILE_NAME}.tar.gz)
endif()