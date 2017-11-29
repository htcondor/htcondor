
if ( RPM_SYSTEM_NAME )

        add_custom_target( srpm_dir COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/srpm )

        add_custom_target( rpm_dir COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/rpm )

        add_custom_target( srpm COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/srpm/makesrpm.sh WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/srpm DEPENDS srpm_dir )

        add_custom_target( rpm COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/srpm/makesrpm.sh -ba WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/rpm DEPENDS rpm_dir )

        add_custom_target( srpm_with_std_univ_sources COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/srpm/makesrpm.sh --bundle-std-univ-externals
                           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/srpm DEPENDS srpm_dir )

        add_custom_target( rpm_with_std_univ_sources COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/srpm/makesrpm.sh -ba --bundle-std-univ-externals
                           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/rpm DEPENDS rpm_dir )

        add_custom_target( srpm_with_all_sources COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/srpm/makesrpm.sh --bundle-all-externals
                           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/srpm DEPENDS srpm_dir )

        add_custom_target( rpm_with_all_sources COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/srpm/makesrpm.sh -ba --bundle-all-externals
                           WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/rpm DEPENDS rpm_dir )

endif ( RPM_SYSTEM_NAME )

if ( WANT_PYTHON_WHEELS )
  add_custom_target( pypi_dir COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/pypi )
  add_custom_target( pypi_htcondor_dir DEPENDS pypi_dir COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor )
  add_custom_target( pypi_condor_libs_dir DEPENDS pypi_htcondor_dir COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/condor_libs )
  add_custom_target( pypi_classad_dir DEPENDS pypi_dir COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/pypi/classad )
  add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/condor_libs/lib${PYCLASSAD_LIB_NAME}.so DEPENDS pypi_condor_libs_dir pyclassad ${CMAKE_CURRENT_BINARY_DIR}/../../src/python-bindings/lib${PYCLASSAD_LIB_NAME}.so COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/../../src/python-bindings/lib${PYCLASSAD_LIB_NAME}.so ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/condor_libs/lib${PYCLASSAD_LIB_NAME}.so )
  add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/condor_libs/lib${UTILS_LIB_NAME}.so DEPENDS pypi_condor_libs_dir pyclassad ${CMAKE_CURRENT_BINARY_DIR}/../../src/condor_utils/lib${UTILS_LIB_NAME}.so COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/../../src/condor_utils/lib${UTILS_LIB_NAME}.so ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/condor_libs/lib${UTILS_LIB_NAME}.so )
  add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/condor_libs/libclassad.so.9 DEPENDS pypi_condor_libs_dir classads ${CMAKE_CURRENT_BINARY_DIR}/../../bld_external/classads-${PACKAGE_VERSION}/install/lib/libclassad.so.9 COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/../../bld_external/classads-${PACKAGE_VERSION}/install/lib/libclassad.so.9 ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/condor_libs/libclassad.so.9 )
  add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/_htcondor.so DEPENDS pypi_htcondor_dir _htcondor ${CMAKE_CURRENT_BINARY_DIR}/../../src/python-bindings/_htcondor.so COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/../../src/python-bindings/_htcondor.so ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/_htcondor.so )
  add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/classad/_classad.so DEPENDS pypi_classad_dir _classad_module ${CMAKE_CURRENT_BINARY_DIR}/../../src/python-bindings/_classad.so COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/../../src/python-bindings/_classad.so ${CMAKE_CURRENT_BINARY_DIR}/pypi/classad/_classad.so )
  string( COMPARE EQUAL ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} INSOURCEBUILD)
  if( NOT INSOURCEBUILD )
    add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/__init__.py DEPENDS pypi_htcondor_dir ${CMAKE_CURRENT_SOURCE_DIR}/pypi/htcondor/__init__.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pypi/htcondor/__init__.py ${CMAKE_CURRENT_BINARY_DIR}/pypi/htcondor/__init__.py )
    add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/classad/__init__.py DEPENDS pypi_classad_dir ${CMAKE_CURRENT_SOURCE_DIR}/pypi/classad/__init__.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pypi/classad/__init__.py ${CMAKE_CURRENT_BINARY_DIR}/pypi/classad/__init__.py )
    add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/setup.py DEPENDS pypi_dir ${CMAKE_CURRENT_SOURCE_DIR}/pypi/setup.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pypi/setup.py ${CMAKE_CURRENT_BINARY_DIR}/pypi/setup.py )
    add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/setup.cfg DEPENDS pypi_dir ${CMAKE_CURRENT_SOURCE_DIR}/pypi/setup.cfg COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pypi/setup.cfg ${CMAKE_CURRENT_BINARY_DIR}/pypi/setup.cfg )
    add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pypi/README.rst DEPENDS pypi_dir ${CMAKE_CURRENT_SOURCE_DIR}/pypi/README.rst COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/pypi/README.rst ${CMAKE_CURRENT_BINARY_DIR}/pypi/README.rst )
  endif( NOT INSOURCEBUILD )
  add_custom_target( pypi_staging DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/pypi/setup.py ${CMAKE_CURRENT_BINARY_DIR}/pypi/setup.cfg pypi/README.rst pypi/htcondor/__init__.py pypi/htcondor/_htcondor.so pypi/htcondor/condor_libs/lib${PYCLASSAD_LIB_NAME}.so pypi/htcondor/condor_libs/lib${UTILS_LIB_NAME}.so pypi/htcondor/condor_libs/libclassad.so.9 pypi/classad/__init__.py pypi/classad/_classad.so )

endif ( WANT_PYTHON_WHEELS )
