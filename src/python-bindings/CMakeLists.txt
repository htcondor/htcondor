

if ( WITH_PYTHON_BINDINGS AND PYTHONLIBS_FOUND AND Boost_PYTHON_LIBRARY AND NOT SOLARIS )
  configure_file (
	"${PROJECT_SOURCE_DIR}/src/python-bindings/test_driver.in"
	"${CMAKE_CURRENT_BINARY_DIR}/test_driver"
  )

  set ( CMAKE_LIBRARY_PATH_ORIG ${CMAKE_LIBRARY_PATH} )
  set ( CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /usr/lib64 )
  set ( CMAKE_LIBRARY_PATH CMAKE_LIBRARY_PATH_ORIG)

  include_directories(${PYTHON_INCLUDE_DIRS} ${BOOST_INCLUDE})
  link_directories(${BOOST_LD})

  GET_FILENAME_COMPONENT(PYTHON_LIBRARY_FILENAME ${PYTHON_LIBRARIES} NAME)
  STRING(REGEX REPLACE ".*python([0-9]+[.]?[0-9]+).*" "\\1" _PYTHON_VERSION ${PYTHON_LIBRARY_FILENAME})
  if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
    set( PYCLASSAD_LIB_NAME "pyclassad${_PYTHON_VERSION}_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}_${CMAKE_MATCH_3}" )
  else()
    message(FATAL_ERROR "Can't determine HTCondor version!")
  endif()
  condor_shared_lib( pyclassad classad.cpp classad_wrapper.h exprtree_wrapper.h )
  target_link_libraries( pyclassad ${CLASSADS_FOUND} ${PYTHON_LIBRARIES} -lboost_python )
  set_target_properties( pyclassad PROPERTIES OUTPUT_NAME "${PYCLASSAD_LIB_NAME}" )
  set_target_properties( pyclassad PROPERTIES COMPILE_FLAGS "-fPIC")

  # Note we do not use condor_shared_lib below because we want a separate install target.
  add_library( classad_module SHARED classad_module.cpp )
  target_link_libraries( classad_module pyclassad -lboost_python ${PYTHON_LIBRARIES} )
  set_target_properties(classad_module PROPERTIES PREFIX "" OUTPUT_NAME classad )
  set_target_properties(classad_module PROPERTIES SUFFIX ".so" )

  set_source_files_properties(config.cpp collector.cpp secman.cpp dc_tool.cpp schedd.cpp classad.cpp classad_module.cpp PROPERTIES COMPILE_FLAGS "-Wno-strict-aliasing -Wno-cast-qual -Wno-deprecated")
  add_library( htcondor SHARED htcondor.cpp collector.cpp config.cpp daemon_and_ad_types.cpp dc_tool.cpp export_headers.h old_boost.h schedd.cpp secman.cpp )
  target_link_libraries( htcondor pyclassad condor_utils -lboost_python ${PYTHON_LIBRARIES} )
  set_target_properties( htcondor PROPERTIES PREFIX "" )
  set_target_properties( htcondor PROPERTIES SUFFIX ".so" )

  install ( TARGETS pyclassad DESTINATION ${C_LIB} )
  install ( TARGETS htcondor classad_module DESTINATION ${C_PYTHONARCHLIB} )

  if ( DARWIN )
        add_custom_command( TARGET classad_module htcondor
                POST_BUILD
                COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs classad.so htcondor.so )
        add_custom_command( TARGET pyclassad
                POST_BUILD
                COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs lib${PYCLASSAD_LIB_NAME}.dylib )
  endif()

  string( COMPARE EQUAL ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR} INSOURCEBUILD)
  if (BUILD_TESTING)
    if(NOT INSOURCEBUILD)
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/classad_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/classad_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/classad_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/classad_tests.py )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/htcondor_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/htcondor_tests.py )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update_tests.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update_tests.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update_tests.py ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update_tests.py )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test.old.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.old.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.old.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test.old.ad )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/test.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/test.ad )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_submit.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_submit.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_submit.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_submit.ad )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/submit.ad DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit.ad COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/submit.ad ${CMAKE_CURRENT_BINARY_DIR}/tests/submit.ad )
      add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update.py DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update.py COMMAND ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/tests/delayed_update.py ${CMAKE_CURRENT_BINARY_DIR}/tests/delayed_update.py )
    endif()
    add_custom_target ( python_bindings_tests ALL DEPENDS tests/classad_tests.py tests/htcondor_tests.py tests/delayed_update_tests.py tests/test.ad tests/test.old.ad tests/submit.ad tests/delayed_submit.ad tests/delayed_update.py ${CMAKE_CURRENT_BINARY_DIR}/test_driver )
    enable_testing()
    if ( LINUX )
      add_test(python_classad_test test_driver tests/classad_tests.py)
      add_test(python_htcondor_test test_driver tests/htcondor_tests.py TestConfig TestVersion)
    endif ( LINUX )
  endif (BUILD_TESTING)
endif ( WITH_PYTHON_BINDINGS AND PYTHONLIBS_FOUND AND Boost_PYTHON_LIBRARY AND NOT SOLARIS )
