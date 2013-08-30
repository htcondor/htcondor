
if ( RPM_SYSTEM_NAME )
	SET(SRPM_FILES condor_config.local.dedicated.resource condor.osg-sysconfig condor.service  condor.spec  condor-tmpfiles.conf config_batch_gahp_path.patch  generate-tarball.sh  NOTICE.txt osg_sysconfig_in_init_script.patch makesrpm.sh )
	SET(SRPM_OUTPUT_FILES )

	FOREACH(filename ${SRPM_FILES})
		add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/srpm/${filename} DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/srpm/${filename} COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/srpm/${filename} ${CMAKE_CURRENT_BINARY_DIR}/srpm/${filename} )
		LIST(APPEND SRPM_OUTPUT_FILES ${CMAKE_CURRENT_BINARY_DIR}/srpm/${filename} )
	ENDFOREACH()
	add_custom_target( copy_srpm_sources ALL SOURCES ${SRPM_OUTPUT_FILES} )

	add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rpm/makerpm.sh DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/rpm/makerpm.sh COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/rpm/makerpm.sh ${CMAKE_CURRENT_BINARY_DIR}/rpm/makerpm.sh )

	add_custom_target( srpm DEPENDS copy_srpm_sources COMMAND ${CMAKE_CURRENT_BINARY_DIR}/srpm/makesrpm.sh ${PROJECT_SOURCE_DIR} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/srpm DEPENDS ${SRPM_OUTPUT_FILES} )

	add_custom_target( rpm COMMAND ${CMAKE_CURRENT_BINARY_DIR}/rpm/makerpm.sh ${PROJECT_SOURCE_DIR} DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/rpm/makerpm.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/rpm )

endif ( RPM_SYSTEM_NAME )

