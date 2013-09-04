
if ( RPM_SYSTEM_NAME )

        add_custom_target( srpm_dir COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/srpm )

        add_custom_target( rpm_dir COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/rpm )

        add_custom_target( srpm COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/srpm/makesrpm.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/srpm DEPENDS srpm_dir )

        add_custom_target( rpm COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/rpm/makerpm.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/rpm DEPENDS rpm_dir )

endif ( RPM_SYSTEM_NAME )

