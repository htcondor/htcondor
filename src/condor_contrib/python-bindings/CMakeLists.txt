

if (PYTHONLIBS_FOUND)
    option(WITH_PYTHON_BINDINGS "Support for HTCondor python bindings" ON)
endif()

if ( LINUX AND WITH_PYTHON_BINDINGS )
  configure_file (
	"${PROJECT_SOURCE_DIR}/src/condor_contrib/python-bindings/test_driver.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_driver"
  )

  set ( CMAKE_LIBRARY_PATH_ORIG ${CMAKE_LIBRARY_PATH} )
  set ( CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/lib64 )
  set ( CMAKE_LIBRARY_PATH CMAKE_LIBRARY_PATH_ORIG)

  include_directories(${PYTHON_INCLUDE_PATH})

  if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
    set( PYCLASSAD_LIB_NAME "pyclassad_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}_${CMAKE_MATCH_3}" )
  else()
    message(FATAL_ERROR "Can't determine HTCondor version!")
  endif()
  condor_shared_lib( pyclassad classad.cpp classad_wrapper.h exprtree_wrapper.h )
  target_link_libraries( pyclassad classad ${PYTHON_LIBRARIES} -lboost_python )
  set_target_properties( pyclassad PROPERTIES OUTPUT_NAME "${PYCLASSAD_LIB_NAME}" )


  condor_shared_lib( classad_module classad_module.cpp )
  target_link_libraries( classad_module pyclassad -lboost_python ${PYTHON_LIBRARIES} )
  set_target_properties(classad_module PROPERTIES PREFIX "" OUTPUT_NAME classad )

  set_source_files_properties(config.cpp collector.cpp secman.cpp dc_tool.cpp schedd.cpp classad.cpp classad_module.cpp PROPERTIES COMPILE_FLAGS "-Wno-strict-aliasing -Wno-cast-qual -Wno-deprecated")
  condor_shared_lib( htcondor htcondor.cpp collector.cpp config.cpp daemon_and_ad_types.cpp dc_tool.cpp export_headers.h old_boost.h schedd.cpp secman.cpp )
  target_link_libraries( htcondor pyclassad condor_utils -lboost_python ${PYTHON_LIBRARIES} )
  set_target_properties( htcondor PROPERTIES PREFIX "" )

  install ( TARGETS pyclassad DESTINATION ${C_LIB} )
  install ( TARGETS htcondor classad_module DESTINATION ${C_PYTHONARCHLIB} )

  string( COMPARE EQUAL ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} INSOURCEBUILD)
  if (BUILD_TESTING)
    if(NOT INSOURCEBUILD)
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/classad_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/tests/classad_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/classad_tests.py )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_tests.py )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test.old.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.old.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test.old.ad )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test.ad )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/submit.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/submit.ad )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test_driver COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/test_driver ${CMAKE_CURRENT_BINARY_DIR}/test_driver )
    endif()
    add_custom_target ( python_bindings_tests ALL DEPENDS tests/classad_tests.py tests/htcondor_tests.py tests/test.ad tests/test.old.ad tests/submit.ad test_driver )
    enable_testing()
    add_test(python_classad_test test_driver tests/classad_tests.py)
    add_test(python_htcondor_test test_driver tests/htcondor_tests.py TestConfig TestVersion)
  endif (BUILD_TESTING)

endif ( LINUX AND WITH_PYTHON_BINDINGS )

